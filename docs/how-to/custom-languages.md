# Custom languages: build one and load it

Sitting Duck ships 28 languages compiled in. You can add a 29th at runtime,
without rebuilding the extension, by compiling a tree-sitter grammar to a shared
library and registering it.

**This is not the same job as [Adding Languages](../development/adding-languages.md).**
That page is for compiling a language *into* the extension — a grammar submodule,
a C++ adapter, `cmake/BuiltinLanguages.cmake`. This page is for a loadable
grammar you build and keep outside the repository.

The contract both ends must satisfy is the [Module ABI](../reference/module-abi.md).
Its layer 1 is your shared library; its layer 2 is your config file.

## 1. Compile the grammar to a shared library

A tree-sitter grammar's generated parser is plain C with no dependencies, so the
build is one `cc` invocation. Starting from a grammar repository:

```bash
# Generate src/parser.c from grammar.js (needs the tree-sitter CLI).
# Skip this if the repository already ships a generated src/parser.c.
tree-sitter generate

# Compile. -I src is required: parser.c includes "tree_sitter/parser.h"
# from the grammar's own src/tree_sitter/ directory.
cc -shared -fPIC -O2 -I src src/parser.c -o libmylang.so
```

**If `src/scanner.c` exists, you must compile it too.** A grammar with an
external scanner has `parser.c` calling `tree_sitter_<name>_external_scanner_*`
functions that live in `scanner.c`; leave it out and the library builds but
`register_language()` fails to resolve the symbol at load time. This is the
common case, not the exception — **17 of the 28 grammars in this repository have
one**:

```bash
cc -shared -fPIC -O2 -I src src/parser.c src/scanner.c -o libmylang.so
```

A C++ scanner (`scanner.cc`) needs `c++` instead of `cc`, and the result still
loads the same way.

**Name the file `.so` on every platform**, macOS and Windows included.
`dlopen()` ignores the suffix, and a uniform name keeps scripts and tests
portable. The in-repo recipe does exactly this — see `test-grammar-lib` in the
`Makefile`, which builds the library the dynamic-language tests use:

```bash
cc -shared -fPIC -O2 \
   -I generated_parsers/tree-sitter-json/src \
   generated_parsers/tree-sitter-json/src/parser.c \
   -o build/test_grammars/libjson_dyn.so
```

### The symbol

`register_language()` resolves one exported C function:

```c
const TSLanguage *tree_sitter_<grammar_name>(void);
```

The name follows the **grammar**, not the name you register it under. The
repository's own test registers `tree-sitter-json` as the language `jsond` while
passing `symbol := 'tree_sitter_json'`. If you do not pass `symbol`, the default
is derived from the language name, which is why a mismatch needs it spelled out.

Registration also checks `ts_language_version()` against the range this build
supports, and smoke-parses the empty string — so a library that loads but cannot
parse fails at registration rather than inside your first real query.

## 2. Write the config

Without a config the grammar parses and every node is untyped. The config maps
node types onto the semantic taxonomy. It is a JSON file (or a JSON string passed
inline):

```json
{
  "language": "mylang",
  "node_types": {
    "document": {
      "semantic_type": "DEFINITION_MODULE",
      "flags": ["IS_SCOPE"]
    },
    "pair": {
      "semantic_type": "DEFINITION_VARIABLE",
      "name_strategy": "FIRST_CHILD",
      "flags": ["NAME_DEFINITION"]
    },
    "string": {
      "semantic_type": "LITERAL_STRING",
      "name_strategy": "NODE_TEXT"
    }
  }
}
```

Each node type takes `semantic_type` (required), and optionally `refinement`,
`name_strategy` and `flags`. The names are **not free text** — they come from
[`spec/taxonomy/taxonomy.yaml`](https://github.com/teaguesterling/sitting_duck/blob/main/spec/taxonomy/taxonomy.yaml),
and an unknown one is refused rather than ignored. See
[Semantic Types](../reference/semantic-types.md) for the vocabulary.

`test/data/dynamic_grammars/jsond_types.json` in this repository is a complete
worked example.

## 3. Enable the gate and register

Loading a grammar executes native code, so it is behind a default-off setting:

```sql
SET sitting_duck_enable_runtime_grammars = true;

SELECT * FROM register_language(
    'mylang',
    '/path/to/libmylang.so',
    symbol := 'tree_sitter_mylang',
    config := '/path/to/mylang_types.json',
    extensions := ['mylang', 'ml'],
    aliases := ['my-lang']
);
```

The library path must also satisfy `allowed_directories` when
`enable_external_access` is off. The check happens **before** any registry
mutation, so a refused path leaves previously registered languages untouched.
The full parameter list is in [`register_language()`](../reference/functions.md#register_language),
and the security model is in the [Module ABI](../reference/module-abi.md#trust-boundary).

## 4. Parse with it

From here it behaves like a built-in language:

```sql
SELECT type, name, start_line
FROM read_ast('example.mylang');

SELECT type, name FROM parse_ast('{"a": 1}', 'mylang');
```

## What you do not get

**Native context extraction is not available to a runtime-registered language,
and the config cannot ask for it.** A config that sets `native_strategy` is
*refused*, not ignored — deliberately, so that you are never left believing
extraction is active when it is not. `CUSTOM` is refused for the same reason: it
needs compiled-in C++.

In practice: `semantic_type`, `name`, and `flags` work. The `parameters`,
`signature_type` and `modifiers` columns stay empty. Getting those means
compiling the language in, which is [Adding Languages](../development/adding-languages.md).

## When it fails

Every rejection names the thing it rejected, so the message is usually the whole
diagnosis:

| message | cause |
|---|---|
| `Failed to parse language config JSON` | the config is not valid JSON |
| `missing the required "node_types" object` | top level needs `language` and `node_types` |
| `Unknown top-level key "…"` | a typo at the top level, rather than silent acceptance |
| `declares language "x" but is being registered as "y"` | the config's `language` must match the name you register |
| `missing the required "semantic_type" field` | every node type needs one |
| `Unknown semantic_type "…" for node type "…"` | not in the taxonomy |
| `Unknown name_strategy "…"` | not a known strategy |
| `Incompatible language version for …` | the grammar's ABI version is outside this build's range; regenerate with a matching tree-sitter CLI |
| a symbol resolution failure at load | usually `scanner.c` was not compiled in, or `symbol` does not match the grammar |

`test/sql/dynamic_languages/` holds the end-to-end tests for all of this —
`register_language_config_errors.test` for the table above, and
`register_language_external_access.test` for the trust gate.

## See also

- [Module ABI](../reference/module-abi.md) — the contract, all three layers
- [`register_language()`](../reference/functions.md#register_language) — the full parameter reference
- [Semantic Types](../reference/semantic-types.md) — the taxonomy your config draws on
- [Adding Languages](../development/adding-languages.md) — compiling a language in, for full native extraction
