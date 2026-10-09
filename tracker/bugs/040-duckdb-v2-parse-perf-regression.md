# 040 — `ast_select` macro planning is catastrophically slow on DuckDB v2.0

**Status:** open — **ship-blocking for the v2.0-compatible claim** (was mis-scoped as a deferrable follow-up)
**Found:** 2026-09-07, via community-extensions PR #2663 `test_against_latest` (DuckDB v2.0-cyanoptera)
**Re-measured 2026-10-08 against cyanoptera `d4c9dfd469f`: numbers below are CURRENT, not stale.**
The ~27x per-call penalty is undiminished and the ship-blocking status is confirmed — see
[RE-MEASUREMENT 2026-10-08](#re-measurement-2026-10-08--the-regression-has-not-improved-on-this-path)
at the end of this file. The upstream #26036 improvement did **not** reach this path.

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

cyanoptera (`d4c9dfd469`), 463 assertions, `rc=0` each:

| run | harness-reported | wall (`time`) | load before -> after |
|---|---|---|---|
| 1 | **2060.017s** | 34m20.067s | 6.54 -> 2.17 |
| 2 | **2043.697s** | 34m3.744s | 17.25 (spike at launch) -> 1.86 |

Spread 0.8%. `user`/`real` = 1.06 on run 1 — the suite is essentially
single-threaded end to end.

**This bug records 2060s. Run 1 came back as 2060.017s.** That exact agreement
to four significant figures is a **coincidence, not a confirmation**, and the
reason matters: the recorded 2060s was measured on the mitigation branch
`fix/css-selectors-multilang-timeout` (pre-parsing applied, ~102 parses -> ~7),
at **464** assertions, on an earlier cyanoptera commit, via a harness this file
does not name. Today's figure is **main @ `6f82c98`**, 463 assertions,
`d4c9dfd469f`, via the `unittest` binary directly. Two different configurations
landing on 2060 is luck.

**So read the comparison this way:** neither historical comparison is clean —
the `Makefile`'s four figures came via `run_tests.py` at a different commit, and
this bug's 2060s came from a different branch and assertion count via an
unrecorded harness. **What is clean is the two sides measured here today, on one
source tree with one harness: 167.0s on v1.5.6 against 2051.9s on cyanoptera,
12.3x.** The agreement with the recorded 2060s is same-order across
configurations that differ in known ways. That is enough to retire "stale" —
this bug's magnitude is current — and not enough to call the numbers identical.
It is also consistent with the branch difference being immaterial, since this
bug's own measurement shows parsing is negligible (`read_ast` 0.026s), which is
exactly why pre-parsing did not help in the first place.

## Result 3 — all four heavy suites named in the `Makefile`

This bug names only `css_selectors_multilang.test`. The `Makefile`'s "Test Runner
Budget" comment names **four**, with times measured against `v2.0.0-dev85467`
*via `run_tests.py`*. All four, one file per `unittest` invocation, exit code
checked individually, every run `rc=0`:

| suite | calls | v1.5.6 | cyanoptera | ratio | `Makefile` (dev85467, run_tests.py) |
|---|---|---|---|---|---|
| `bugs/issue_88_89_callee_name_and_guards.test` | 25 | **20.830s** | **425.070s** | 20.4x | 709s |
| `ast_select_combinator_steps.test` | 37 | **38.874s** | **666.558s** | 17.1x | 1203s |
| `ast_select_pseudo_classes.test` | 65 | **441.443s** | **2058.709s** | 4.7x | 2431s |
| `css_selectors_multilang.test` | 122 | **167.0s** (mean of 3) | **2051.9s** (mean of 2) | 12.3x | 3168s |
| **total** | | **668.1s** (11.1 min) | **5202.2s** (86.7 min) | **7.8x** | 7511s (125 min) |

load 1.19–3.35 across the cyanoptera trio, 1.39–1.94 across the v1.5.6 trio.

Two things to read carefully here, because they point opposite ways:

- **Against the `Makefile`'s numbers, every suite is faster** (7511s -> 5202s,
  1.44x). **Do not bank that as an upstream win.** Those figures came through
  `run_tests.py` at a different commit; these came from the `unittest` binary
  directly. The harness differs, so the comparison is not clean.
- **Against this bug's own directly-measured 2060s, nothing changed.** That IS a
  clean comparison — same harness, same invocation — and it is the one to trust.

## Result 4 — #160 gets its current v2.0-line number

Reproducing `048`'s 2026-10-07 methodology on both lines. `small` = 303 nodes
(`test/data/python/css_selectors_test.py`), `big` = 156,169 nodes
(`src/**/*.cpp`), `threads`=36:

| table | nodes | selector | cyanoptera | v1.5.6 |
|---|---|---|---|---|
| small | 303 | `.fn` | 12.778s / 12.681s | 0.440s / 0.506s |
| big | 156,169 | `.fn` | 12.809s / 12.946s | 0.608s / 0.571s |
| small | 303 | `.fn:calls(main)` | 12.731s | 0.523s |

load 1.69 -> 1.42 (cyanoptera), 1.17 -> 1.16 (v1.5.6).

- **#160's number on the v2.0 line is ~12.8s per `ast_select_from` call.** Its
  issue text says ~6s; the real figure on this line is **twice** what the issue
  claims, not a twelfth of it.
- `048`'s ~0.5s is confirmed independently (0.44–0.61s here, from a separate
  build in a separate session). **Both numbers in #160's thread are right — they
  are just measuring different DuckDB lines.** #160 should record both.
- The *shape* `048` describes holds on the v2.0 line too, and more starkly:
  **515x more nodes costs 0.1s more** (12.73s -> 12.88s), and a call-graph
  pseudo-class is indistinguishable from a bare class. The cost is constant,
  compile-side, and independent of everything that should matter.

## Result 5 — the core-count question, measured rather than caveated

Same per-call exec, pinned with `taskset -c 0,1` to **2 cores**:

| | 36 cores | 2 cores (`taskset -c 0,1`) |
|---|---|---|
| cyanoptera | 13.091 / 13.049 / 13.050s | **12.907 / 12.935 / 12.940s** |
| v1.5.6 | 0.440 / 0.500 / 0.486s | **0.564 / 0.515 / 0.486s** |

load 0.56–1.05 throughout. **Taking 34 of 36 cores away changes the cyanoptera
cost by under 2%** — and `user` ~= `real` on the `EXPLAIN` says why: the
expensive work is single-threaded binding/optimising.

## Honest read: is `040` still ship-blocking?

**Yes, at undiminished magnitude.** The original argument was "if `ast_select`
costs ~15s to query 3 nodes on v2.0, the headline CSS-selector feature is
effectively unusable for anyone who installs it on DuckDB latest." Today that
cost is **13.0s**, on 2 cores or 36, on 3 nodes or 156,169. The argument is
unchanged and the number backing it is current.

What *has* changed is the diagnosis, and it makes this bug **less** dependent on
upstream, not more:

- #26036's own generator improved ~5x (20.6x -> 4.3x at 93 KB) and its startup
  regression is fully fixed (2.50s -> 0.08s). **None of that reached this path.**
- So the `ast_select` pathology is most likely **not** the same cost centre as
  #26036's statement-size super-linearity. It is more plausibly the 28
  cross-products / join-ordering blowup this bug already identified — which
  #26036 being fixed would not fix. *(Hypothesis, not a measurement: the
  measurement says only that the improvement did not transfer.)*
- Practical consequence: **fix direction 1 (shrink the macro's plan) is the one
  to fund.** Waiting on upstream has now been tried for three weeks, upstream
  genuinely improved, and this path did not move. `048` item #6 (seeded
  call-graph join) and #5 (`ast_select` -> C++) are the levers that remain.

## Honest read: could the canary's test phase now pass in a CI budget?

**Probably yes — but it does not make re-enabling it a good trade, and this
measurement does nothing to relieve the gate the `Makefile` already states.**

- **It would pass, not time out.** All four suites pass, `rc=0`, and the slowest
  file is ~2060s against the `TEST_BATCH_TIMEOUT` of 7200s — ~3.5x headroom.
- **But the cost is hours.** The four heavy suites alone are **86.7 min of
  serial work on this box**, and `TEST_BATCH_SIZE=1` plus a 2–4 core runner
  means near-serial execution. Add the rest of the 138-file suite and a full
  DuckDB build in the same job, against GitHub's 6h job cap, and the margin is
  real but not comfortable.
- **The `Makefile` already frames this correctly** and this measurement
  confirms, not changes, that framing: "RE-ENABLING CANARY TESTS is now gated on
  RUNTIME COST, not on duckdb#26036 … dropping `skip_tests` trades a build-only
  canary for a multi-hour one. That is a deliberate call to make with a release."
  Teague ruled on 2026-10-08 that the canary stays off. **Nothing measured here
  argues against that ruling.** No CI configuration was changed.

### The core-count caveat, stated explicitly — and then measured

**CI runners have 2–4 cores against this box's 36, so a time measured here is a
lower bound, not what CI would see.** That caveat stands and must be carried
with these numbers.

What the 2-core probe adds is *how much* of a lower bound: **not much, and not
because of core count.** Dropping to 2 cores cost under 2%. The residual CI
uncertainty is therefore **per-core clock speed and cold filesystem cache, not
parallelism** — a GitHub-hosted runner's single-core throughput is meaningfully
below this desktop part, so I would expect something in the region of 1.3–2x
these figures, i.e. the slowest file landing around 2700–4100s. That is still
inside the 7200s per-file budget, but it narrows the headroom from ~3.5x to
roughly ~1.8–2.7x. **That multiplier is an estimate extrapolated from the
measured core-count insensitivity, not a measurement on CI hardware — nobody has
timed this on a 2-core GitHub runner, and that is the one number this record
does not have.**

## What could still be wrong with this record

- The 2060.017s / 2060s agreement is a coincidence between two configurations
  that differ in branch, assertion count, DuckDB commit and harness. Run 2
  (2043.7s) and the clean same-day 167.0s-vs-2051.9s pair are the reasons to
  believe the magnitude; the coincidence itself is not evidence of anything.
- **The assertion count moved, 464 -> 463.** This bug records 464; both builds
  report 463 today. One assertion was removed or merged from the suite between
  then and `6f82c98`. It is a 0.2% change and cannot explain a 2000s runtime,
  but it is one more reason the historical number is not a like-for-like
  baseline, and I did not track down which assertion changed.
- The `Makefile`'s four baseline figures came via `run_tests.py`; mine came from
  `unittest` directly. The 1.44x "improvement" against them may be harness
  overhead rather than DuckDB, and I did not separate the two.
- The two builds do not link an identical extension set — cyanoptera links
  `[core_functions, parquet, json, icu]` with `sitting_duck` loadable, v1.5.6
  links `[core_functions, sitting_duck, parquet]`. That is each line's own
  default build, which is the honest comparison, but a larger catalog is not a
  free variable in a bind-cost measurement and I did not control for it.
- The v1.5.6 binary reports `library_version` `v0.0.1` because its worktree is a
  shallow clone with no tags. `source_id` `069cc9f9` is what identifies it.
- Attribution of the delta to DuckDB rests on both sides being built from
  identical sitting_duck source, which they are (`6f82c98`).
- Only `css_selectors_multilang.test` was run more than once on each side. The
  other three suites are single runs; given the ±3% and ±0.8% spreads seen on
  the repeated one I expect them to be representative, but that is inference.

## Shared submodule pin — verified

`/home/teague/Projects/sitting_duck/duckdb` was never written to. Its gitlink and
its checked-out HEAD both read `069cc9f9b5be802405797faecc284961b07c70ef`,
checked before setup, after setup, and after all measurements. The cyanoptera
side was built from an independent repository inside this worktree, with
`d4c9dfd469f` fetched over a local path from the pre-existing cyanoptera
worktree; no command in this session targeted the shared checkout's repository.
