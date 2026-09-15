# `ast_unparse` — reconstruct source from the AST (prototype)

A **rules-based** unparser: given a sitting_duck AST, emit source text that
**re-parses to the same tree** ("pseudo-identity"). It never uses `peek` (a
truncated preview — lossy on long tokens) and never replays byte offsets, so it
works on a *transformed* tree, not just a verbatim round-trip.

Status: **prototype**, loadable on top of the extension. Not embedded, not in the
build. This doc's real purpose is the **`.def` integration plan** (last section).

## Run it

```sql
LOAD 'build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
.read prototypes/unparse/ast_unparse.sql
SELECT source FROM ast_unparse('src/**/*.py');
```

## How it works — three rule layers, all from data the AST already has

| Layer | Rule | Source of truth |
|---|---|---|
| **Leaf text** | `COALESCE(NULLIF(name,''), type)` | anonymous tokens have `type` == the literal (`def`, `(`, `+`); named leaves carry text in `name` when `name_strategy = NODE_TEXT` |
| **Spacing** | no space before `,;:)]}`, none after `([{.` | universal code conventions (token = type for anonymous punctuation) |
| **Layout** | newline when a leaf starts below where the previous one *ended* (blank lines preserved); indent = number of enclosing **block** nodes | line numbers + the contiguous-descendant-range invariant (`A` is an ancestor of `L` iff `A.id ≤ L.id ≤ A.id + A.descendant_count`) — **no recursion** |

Whitespace/newlines are **not nodes** (tree-sitter emits them as gaps), so layout
is *inferred* from positions, never read off a token. That's why the bar is
structural pseudo-identity, not byte-exactness.

## Pseudo-identity (verified: `parse → unparse → re-parse` == same tree)

| language | result |
|---|---|
| Python (nested `if` in `def` + trailing stmt) | ✓ (indentation keeps the trailing stmt out of the block) |
| JavaScript, C, Go | ✓ |

Example — `def f(a, b):\n    if a:\n        return a + b\n    return b` unparses to
the same (only cosmetic `f (`) and re-parses identically.

---

## Integrating the idioms into `.def` files

This is the part worth deciding. A `.def` entry today is:

```c
DEF_TYPE("node_type", SEMANTIC_TYPE | refinements, name_strategy, native_strategy, flags)
```

Each unparse idiom maps onto that line — two reuse existing fields, one wants a
new flag.

### 1. Leaf text → reuse `name_strategy` (no new syntax)

A leaf's text is recoverable iff it's an **anonymous token** (`type` == text, free)
or a **named leaf with `NODE_TEXT`** (its `name` holds the text). So the "unparse
audit" is just: *every text-bearing named leaf needs `NODE_TEXT`.*

Confirmed gap — `comment` loses its text today:

```c
// before (python_types.def): name is empty -> unparse would emit the literal "comment"
DEF_TYPE("comment", METADATA_COMMENT, NONE, NONE, 0)
// after: name = the comment text, so unparse reproduces it
DEF_TYPE("comment", METADATA_COMMENT, NODE_TEXT, NONE, 0)
```

Audit targets: `comment`, and any numeric/`*_content`/operator-token leaf whose
strategy is `NONE`. (Trade-off: `NODE_TEXT` also populates `name` for these nodes,
which is mostly harmless but does put comment text in the `name` column. If that's
undesirable, unparse needs a text channel separate from `name` — a bigger change;
for the prototype, reusing `name` is the pragmatic call.)

### 2. Indentation block → a new flag `IS_INDENT_BLOCK` (bit 6)  ← recommended

Indentation nesting is the one genuinely per-language input. Today the prototype
hardcodes a `language → block_type` map (`block`, `statement_block`,
`compound_statement`, …). The `.def`-native form is a flag on the block node,
parallel to `IS_SCOPE` / `IS_CONSTITUENT`:

```c
// node_config.hpp — bit 6 (0x40), next reserved bit after IS_CONSTITUENT (0x20)
constexpr uint8_t IS_INDENT_BLOCK = 0x40;

// python_types.def
DEF_TYPE("block", ORGANIZATION_BLOCK, NONE, NONE, ASTNodeFlags::IS_INDENT_BLOCK)
// javascript_types.def
DEF_TYPE("statement_block", ORGANIZATION_BLOCK, NONE, NONE, ASTNodeFlags::IS_INDENT_BLOCK)
// c_types.def
DEF_TYPE("compound_statement", ORGANIZATION_BLOCK, NONE, NONE, ASTNodeFlags::IS_INDENT_BLOCK)
```

Then unparse drops the map and becomes **language-agnostic**:

```sql
-- prototype:                       -> production:
blocks AS (SELECT ... FROM ast a    blocks AS (SELECT file_path, node_id, descendant_count
  JOIN block_type_map m ON ...)                FROM read_ast(path) WHERE is_indent_block(flags))
```

Why a flag beats the map or a per-language directive: it lives exactly where the
per-language fact already lives (on the node), reuses the existing flag +
predicate machinery (`is_indent_block(flags)`, and the `flags` string array), and
needs no new `.def` macro. `IS_SCOPE` is *close* but wrong here — scopes attach to
`function_definition`/`class` (the header), while indentation attaches to the
`block`/suite; brace languages want the flag on the `{}` compound, not the def.

### 3. Spacing → universal defaults now, per-node override later

The tight-token rules (`,;:)]}` no-space-before; `([{.` no-space-after) are shared
across C-family + Python + JS, so they live in the macro as defaults — **no `.def`
change needed** for the common case. The residual nuance is keyword-vs-identifier
before `(` (`if (x)` keeps the space, `f(x)` doesn't); a bare default gets one of
them wrong. Options when we want it exact, cheapest first:
- derive it: no-space-before-`(` when the preceding leaf is **not** `IS_SYNTAX_ONLY`
  (i.e. an identifier/call target), keep the space after a keyword — no `.def` change;
- a per-language override set of tight tokens, as a small directive if a language
  genuinely diverges (rare).

## Summary of the `.def` surface

| idiom | `.def` change | mechanism |
|---|---|---|
| leaf text | `name_strategy = NODE_TEXT` on text-bearing named leaves (audit) | **existing** field |
| indentation | `IS_INDENT_BLOCK` on the block node | **one new flag bit** (bit 6) + `is_indent_block()` predicate |
| spacing | none for the common case | macro defaults; optional derived/override later |

So the net new machinery to make unparse fully `.def`-driven is **one flag bit +
one predicate** (mirroring the `IS_CONSTITUENT` work), plus a `NODE_TEXT` audit on
comment/content leaves. Everything else the AST already encodes.

## Limits / next steps
- Byte-exact (not just pseudo-identity) needs `source:='full'` columns to size the
  exact inter-token whitespace; the layout rule above only *places* breaks.
- `interpolation`/`template_substitution` keep `LITERAL_STRING` but contain code;
  round-trip is fine, but a code-aware unparse of their inner expressions is future.
- Not wired into the build; if promoted, embed as `ast_unparse` and add the flag.
