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

## Bug 1: `type:has(.class)` returns 0 results

When the base selector is a type (tag_name) and `:has()` uses a semantic-class
argument, the filter produces no matches.

### Examples
```sql
-- Returns 0 rows (incorrect):
SELECT name FROM ast_select('test/data/python/css_selectors_test.py',
    'function_definition:has(.call)');

-- Returns 3 rows (correct):
SELECT name FROM ast_select('test/data/python/css_selectors_test.py',
    '.function:has(.call)');  -- main, make_animal, process_animals
```

### Workaround
Use `.class:has(.class)` form OR switch inner `:has()` to a type selector:
`function_definition:has(call)` works.

### Likely cause
The `has_conditions` CTE extracts `has_class` correctly, but when the base
selector is a type, something prevents the `has_class` check from firing
against the right `semantic_type` values in the candidate set. Suspect:
the CASE dispatch for `tag_name` base + `:has()` doesn't call `is_semantic_type`
on descendants. Needs investigation in `src/sql_macros/css_selectors.sql`
around lines 418-429 and 692-703.

## Bug 2: `:not(:has(.class))` also broken

Same root cause as Bug 1.

```sql
-- Returns 0 rows (incorrect — should return 7 functions without calls):
SELECT name FROM ast_select('test/data/python/css_selectors_test.py',
    '.function:not(:has(.call))');

-- Returns 7 rows (correct) using type-based :has:
SELECT name FROM ast_select('test/data/python/css_selectors_test.py',
    'function_definition:not(:has(call))');
```

## Bug 3: Combinators combined with pseudo-classes on the right side

When a combinator is followed by a selector that has a pseudo-class, the CSS
parser wraps the whole thing as `pseudo_class_selector(combinator_selector, ...)`.
The selector engine then treats the root as `pseudo_class_selector` and applies
only the pseudo-class filters, ignoring the combinator structure entirely.

### Example
```sql
-- Should return Cat, Dog (classes that have speak method AND follow another class)
-- Actually returns Animal, Cat, Dog (ignores the sibling combinator)
SELECT name FROM ast_select('test/data/python/css_selectors_test.py',
    'class_definition ~ class_definition:has(function_definition#speak)');
```

### Likely cause
`sel_root_raw` picks the outermost pseudo_class_selector and dispatch code
hits the pseudo_class_selector CASE branch (lines 640-645) which only
checks `type_filter`, `name_filter`, `class_filter`. The `child_selector` or
`sibling_selector` child is never used for the combinator matching.

### Impact
Combinator + pseudo-class is a common pattern that users would expect to work.
The bug silently returns a superset of correct results, which is worse than
a clean failure.

## Bug 4: `ast_select_from` defined in SQL but missing from embedded macros

`ast_select_from` macro is defined in `src/sql_macros/css_selectors.sql`
(commit e0902f7) but was never embedded into `src/include/embedded_sql_macros.hpp`.
Running `python3 scripts/embed_sql_macros.py` regenerates the embed file but
reveals a separate issue: the `ast_select` convenience wrapper uses
`query_table('__ast_src')` on a CTE name, which DuckDB cannot resolve.

### Fix options
1. Make `ast_select` a wrapper that doesn't rely on CTE-named query_table,
   or register `ast_select_from` as a standalone macro without the wrapper.
2. Update the embed script to include `ast_select_from` and fix the wrapper.

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
