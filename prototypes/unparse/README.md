# `ast_unparse` — reconstruct source from the AST (prototype)

A **rules-based** unparser: given a sitting_duck AST, emit source text that
**re-parses to the same tree** ("pseudo-identity"). It never uses `peek` (a
truncated preview — lossy on long tokens) and never replays byte offsets, so it
works on a *transformed* tree, not just a verbatim round-trip.

Status: **prototype**, loadable on top of the extension. Not embedded, not in the
build. This doc's real purpose is the **unparse-rules architecture** (below).

## Run it

```sql
LOAD 'build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
.read prototypes/unparse/ast_unparse.sql
SELECT source FROM ast_unparse('src/**/*.py');
```

## How it works — three layers, all from data the AST already has

| Layer | Rule | Source |
|---|---|---|
| **Leaf text** | `COALESCE(NULLIF(name,''), type)` | anonymous tokens have `type` == the literal (`def`, `(`, `+`); named leaves carry text in `name` when `name_strategy = NODE_TEXT` |
| **Spacing** | whitespace by default; drop the inter-token space iff `left.tight_after OR right.tight_before` (the OR rule) | the `spacing` unparse-rules table |
| **Layout** | newline when a leaf starts below where the previous *ended* (blank lines preserved); indent = count of enclosing **indent-block** tags | the `indent_blocks` unparse-rules table + the contiguous-descendant-range invariant (`A` ancestor of `L` iff `A.id ≤ L.id ≤ A.id + A.descendant_count`) — no recursion |

Whitespace/newlines are **not nodes** (tree-sitter emits them as gaps), so layout
is *inferred* from positions. That's why the bar is structural pseudo-identity,
not byte-exactness. Verified `parse → unparse → re-parse` == same tree on
Python (incl. nested blocks + a trailing statement), JS, C, Go.

## Architecture: unparse rules are separate from semantics — NOT flags

**Presentation is not semantics.** Node-type semantic flags (`IS_SYNTAX_ONLY`,
`IS_SCOPE`, `IS_CONSTITUENT`, `NAME_ROLE`, …) exist to interpret generalized
*meaning*; how a language *renders* is a different concern and does **not** belong
in the flag byte. So the unparse rules live in their own **per-language,
tag-keyed lookup**, kept entirely out of `.def` semantics:

```sql
-- (1) which tags are indent-defining blocks (nesting drives indentation)
indent_blocks(language, tag) = { ('python','block'), ('javascript','statement_block'),
                                 ('c','compound_statement'), ('go','block'), ... }

-- (2) tight-spacing tokens; language '*' = universal; whitespace is the default,
--     a token opts out before/after itself, and the OR rule drops the space.
spacing(language, tag, tight_before, tight_after) = {
   ('*', ',', true, false), ('*', ')', true, false), ('*', '(', false, true), ... }
```

### Why a lookup table, not `IS_SCOPE` and not a new flag
- **`IS_SCOPE` does not track indentation.** Measured: it sits on `module` and
  `function_definition`, *not* on the `block`/suite, and `if_statement` (which
  indents) creates no scope. `return a + b` nests inside **two** `block`s (indent
  2) but only **one** `is_scope` node. So reading `is_scope` would mis-indent —
  indentation genuinely needs the explicit indent-block tag list.
- **No new presentation flag.** The flag byte is a scarce semantic resource
  (1 bit left); adding `IS_INDENT_BLOCK`/tight-spacing flags would put rendering
  concerns into the semantics layer. The lookup table is the right home and needs
  no C++/`.def` change at all.

### Where the rules would live in production
A per-language unparse-rules registry beside the grammar configs — e.g. a data
file (`unparse_rules/<lang>`) or a small generated table — read by an embedded
`ast_unparse`. Same shape as the two CTEs above; the prototype just inlines them.

### Leaf text: the one thing that reuses an existing field
Recoverable leaf text needs no new mechanism — it's `name` (via
`name_strategy = NODE_TEXT`) or the anonymous token's `type`. The only real gap
is text-bearing *named* leaves whose strategy is `NONE` (e.g. `comment` today
loses its text): giving them `NODE_TEXT` fixes unparse. That is a legitimate
semantic decision about the `name` field, not a presentation rule, so it stays in
`.def`. (Trade-off: `NODE_TEXT` also populates `name` for those nodes; acceptable,
or unparse gets a text channel separate from `name` if that pollutes name queries.)

## Limits / next steps
- Byte-exact (vs. pseudo-identity) needs `source:='full'` columns to size exact
  inter-token whitespace; the layout rule only *places* breaks.
- `f (` vs `f(`: keyword-vs-identifier before `(` — a single universal rule gets
  one wrong; a per-language `spacing` row (or deriving from `IS_SYNTAX_ONLY` of
  the left token) resolves it. Left as prototype nuance.
- `interpolation`/`template_substitution` keep `LITERAL_STRING` but contain code;
  round-trips fine, code-aware inner unparse is future.
- Not wired into the build; promotion = embed `ast_unparse` + move the two lookup
  tables into a per-language unparse-rules registry.
