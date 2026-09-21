# 040 — `ast_select` macro planning is catastrophically slow on DuckDB v2.0

**Status:** open — **ship-blocking for the v2.0-compatible claim** (was mis-scoped as a deferrable follow-up)
**Found:** 2026-09-07, via community-extensions PR #2663 `test_against_latest` (DuckDB v2.0-cyanoptera)

## Corrected diagnosis (the original "re-parsing is slow" premise was WRONG)

The first cut of this bug blamed ~100 repeated `ast_select(file, …)` re-parses. That is
**falsified by measurement.** The mitigation branch `fix/css-selectors-multilang-timeout`
pre-parsed each file once (`ast_select_from` on a temp table, ~102 parses → ~7) and the test
**still took 2060s** (all 464 assertions pass, just slow). Pre-parsing removed the wrong cost.

### What the measurements actually show (build = Release, `-O3 -DNDEBUG`, cyanoptera)

In a single already-loaded session:
- `parse_ast('def f(): pass','python')` → **0.011s**
- `read_ast('…/css_selectors_test.py')` → **0.026s**  (parsing is NOT the problem)
- `ast_select_from('t','function_definition')` → **~15s**, and this cost is:
  - **independent of input size** — same ~15s on a 303-row AST and on a 3-node AST
  - **independent of selector complexity** — `function_definition`, `.function`, `#name`,
    `:has(.function)`, and a descendant combinator are all within ~3s of each other
  - **CPU-bound** (user ≈ real; ~180 threads spawned)

### The cost is PLANNING, not execution

`EXPLAIN` (binds + optimizes, does **not** execute) of a single selector on a 3-node table:
- **~55s**, and **repeatable** (two `EXPLAIN`s + one exec exceeded a 120s ceiling).

The `ast_select` macro expands to a **pathological query plan**: **215 Projections,
28 CROSS_PRODUCTs, 17 Hash Joins**, with the parsed CSS-selector AST inlined into the plan
as large STRUCT literals. On DuckDB v1.4.5 this same macro plans fast enough that
`css_selectors_multilang.test` passes well under the 600s per-test CI timeout (the community
`build_all` legs built against v1.4.5 are green). On v2.0-cyanoptera the binder/optimizer
takes tens of seconds **per call** — the ~28 cross products are the likely trigger for a
join-ordering / cardinality blowup.

## Why the CI symptom appears only on the amd64-latest leg

- v1.4.5 legs (regular `build_all`): macro plans fast → test passes.
- amd64 `test_against_latest` (v2.0-cyanoptera, manylinux docker): ~15s/call planning ×
  ~100 selector calls → >600s → timeout.
- arm64 cyanoptera leg passed, and a native amd64 run "passed" — because those finished
  eventually (>2000s) without hitting *their* wall, not because they were fast. Any earlier
  "full suite passed on cyanoptera" note in this tracker is consistent only if that run
  actually took 30+ min; it does not mean the selector path was fast.

## This is ship-blocking, not a follow-up

If `ast_select` costs ~15s to query 3 nodes on v2.0, the extension's headline CSS-selector
feature is effectively unusable for anyone who installs it on DuckDB latest. That is a broken
feature, not a CI-hygiene issue to route around a non-required check.

## Fix directions (in our control vs upstream)

1. **Shrink the macro's plan (our side, preferred).** The selector engine emits a plan with
   28 cross products and 215 projections for a trivial selector. Reducing that expansion
   (fewer decorrelated cross products, avoid inlining the whole parsed-selector STRUCT into
   the plan, push selector-AST walking into a smaller/CTE form) should cut planning time on
   BOTH versions and is not gated on an upstream fix.
2. **Upstream (DuckDB v2.0 optimizer).** Confirm whether v2.0 regressed join-ordering /
   binding on many-cross-product plans vs v1.4.5; if so, file upstream with this repro.

## Repro (native, cyanoptera build)

```sql
LOAD 'build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
CREATE TABLE t AS SELECT * FROM parse_ast('def f(): pass','python');
.timer on
EXPLAIN SELECT count(*) FROM ast_select_from('t','function_definition');  -- ~55s, no execution
```

## Next steps when picked up

1. Dump the fully-expanded macro SQL for one selector; find where the 28 cross products come
   from (likely the pseudo-class predicate catalog lookups and/or combinator handling).
2. Try a v1.4.5 build to get the per-call planning delta (CI already proves it's faster there).
3. Prototype a slimmer expansion; re-measure `EXPLAIN` time; re-run css_selectors_multilang.
