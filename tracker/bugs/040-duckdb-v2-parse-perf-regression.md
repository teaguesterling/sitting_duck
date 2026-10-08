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

---

# RE-MEASUREMENT 2026-10-08 — the regression has NOT improved on this path

Motivation: upstream duckdb/duckdb#26036's own generator was re-measured on
2026-10-08 and found **~5x less severe than recorded** (at 93 KB, cyanoptera
`d4c9dfd469f` is **4.3x** v1.5.6, down from the **20.6x** measured 2026-09-22;
the startup regression is fully fixed, 2.50s -> 0.08s — see `048`). The numbers in
this bug were ~3 weeks old, so they were re-taken to see whether the upstream
improvement carried over.

**It does not carry over.** On the `ast_select` macro path the v2.0 penalty is
still ~27x, measured today on identical sitting_duck source.

## What was built and measured

| | commit | `pragma_version().source_id` | build |
|---|---|---|---|
| v2.0-cyanoptera | `d4c9dfd469f25b217c6fa6090dae3f4899feb5cc` | `d4c9dfd469` | `v2.0.0-dev1` |
| v1.5.x line (repo pin) | `069cc9f9b5be802405797faecc284961b07c70ef` | `069cc9f9` | version string reads `v0.0.1` — a shallow-clone `git describe` fallback; `source_id` is authoritative |

Both built from **the same sitting_duck source**, `main` @ `6f82c98`, Release,
`GEN=ninja`, **all 27 built-in languages** (no `-DSITTING_DUCK_LANGUAGES`), 0
compiler errors on either side.

The pre-existing `/home/teague/Projects/sitting_duck/build/release` (mtime
2026-10-05 14:19) was **deliberately not used** as the v1.5.6 comparator: it
predates `730b648` (2026-10-05 18:25), which added per-call semantic-class
validation *inside* the `ast_select` macro. Comparing a post-`730b648`
cyanoptera build against a pre-`730b648` v1.5.6 build would have inflated the
ratio. A fresh v1.5.6 build at `6f82c98` was made instead.

## Measurement conditions

36-core box. `/proc/loadavg` recorded either side of every run and quoted below.
Load sat at **4.2–7.0** throughout — about 12–19% utilisation; the two other
`unittest` runs visible during this window consumed ~3 cores between them
(other agents verifying their own work). The earlier "wait for load < 3" rule
was recalibrated: the 7.41s -> 1.36s swing that motivated it was measured at
load 25–28 on this same box, i.e. genuine saturation, which is not this
situation. DuckDB thread count was left at the default (all cores) so the
numbers stay comparable with the ones already recorded in this file and in the
`Makefile`. **Every run is reported; nothing is a best-of.**

## Result 1 — this bug's own repro block (lines 65–70), 3-node table

```
CREATE TABLE t AS SELECT * FROM parse_ast('def f(): pass','python');
EXPLAIN SELECT count(*) FROM ast_select_from('t','function_definition');
```

| | run 1 | run 2 | run 3 |
|---|---|---|---|
| **cyanoptera `EXPLAIN`** | 73.328s | 74.111s | 76.295s |
| **cyanoptera exec** | 13.091s | 13.049s | 13.050s |
| **v1.5.6 `EXPLAIN`** | 0.646s | 0.592s | 0.583s |
| **v1.5.6 exec** | 0.440s | 0.500s | 0.486s |

load: cyanoptera 4.25 -> 6.59; v1.5.6 5.73 -> 6.64.

- **`EXPLAIN` ratio ~124x. Exec ratio ~26.6x.**
- `user` ~= `real` on the cyanoptera `EXPLAIN` (73.14 user / 73.33 real) —
  **planning is effectively single-threaded**, so it will not be rescued by a
  wider machine, and a 2-core CI runner costs it little extra.
- Old vs new on this same repro: `EXPLAIN` **~55s -> ~74s**, per-call exec
  **~15s -> ~13.1s**. Both are the same order; neither moved the way #26036's
  generator did. The `EXPLAIN` figure is not a clean before/after (the macro
  itself grew via `730b648`/`048bd59` in between), so the honest statement is
  **"unchanged in magnitude", not "regressed further"**.
- The v1.5.6 exec figure (0.44–0.50s) independently reproduces `048`'s
  2026-10-07 `#160` measurement of ~0.5s, from a separate build. That agreement
  is worth something: two builds, two sessions, same number.

## Result 2 — `test/sql/css_selectors_multilang.test`

v1.5.6 (`069cc9f9`), 463 assertions, `rc=0` each, one file per `unittest`
invocation:

| run | wall | load before -> after |
|---|---|---|
| 1 | **162.1s** | 6.22 -> 5.68 |
| 2 | **172.9s** | 5.42 -> 7.03 |
| 3 | **167.0s** | 6.62 -> 6.99 |

Spread ±3% — the measurement is reproducible at this load.

cyanoptera: see the next section (measured separately; it is the long pole).
