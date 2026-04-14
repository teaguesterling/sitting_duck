# Bug: DuckDB v1.5.1 planner crash on correlated NOT EXISTS in SQL macro with column-valued arg

**Status:** Open (upstream — tracked as duckdb/duckdb#21890)
**Priority:** Medium (blocks `ast_select_rules` feature, does not affect `ast_select`)
**Discovered:** 2026-04-10
**Upstream issue:** https://github.com/duckdb/duckdb/issues/21890
**Upstream comment with our repro:** https://github.com/duckdb/duckdb/issues/21890#issuecomment-4228003688

## Summary

DuckDB v1.5.1 crashes with an internal binder assertion when a SQL macro is
called with a column-valued argument AND the macro body contains a correlated
`NOT EXISTS` / `EXISTS` / scalar `LIMIT 1` subquery over a CTE derived from
`unnest(scalar_function(macro_arg))`. This is a v1.5.0 → v1.5.1 regression in
the optimizer's dependent-join flattening / `ColumnBindingResolver` path.

The same macro called with a **literal** argument works fine. The bug fires
only on column-valued dispatch.

## Impact on sitting_duck

- `ast_select` continues to work for all literal selectors (220/220 regression
  tests pass). Normal user-facing behavior is unchanged.
- `ast_select_rules` and `ast_select_list` (added in the same commit as this
  tracker entry) are shipped in their "as-designed" form with a clear WIP
  status. They register successfully but crash on any call until the upstream
  fix lands. Test file `test/sql/_wip_ast_select_rules.test` asserts the
  macros register; the intended-behavior queries are documented as TODO
  blocks to uncomment after the upstream fix.
- Four commits of refactoring in `css_selectors.sql` (see commits `86587bc`,
  `69112d3`, `eb79a0f`, `7bba630`) eliminated self-correlated subqueries in
  the simple_type, simple_id, simple_class, sel_root, combinator_parts,
  has_conditions, not_has_conditions, attr_conditions, pseudo_classes, and
  pseudo_element CTEs. The outer SELECT body still has correlated NOT EXISTS
  patterns over `has_conditions` / `pseudo_classes` / etc. that would also
  trip the bug for column-valued dispatch, but the CTE refactors are
  architectural improvements that stand on their own merits.

## Minimal reproducer (pure vanilla DuckDB v1.5.1, no extensions)

```sql
-- The macro: a CTE built from unnest(scalar_function(macro_arg))
-- with a self-correlated NOT EXISTS over that CTE.
CREATE OR REPLACE MACRO bug_repro(q) AS TABLE
  WITH sel AS (
    SELECT unnest(string_split(q, ',')) AS tag,
           row_number() OVER () AS node_id
  )
  SELECT s.tag, s.node_id
  FROM sel s
  WHERE s.tag = 'a'
    AND NOT EXISTS (
        SELECT 1 FROM sel args
        WHERE args.tag = 'b'
          AND s.node_id > args.node_id
    );

-- WORKS: literal argument
SELECT * FROM bug_repro('a,b,a,b');
-- → (a, 1), (a, 1)

-- CRASHES: same string, passed as column value
WITH inputs(q) AS (VALUES ('a,b,a,b'))
SELECT m.* FROM inputs i, bug_repro(i.q) m;
-- INTERNAL Error: Failed to bind column reference "" [N.M]:
--   inequal types (BIGINT != VARCHAR)
-- (followed by a recursive stack trace through ColumnBindingResolver)
```

Run with: `/path/to/duckdb < this-snippet.sql`

## Trigger conditions

The bug fires if and only if **all four** are simultaneously true:

1. The query is inside a SQL macro (`CREATE MACRO foo(q) AS TABLE ...`),
   not a plain query or a view.
2. The macro is called with a **column-valued** argument (not a literal).
3. The macro body has a CTE built from `unnest(scalar_function(macro_arg))`.
4. The macro body has a correlated subquery (`NOT EXISTS`, `EXISTS`, or
   scalar `LIMIT 1`) over that CTE or any CTE derived from it.

Removing any one of (1)–(4) avoids the crash. Verified workarounds (all work):
- Rewriting `NOT EXISTS (SELECT FROM sel args WHERE s.x > args.x)` as
  `LEFT JOIN sel args ON s.x > args.x WHERE args.col IS NULL` (flat anti-join)
- Rewriting correlated `LIMIT 1` scalar subqueries as window-ranked CTEs with
  `WHERE rn = 1` (uncorrelated scalar selects from a derived relation)
- Calling the macro with a literal instead of a column value

## Error signature

```
INTERNAL Error: Failed to bind column reference "" [N.M]:
  inequal types (BIGINT != VARCHAR)

Stack Trace:
  ... ColumnBindingResolver::VisitReplace ...
  ... ColumnBindingResolver::VisitOperator ... (recursively)
  ... Optimizer::RunOptimizer ...
```

- Empty quoted column name (`""`) — the binder has lost the reference's origin
  during correlated-subquery rewriting.
- `BIGINT != VARCHAR` — BIGINT is an actual numeric column inside the CTE
  (e.g. `node_id`); VARCHAR is the outer macro parameter (`q`). The binder is
  comparing a column from inside the correlated subquery against the macro
  argument, which should not be involved in that comparison at all.

## Related upstream issues

- duckdb/duckdb#21604 — complex CTE chain with UNION ALL + ROW_NUMBER, same
  error signature (INTEGER != VARCHAR). Closed as reproduced.
- duckdb/duckdb#21788 — ROW_NUMBER OVER PARTITION BY in CTE chain, same error
  signature (VARCHAR != BIGINT). Closed as reproduced.
- duckdb/duckdb#21890 — HAVING with correlated scalar subquery + TRY(). Open.
- duckdb/duckdb#21913 — open PR proposing a fix for #21890 by remapping
  stale correlated bindings during dependent join flattening.

All four share the same error class (`Failed to bind column reference` +
`inequal types`) and stack-trace area (`ColumnBindingResolver::VisitReplace`).
Incremental fixes have been merged but the underlying bug class still has
uncovered trigger shapes.

## Upstream timeline

- **2026-04-13 06:51 UTC** — PR #21913 (fix for #21890) merged to `main`.
- **2026-04-13 10:42 UTC** — PR #22031 **reverted** #21913 on `main`. The fix
  broke `test/sql/error/test_try_expression.test_slow` and
  `test/sql/cte/test_correlated_recursive_cte.test_slow`; CI on #21913 only
  ran fast tests so the regression slipped through.
- **2026-04-13 10:46 UTC** — DuckDB v1.5.2 tagged/published, four minutes
  after the revert. **v1.5.2 does NOT contain the fix.**
- **2026-04-13 (same day)** — PR #22033 opened against `v1.5-variegata`
  (the 1.5 release branch) with a tightened version that preserves the
  original fix, tracks grouped/correlated column refs without allowing
  aggregates inside `TRY(...)`, and restores immediate-left-child
  correlated-binding remapping. Currently **open, not yet merged.**

v1.5.2 does include "TopNWindowElimination Column Binding Fix" and other
binder touch-ups, but those are different code paths and don't address
our trigger shape.

## Resolution plan

1. **Watch duckdb/duckdb#22033** (not #21913) for merge into `v1.5-variegata`.
2. **When the next 1.5.x patch release ships the fix:**
   - Pull the updated `duckdb` submodule.
   - Rebuild sitting_duck.
   - Rename `test/sql/_wip_ast_select_rules.test` → `test/sql/ast_select_rules.test`.
   - Uncomment the TODO blocks in that test file and update expected values.
   - Verify the CSS selector regression suite still passes.
   - Close this tracker entry and move to `tracker/bugs/resolved/`.
3. **No rush to revert our workarounds** even when the fix lands — the CTE
   refactors (typed views, `row_number() + WHERE rn = 1`, flat anti-joins)
   are architecturally cleaner than the correlated forms they replaced, so
   they stand on their own merits.
4. **If #22033 doesn't cover our specific macro shape when it lands**, add
   our repro to the upstream discussion (the comment on #21890 is already
   posted) and either:
   - File a separate issue, or
   - Work around it by fully restructuring `ast_select`'s outer SELECT body
     to eliminate correlated NOT EXISTS patterns over `has_conditions` etc.
     (estimated 4–8 hours of careful work).
