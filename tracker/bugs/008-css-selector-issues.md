# Bug: CSS Selector Issues Discovered During Test Coverage Expansion

**Status:** Open
**Priority:** Medium-High
**Discovered:** 2026-04-13

## Description

While writing comprehensive cross-language CSS selector test coverage, several
selector bugs were identified. The new test file
`test/sql/css_selectors_multilang.test` documents expected vs actual behavior
and passes against current (buggy) implementation so the regressions are
locked in. The underlying bugs are listed below.

## Bug 1: `type:has(.class)` returns 0 results — **FIXED 2026-04-13**

When the base selector is a type (tag_name) and `:has()` uses a semantic-class
argument, the filter produced no matches.

### Root cause

`simple_class_candidates` in `css_selectors.sql` extracted class_names from
anywhere in the selector AST, including inside `:has(...)` / `:not(...)` args
— unlike `simple_type_candidates` and `simple_id_candidates`, which anti-join
against `sel_arg_blocks`. So for `function_definition:has(.call)` the CSS
parser wraps the whole expression as `pseudo_class_selector(...)`, the root
hits the `pseudo_class_selector` CASE branch which enforces `class_filter`,
and `class_filter` leaked from `.call` inside `:has()`. No `function_definition`
node is semantically `.call`, so the base filter matched zero rows.

### Fix

Added the same `LEFT JOIN sel_arg_blocks ... WHERE a.node_id IS NULL`
anti-join to `simple_class_candidates`, mirroring the pattern already used
by the type and id candidate CTEs.

## Bug 2: `:not(:has(.class))` also broken — **FIXED 2026-04-13**

Related but separate cause from Bug 1. `not_has_conditions` only extracted
`not_has_type` and `not_has_name` — it had no `not_has_class` column. So
`.call` inside `:not(:has(.call))` was silently dropped, the inner EXISTS
matched any descendant (`NULL OR ...` → true), and the outer `NOT EXISTS`
failed for every candidate with any children.

### Fix

Added `not_has_class` (via `sel_pcs_first_class_name`) to `not_has_conditions`
and wired the corresponding `is_semantic_type(d.semantic_type, UPPER(...))`
check into the `:not(:has())` filter, mirroring the regular `:has()` filter.

## Bug 3: Combinators combined with pseudo-classes on the right side — **FIXED 2026-04-13**

When a combinator is followed by a selector that has a pseudo-class, the CSS
parser wraps the whole thing as `pseudo_class_selector(combinator_selector, ...)`.
The selector engine treated the root as `pseudo_class_selector` and applied
only the pseudo-class filters, ignoring the combinator structure entirely —
silently returning a superset of correct results.

### Fix

Added `sel_pseudo_class_unwrap` CTE analogous to `sel_pseudo_element_unwrap`:
when `sel_root_raw` is a `pseudo_class_selector` with a direct combinator
child (`child_selector`, `descendant_selector`, `sibling_selector`,
`adjacent_sibling_selector`), re-root to that combinator child. The existing
`:has` / `:not` / pseudo-class filters still apply because they iterate over
all sel nodes, not just sel_root.

Results after fix (example):
```sql
-- Correctly returns Cat, Dog (classes that follow another class AND have speak)
SELECT name FROM ast_select('test/data/python/css_selectors_test.py',
    'class_definition ~ class_definition:has(function_definition#speak)');

-- Correctly returns all class methods with return statements
SELECT name FROM ast_select('test/data/javascript/css_selectors_test.js',
    'class_body > method_definition:has(return_statement)');
```

### Known limitation — **RESOLVED 2026-04-14**

Nested pseudo-class wraps like `A > B:has(X):not(Y)` (parsed as
`pseudo_class_selector(pseudo_class_selector(child_selector, :has), :not)`)
now unwrap correctly. The old version only looked at direct children of
sel_root_raw; the new version searches the whole sel_root subtree for a
combinator outside any arg blocks, picking the shallowest. The anti-join
against `sel_arg_blocks` avoids re-rooting into combinators that live
inside `:has(...)` or `:not(...)` arguments (which are descendant filters,
not the base selector).

## Bug 4: `ast_select_from` defined in SQL but missing from embedded macros — **FIXED 2026-04-13**

`ast_select_from` was defined in `src/sql_macros/css_selectors.sql`
(commit e0902f7) but was never embedded into `src/include/embedded_sql_macros.hpp`.
Re-running `python3 scripts/embed_sql_macros.py` to pick it up revealed a
bug left over from the refactor:

### Root cause

Line 624 of `css_selectors.sql` (inside `ast_select_from`) referenced a bare
`language` identifier in `COALESCE(language, 'python')`. In the pre-refactor
form this was inside `ast_select`, where `language` was a macro parameter.
After commit e0902f7 split the body into `ast_select_from` (which has no
`language` parameter), the reference was left behind. DuckDB's binder
tried to resolve `language` as a column reference and failed with
"Referenced column 'language' was not found".

Because this reference sits inside an always-bound CTE (`ast_pattern`),
the binder fails for EVERY call to `ast_select_from` / `ast_select`,
not just ones that use `:match()` / `:contains()`.

### Fix

Pull the language from the AST table itself:
```sql
COALESCE((SELECT language FROM ast WHERE language IS NOT NULL LIMIT 1), 'python')
```

This works because the `ast` CTE is always in scope (it's the parsed AST
table), and every node has a `language` column. For mixed-language globs
this arbitrarily picks one — acceptable since :match patterns already
assume a single grammar.

### Secondary concern (CTE + query_table in ast_select wrapper)

I had worried that the `ast_select` wrapper using `query_table('__ast_src')`
on a CTE name would fail. Testing confirms it DOES work — DuckDB resolves
CTEs through `query_table` correctly. No separate fix needed.

### Test coverage

`test/sql/css_selectors_multilang.test` section 17 restores the
`ast_select_from` tests across type/id/class selectors, all four combinators,
:has / :not / :has(.class), compound selectors, and multi-language usage.

## Bug 5: Compound filter leaks across root selector types — **FIXED 2026-04-14**

The simple-selector CASE dispatch applied a different subset of filters
depending on which selector node type the CSS parser chose as the root:

| Root type                  | type_filter | name_filter | class_filter |
|----------------------------|-------------|-------------|--------------|
| `tag_name` (before fix)    | ✓           | ✓           | ✗            |
| `id_selector` (before fix) | ✓           | ✓           | ✗            |
| `class_selector` (before)  | ✗           | ✗           | ✓            |
| `attribute_selector` (bef) | ✓           | ✗           | ✗            |

Consequence: `.func#ExecuteRecursivePipelines` (parses root =
`id_selector`) ignored the `.func` class filter and matched every node
named "ExecuteRecursivePipelines" — including call_expression,
identifiers (two per definition), and the function_definition itself.

Fix: apply all three filters uniformly from every root type. After the
fix, `.func#main` returns only the single `function_definition` named
`main`, and `function_definition.class#main` correctly returns zero
(because the node isn't semantically a class).

## Bug 6: Combinator EXISTS subqueries decorrelated into always-on joins — **FIXED 2026-04-14**

Even for selectors without any combinator, DuckDB's optimizer decorrelated
the EXISTS subqueries inside the combinator CASE branches into hash joins
over the full AST. On large codebases (e.g., DuckDB's own source tree, ~2M
AST nodes) this produced a 28.8M-row HASH_JOIN and pushed simple queries
to 20+ seconds.

EXPLAIN ANALYZE showed the 28M join came from the descendant_selector
branch — `file_path = file_path AND node_id < node_id AND (node_id +
descendant_count) >= node_id` — fired regardless of `sp.sel_type`.

Fix: replace the single CASE dispatch with a UNION ALL of per-sel_type
sub-queries, each gated by `sp.sel_type = 'X'` at the FROM-WHERE level.
When the selector is, say, `.func#name`, the combinator branches have a
provably-false equality and produce zero rows without ever firing their
decorrelated joins.

Benchmark on duckdb/src/**/*.cpp (~1400 files, ~2M AST nodes):
- `function_definition#ExecuteRecursivePipelines`: 22.5s → 1.3s (17×)
- `...::callers`: 19.3s → 1.2s (16×)
- `...::callees`: 19.9s → 1.3s (15×)

## Test Coverage Added

The new test file `test/sql/css_selectors_multilang.test` covers:
- Type, ID, class selectors across Python, JavaScript, Rust, Go
- Descendant, child, adjacent sibling, general sibling combinators
- `:has()`, `:not(:has())`, compound selectors
- Positional pseudo-classes (`:first-child`, `:last-child`, `:nth-child`)
- Structural pseudo-classes (`:named`, `:definition`, `:scope`)
- Bare type prefix matching (`if`, `return`)
- Rosetta code examples cross-language consistency

Tests are written to pass against CURRENT behavior so as not to gate other
work, but the known-buggy sections are annotated with comments describing
expected-correct vs current behavior.

## Test Fixtures Added

- `test/data/python/css_selectors_test.py` - Python class hierarchy with methods
- `test/data/javascript/css_selectors_test.js` - JS classes, async, try/catch
- `test/data/rust/css_selectors_test.rs` - Rust struct/impl/trait
- `test/data/go/css_selectors_test.go` - Go struct/interface/methods
