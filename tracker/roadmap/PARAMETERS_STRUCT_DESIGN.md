# Unified Parameters/Arguments Struct Design

**Status:** Proposed — target **v2.0.0** (breaking schema change)
**Drafted:** 2026-08-31

## Overview

Redesign the `parameters` column's element struct so a single shape serves
function/method **definitions**, **declarations**, and **calls**. The current
`STRUCT(name VARCHAR, type VARCHAR)` encodes only a definition's mental model;
call arguments have no reliable "name"/"type", so the extractor jams the
argument *value* into `type`, producing semantically wrong rows like
`calc.add(1, 2)` → `[{name:'', type:1}, {name:'', type:2}]`.

The fix is not a rename — it is **separating `type` (annotation) from `value`
(default / argument expression)** and adding position + an AST anchor.

## Current behavior (the bug)

```sql
-- def add(self, a, b):            -> parameters = [{name:self,type:''},{name:a,type:''},{name:b,type:''}]
-- calc.add(1, 2)  (a CALL node)   -> parameters = [{name:'',type:1},{name:'',type:2}]   -- value in `type`!
-- os.path.join('a','b')           -> parameters = [{name:'',type:'\'a\''},{name:'',type:'\'b\''}]
```

`type` is doing double duty: real type annotation for defs, raw argument value
for calls. Wrong meaning, empty `name`, no position.

## Proposed element struct

```
STRUCT(
  ordinal  INTEGER,   -- 0-based position. ALWAYS set. The only stable identity
                      --   for positional call args (which have no name).
  name     VARCHAR,   -- def: parameter name; call: keyword name;
                      --   NULL for a positional call argument.
  kind     VARCHAR,   -- 'positional' | 'keyword' | 'vararg' | 'kwarg'
                      --   (folds the scattered is_variadic/is_optional bools;
                      --    "has default" becomes simply `value IS NOT NULL`).
  type     VARCHAR,   -- def: type-annotation text; call: NULL.
  value    VARCHAR,   -- def: default value; call: the argument expression text.
                      --   THE bug fix — arguments live here, never in `type`.
  node_id  BIGINT     -- this param/arg's AST node; the single anchor for
                      --   normalization and drill-down (see below).
)
```

### Worked examples

| Source | Element rows |
|---|---|
| `def add(a: int, b: int = 2)` | `{0,'a','positional','int',NULL,n}`, `{1,'b','positional','int','2',n}` |
| `def f(*args, **kw)` | `{0,'args','vararg',NULL,NULL,n}`, `{1,'kw','kwarg',NULL,NULL,n}` |
| `calc.add(1, x=2)` | `{0,NULL,'positional',NULL,'1',n}`, `{1,'x','keyword',NULL,'2',n}` |

`WHERE arg.name IS NULL` cleanly means "positional argument."

## Design decisions & rationale

**Keep `name` and `type`; add `value`.** `name` is genuinely correct for both
(parameter name / keyword). `type` is genuinely a type — NULL for calls is
honest. We deliberately do **not** rename `type` to `annotation`/`label`:
`annotation` already means *decorators* elsewhere in the schema (the
`annotations` column) and reusing it would collide. The real fix is adding
`value`, not renaming.

**One `node_id`, flat — not nested `STRUCT(text, node_id)` per field.** A single
param-level `node_id` already solves normalization: from that node you navigate
the AST to its type child and value child. The string representation lives in
the flat `type`/`value` fields; the node anchor gives you the whole subtree for
canonicalization or analysis. Example — "which arguments are themselves calls":

```sql
SELECT * FROM (SELECT unnest(parameters) AS a FROM read_ast(...) WHERE is_function_call(semantic_type))
WHERE a.node_id IN (SELECT node_id FROM read_ast(...) WHERE is_function_call(semantic_type));
```

Nesting `type`/`value` into `STRUCT(text, node_id)` doubles query depth
(`param.type.text`) and buys node references already reachable from the anchor.
Rejected as over-engineered. (`qualified_name` is a `LIST<STRUCT>` and earns its
nesting; this would not.)

**`kind` folds existing bools.** Extractors already track `is_variadic` /
`is_optional` informally; a `kind` enum captures positional/keyword/vararg/kwarg
uniformly (cf. Python `inspect.Parameter.kind`), and optionality collapses to
`value IS NOT NULL`.

## Migration notes (why v2.0.0)

Breaking: the `parameters` column's element struct type changes. Blast radius:

- **All language extractors (~12)** in `src/include/*_native_extractors.hpp` and
  `src/language_adapters/` — each `ParameterInfo` producer must populate the new
  fields (ordinal, kind, value, node_id) and stop writing values into `type`.
- **`ParameterInfo` struct** in `src/include/ast_type.hpp` and the output-schema
  assembly in `src/unified_ast_backend.cpp` (the `parameters` LIST<STRUCT> build).
- **Every test asserting `parameters`** — audit `test/sql/**` for `parameters`.
- **Docs**: `output-schema.md`, `native-extraction-semantics.md` (the latter
  already flags the `parameters` type ambiguity + the `['']` call placeholder).

Populate incrementally is acceptable *within* the new struct (e.g. `node_id`
may land after `value`), but the struct SHAPE must be introduced all at once —
adding fields later is itself breaking, so define the full shape at the v2.0.0
cut.

## Implementation phases (when scheduled)

1. Extend `ParameterInfo` (ast_type.hpp) with `ordinal`, `kind`, `value`,
   `node_id`; keep `name`, `type`.
2. Rework the output-schema builder to emit the 6-field struct.
3. Migrate extractors language-by-language (start Python/Java/TS — best-covered),
   TDD each: calls put args in `value`, defs put annotations in `type`.
4. Sweep tests to the new shape; update docs.
5. Changelog: breaking-change entry with a before/after example.

## Related

- `tracker/roadmap/VALUE_FIELD_DESIGN.md` — the `value` column for nodes; same
  spirit (separating value from name/type) applied to the params struct.
- `native-extraction-semantics.md` "Known Inconsistency" on call `parameters`.
