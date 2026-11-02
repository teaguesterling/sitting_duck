# Module ABI

A language module is the complete implementation of one language: a grammar, a
semantic mapping, and the native extraction that fills the signature columns.
This page is the contract a module must satisfy. The machine-checkable half is
[`spec/abi/module-abi.yaml`](https://github.com/teaguesterling/sitting_duck/blob/main/spec/abi/module-abi.yaml),
and `scripts/verify_module_abi.py` gates the sources against it.

**ABI version: 1.0.0.** It is versioned apart from the extension version. A
mechanism for a module to *declare* the range it builds against is M2 intent,
not something that exists today — this version is a statement about the
interface, and the gate is what keeps the statement true.

The contract has three layers. They are independent. A module can satisfy one
and not the others, and only the third is C++.

## Layer 1 — the binary contract

This is what crosses a shared-library boundary. It is deliberately one symbol:

```c
const TSLanguage *tree_sitter_<language>(void);
```

`register_language()` is a table function. It resolves the symbol you name,
calls it, and checks the grammar's own version:

```sql
SET sitting_duck_enable_runtime_grammars = true;
SELECT * FROM register_language('mylang', '/path/libtree-sitter-mylang.so',
                                symbol := 'tree_sitter_mylang',
                                config := 'mylang_types.json',
                                extensions := ['mylang'],
                                aliases := ['ml']);
```

`ts_language_version()` must fall within
`TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION..TREE_SITTER_LANGUAGE_VERSION`, and
a grammar outside that range is refused at registration rather than mid-query.
Registration also smoke-parses the empty string, so a library that loads but
cannot parse fails before any user query reaches it.

**No C++ type crosses this boundary.** The methods in layer 3 pass `string`,
`vector`, `unordered_map` and `unique_ptr`, none of which has a stable layout
across separately compiled binaries. That is why layer 3 is not part of the
binary contract, and why semantics arrive as data instead — layer 2.

Adding a second required symbol is a MAJOR bump.

## Layer 2 — the data contract

An out-of-tree module declares its semantics as JSON, not as C++. This is the
layer third-party modules actually use: `DynamicLanguageAdapter` implements the
whole of layer 3 from it, so a module needs no compiled adapter at all.

The config is `{"language": …, "node_types": {…}}`, and each node type carries:

| field | meaning |
|---|---|
| `semantic_type` | the universal type, from the taxonomy |
| `refinement` | integer refinement within that type |
| `name_strategy` | how to find this node's name |
| `flags` | array of flag names (`NAME_DEFINITION`, `IS_SCOPE`, …) |

```json
{ "language": "jsond",
  "node_types": {
    "pair": { "semantic_type": "DEFINITION_VARIABLE",
              "name_strategy": "FIRST_CHILD",
              "flags": ["NAME_DEFINITION"] } } }
```

The names those fields may use are **not free text**. They are defined in
[`spec/taxonomy/taxonomy.yaml`](https://github.com/teaguesterling/sitting_duck/blob/main/spec/taxonomy/taxonomy.yaml),
the C++ tables are generated from it, and a config naming something it does not
define is **refused** rather than silently dropped — `Unknown semantic_type`,
`Unknown name_strategy`, `Unknown flag`, each naming the node type.

**Native extraction is not in this layer, deliberately.** A runtime config that
sets `native_strategy` is refused outright, rather than accepted and ignored,
so a config author cannot be left believing native context extraction is
active when it is not. `CUSTOM` is refused for the same reason: it needs native
C++ logic that cannot be loaded at runtime. An out-of-tree module therefore gets
semantic types, names and flags, and the signature columns stay empty until the
module is compiled in.

This layer is additive only. Removing a field, or renaming a strategy, is a
MAJOR bump.

## Layer 3 — the source contract

A compiled-in module implements `LanguageAdapter`. Ten methods are pure virtual,
so the compiler enforces them:

`GetLanguageName`, `GetAliases`, `GetParsingFunction`, `GetNormalizedType`,
`ExtractNodeName`, `ExtractNodeValue`, `IsPublicNode`, `GetNodeConfigs`,
`InitializeParser`, `CreateFreshParser`.

`GetAnonNodeConfigs` has a default that returns an empty map. A module needs it
only when its grammar has a named rule and an anonymous token with the same
`raw_type` — ruby's `if` is both the if-statement and the bare keyword (#215).

Adding a pure virtual is a MAJOR bump. Adding a defaulted virtual is MINOR.

### The half the compiler does not enforce

Native context extraction is declared by **template specialization**, not by an
override. A module declares it in three places:

```cpp
// 1. the extractor itself, in <lang>_native_extractors.hpp
template <NativeExtractionStrategy Strategy> struct MyLangNativeExtractor;

// 2. the mapping from adapter to extractor
template <> struct NativeExtractionTraits<MyLangAdapter> {
    template <NativeExtractionStrategy Strategy>
    using ExtractorType = MyLangNativeExtractor<Strategy>;
};

// 3. optional: bare name-binding rules (#64), both defaulting to false
template <> inline bool IsBareNameDefinitionParent<MyLangAdapter>(const char *parent_type);
template <> inline bool IsBareNameDefinitionField<MyLangAdapter>(TSNode, TSNode, const char *);
```

**A missing mapping is not a compile error.** `ExtractNativeContextTemplated`
falls back to the base `NativeExtractor<Strategy>`, which returns an empty
`NativeContext`. The `.def` file still declares the strategy, the query still
runs, and the `parameters` column simply comes back empty.

That is #220: kotlin, lua and sql each declared function-shaped strategies with
no extractor, in three languages, with no compile error, link error or warning.
It is also why the gate exists — and why the spec lists, under
`native_extraction.unmapped`, every language that is in that condition today.

Two strategy rules follow from the same mechanism:

- A strategy the dispatcher has **no case for** reaches `default:` and returns
  an empty context. The spec lists those under `reserved`; declaring one in a
  `.def` fails the gate.
- A strategy under `dispatched` must have a case. The gate compares both ways,
  so a stale spec entry cannot make its own check pass.

## Registration

Four doors reach one registry, `LanguageAdapterRegistry`:

| door | mechanism | layers used |
|---|---|---|
| compiled-in | `module.cmake` composition, per-language since #92 | 2 + 3 |
| runtime `.so` | `register_language()` (#80) | 1 + 2 |
| pack extension | `INSTALL sitting_duck_<lang>` | 1 + 2 |
| WASM engine | opt-in engine module, v2.x | 1 + 2 |

Only the compiled-in door uses layer 3.

## Conformance

A module either passes the conformance kit or it is not a module:

```bash
bash scripts/conformance_kit.sh --binary build/release/duckdb
```

The kit judges each language against the capabilities it **declares** — call
nodes are named, modifiers and signatures populate, raw-AST-join agreement, no
silent-empty. It exits 3 if any declared capability is undelivered and 4 if one
of its own negative controls stops firing, so a run that cannot fail is reported
rather than counted.

It takes an out-of-tree module through `--extension`, `--def-source` and
`--corpus`, but **one axis is still in-tree only**: the native-context checks
are derived from `DEF_TYPE` tables in this repository, because
`native_strategy` is not surfaced by `ast_type_map()`. Exposing it there is what
would make the kit fully out-of-tree-capable on that axis. The kit's own header
says so, and the limit matters less than it reads: a runtime config cannot
declare native extraction at all (see layer 2).

## Trust boundary

`register_language()` executes native code, so the boundary is stated plainly:
the gate controls **whether** and **from where**, never **what**.

- **Whether**: `sitting_duck_enable_runtime_grammars` is **default-off**. With
  it off, the function refuses.
- **From where**: the library path must satisfy `allowed_directories` /
  `enable_external_access`, checked before any registry mutation, so a refused
  path leaves previously registered languages untouched.
- **What**: nothing. No pre-execution check can vet a `.so`. This is identical
  to DuckDB's own unsigned-`LOAD` model.

Reaching the function at all requires session `SET` privilege — a caller who
could already `LOAD` an extension — so there is **no privilege escalation**.

This is a settled decision, and additional gates are deliberately absent: they
add friction rather than safety. Signing is the only lever that would add real
assurance, and tree-sitter grammars cannot realistically be signed, which is
why this is a dedicated setting rather than `allow_unsigned_extensions`.

The loaded library is never closed. Adapters and in-flight parses hold pointers
into the loaded `TSLanguage`, so unloading would crash the process.

## Shared conventions

A module that breaks one of these produces plausible, wrong output rather than
an error, so they are part of the contract:

- **Annotations carry their `@`.** Decorator and annotation text is always
  `@`-prefixed, so `SplitAnnotations` can separate them from keyword modifiers.
- **Byte offsets are 0-based and half-open**, under `source := 'full'`.

## Related

- [`tracker/features/044-v2-m1-contract.md`](https://github.com/teaguesterling/sitting_duck/blob/main/tracker/features/044-v2-m1-contract.md) — the M1 milestone this contract closes
- [`docs/planning/v2-architecture.md`](https://github.com/teaguesterling/sitting_duck/blob/main/docs/planning/v2-architecture.md) — where the module layout is going (M2, M3)
- [`register_language()`](functions.md#register_language) — the full parameter reference for the runtime door described in layer 1
- [Semantic Types](semantic-types.md) — the taxonomy the data layer names
