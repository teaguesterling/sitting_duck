# Brief — ast_select performance: 18 s per call, and 5.5 s of it is ours

**Status: specced, re-measured 2026-11-09, not started.** The design already
exists as `tracker/features/046-cpp-selector-engine.md`, with acceptance
criteria and risks. **Your first job is not to implement it, and it is no longer
to re-measure either.** That was done on 2026-11-09, and the numbers below
supersede both the spec's figures and this brief's first version. Decide from
them whether 046 is still the right shape, then get a plan approved before
writing code, per this repo's CLAUDE.md.

> **CORRECTION, 2026-11-09, and read it before anything else here.** The first
> version of this brief led with "42 s per call" and "3–7× worse than the
> spec". Both were wrong, and so was the conclusion drawn from them.
>
> The DuckDB v2.0 shell turns on agent mode whenever `CLAUDECODE` is set, which
> is every Claude Code session, and **that mode plans every statement twice**.
> Every timing taken that way was exactly 2× the real cost.
>
> Pass `-no-agent` to every `duckdb` invocation you time. Confirm it from the
> output: agent mode prints a banner on stderr and draws ASCII `|` tables,
> normal mode draws box-drawing `│` tables. **`env -u AI_AGENT` does NOT turn it
> off.** If you see a 2× spread on identical code, suspect the mode before the
> machine.

---

## Situation

`sitting_duck` is a DuckDB extension that parses source code with tree-sitter
and exposes the syntax tree as SQL tables. On top of those tables it offers
`ast_select(path, selector)` — a CSS-like selector language for querying syntax
trees, so `ast_select('f.py', 'function_definition')` returns the function
nodes, and richer selectors handle containment, adjacency, scopes and the call
graph.

That selector engine is implemented as a **SQL macro**, not as C++:
`src/sql_macros/css_selectors.sql` is 2071 lines with 88 common table
expressions, 11 of them validation, and 34 predicate-dispatch arms. It compiles
into a 324 KB generated header. Every `ast_select` call re-plans that whole
88-CTE query.

The project is mid-migration from DuckDB v1.5.x to v2.0. The repo currently
pins a v2.0 development build.

## Task

Three costs follow from the macro shape, and spec 046 argues they are one root
cause — the selector is re-planned on every call, so the cost is constant per
call, independent of selector complexity or table size:

1. A fixed planning cost per call. Anything issuing many selectors — a
   verifier, an evaluation harness, an agent loop — pays it per selector.
   `PREPARE` does amortize it, but **only with constant arguments** (18.1 s
   once, then 0.21 s per `EXECUTE`). Written as `ast_select($1, $2)`, `PREPARE`
   is instant and every `EXECUTE` pays 18.4 s again. So the premise holds for
   any caller that varies the selector or the file, which is every real
   caller.
2. The repo's v2.0 continuous-integration leg runs with tests skipped, because
   `css_selectors_multilang.test` takes roughly 40 minutes on the v2.0 line
   against seconds on v1.5.6. So the v2.0 signal is compile-only.
3. The community-extension registry's "test against latest" job needed a
   7200-second batch budget to fit the selector-heavy suites, against a
   600-second default. One leg measured 1 h 20 m 57 s.

## Action

Nothing implemented. What has changed since the spec was written is the
magnitude, and it has changed enough to be worth re-deciding on.

## Result

**Measured 2026-11-09 on `build/release` (DuckDB `v2.0.0-dev88609`,
`d54cbbe3df`), with `-no-agent` and `SET threads=1`, on longbottom at load 1.9
to 4.9:**

```
SELECT count(*) FROM ast_select('test/data/python/simple.py', 'function_definition');
  -> 4 rows.   Run Time (s): real 18.230

the equivalent plain query:
SELECT count(*) FROM read_ast('test/data/python/simple.py') WHERE type='function_definition';
  -> 4 rows.   Run Time (s): real 0.003

PREPARE with CONSTANT arguments, then two EXECUTEs:
  PREPARE  18.128     EXECUTE  0.219     EXECUTE  0.205

PREPARE with PARAMETERS, then three EXECUTEs:
  PREPARE   0.002     EXECUTE 18.640 / 18.364 / 18.362
```

So a single selector call costs **18.2 seconds** where the equivalent plain SQL
costs **3 milliseconds**.

**Where the time goes.** DuckDB's own detailed profiling, one call:

```
planner.total_time          16.741      optimizer.total_time     1.220
planner.binding_time         0.047        projection_pullup      0.841
physical_planner.total_time  0.091        join_order             0.125
query.cpu_time               0.125        cte_inlining           0.113
```

Binding is 47 ms. The whole optimizer is 1.22 s. Execution is 125 ms of CPU.
**16.74 s of 18.49 s is the logical planner building the plan.** An upstream
report needs to name the planner, not the optimizer.

**Do not use `EXPLAIN` as a planning-cost proxy.** It costs 73.4 s against the
query's 18.2 s, because it also renders a very large plan tree. This brief's
first version cited `EXPLAIN` at 20.6 s as evidence that "at least 20.6 s is
planning". That reasoning does not hold.

**Against spec 046's figure this is about 3× worse, not 7×.** The spec records
~6 s per call on the v2.0 line, measured 2026-09-15 against build `e3946f2327`.
It is now 18.2 s per call — and part of that rise is the macro's own growth, not
DuckDB's. Which brings us to the part that changed most.

### The attribution, which is the part that changed

Four builds sit in this one checkout, and **they do not hold sitting_duck
constant**: its selector macro grew from 1950 to 2371 lines, about 22%, over the
same period. Planning cost rises with statement size, so macro growth and DuckDB
changes were confounded in every earlier comparison, this brief's included.

Separating them needs no new build. The macro files are plain SQL, so the
September macro can be read into the November binary, where it shadows the
built-in one. Three paired rounds, alternating in one session, held to ±0.3 s:

| Build | Macro | Single-plan cost |
|---|---|---|
| v1.5.6 | Sept | 0.657 / 0.561 s |
| v2.0.0-dev85467 | Sept | 15.308 / 12.403 s |
| v2.0.0-dev86072 | Sept | 12.577 / 11.822 s |
| v2.0.0-dev88609 | Sept | ~12.8 s |
| v2.0.0-dev88609 | **Nov** | **18.230 s** |

Two conclusions:

- **The dev86072 → dev88609 DuckDB step is about zero.** The same macro costs
  ~12.8 s on the newer build against 11.8–12.6 s on the older one. The "further
  ~3×" in this brief's first version was agent mode plus macro growth.
- **About 5.5 s of the 18.2 s is this project's own**, from 22% macro growth.
  That is the cheapest available win, and it needs no C++.

The real upstream gap is the older one: the v1.5.x → v2.0 jump, roughly 20×,
unmoved since September.

---

## Your prompt

Two of the three questions this brief first asked are answered in the Result
section above. What is left:

1. **Does the cost scale, or is it constant?** The spec asserts constant. Test
   it where it is cheap: vary the selector's complexity, then vary the file size
   and node count, with `-no-agent` throughout. A constant cost and a scaling
   cost call for different fixes. Note that the macro's own SIZE does scale the
   cost — 22% more macro cost about 5.5 s — so "constant" can only mean constant
   in the INPUT, not in the macro.
2. **Try the cheap win first: shrink the statement.** About 5.5 s of the 18.2 s
   came from the macro growing 22%. Find out what that growth was and whether
   any of it can be made lazy, shared, or moved out of the planned statement.
   This is the one fix that needs no C++ and no upstream help, and it should be
   measured before 046 is judged.
3. **Only then** judge whether to implement 046 as written, implement part of
   it, or do something else. Spec 046 is still worth doing for the remaining
   ~12 s, but it is no longer the only lever, and the urgency argument that
   rested on "42 s per call" is gone.

Do NOT re-litigate whether this is a sitting_duck problem or a DuckDB problem.
It is measurably both: ~12 s of the 18.2 s arrived with the v1.5.x → v2.0 jump
and has not moved since September, and ~5.5 s is this project's own macro
growth.

Read `tracker/features/046-cpp-selector-engine.md` before proposing anything —
it has acceptance criteria and a risk list you should not have to invent.

## Already measured — do not re-derive

From spec 046, which is itself careful about this:

| DuckDB line | planning cost per call | provenance |
|---|---|---|
| v2.0-dev (September) | ~6 s — about 5.2 s planner, 0.9 s optimizer, 0.2 s execution | `tracker/features/042`, 2026-09-15, build `e3946f2327` |
| shipped v1.5.x | ~0.3–0.5 s | `042` (v1.5.5); `048` and a 2026-10-08 re-measure (v1.5.6, 0.441–0.502 s) |
| **v2.0-dev, Nov macro** | **18.230 s, of which 16.74 s is the planner** | this brief, 2026-11-09, `-no-agent`, build `7f9baef` / `v2.0.0-dev88609` |
| **v2.0-dev, Sept macro** | **~12.8 s** | same binary, September macro read in to shadow the built-in one |
| v1.5.6, Sept macro | 0.657 / 0.561 s | same checkout, 2026-11-09, `-no-agent` |

**Every figure in this table that was measured before 2026-11-09 may be 2×
high**, because agent mode plans twice. The three rows dated 2026-11-09 are not.

The spec's own warning, which still holds and is now more important:

> "The magnitude depends on which DuckDB line you are on, and the two differ by
> ~12×. Do not quote a number without saying which."

Add a second clause to it: do not quote a number without saying which MACRO, and
whether `-no-agent` was set.

Structure of the macro, measured when the spec was written:

| | |
|---|---|
| `src/sql_macros/css_selectors.sql` | 2071 lines |
| common table expressions | 88, of which 11 are validation |
| predicate dispatch arms | 34 |
| generated `embedded_sql_macros.hpp` | 324 KB |

Suite timings measured in this session, for context on what a slow selector path
costs the test suite: `css_selectors.test` 273.7 s,
`conformance/no_silent_empty.test` 282.4 s. A separate, older measurement puts
`css_selectors_multilang.test` at 2909.9 s (48 minutes) — note that two places
in the repo quote "~40 min" for it, so the quoted figure is itself stale.

Related upstream and internal references, from the spec: upstream
`duckdb/duckdb#26036` is the test-timeout symptom; issue #160 is the planning
cost; `tracker/bugs/040` is the v2.0 planning-cost regression; `tracker/043`
Part A is a seeded call-graph join that retires much of the same per-call tax in
pure SQL and which `tracker/features/048` sequences **ahead** of the C++ engine
work.

## Limits agreed up front

- **The test suite is the specification.** Spec 046 is explicit: every existing
  selector test must pass unchanged — `ast_select_pseudo_classes`,
  `css_selectors`, `css_selectors_multilang`, `ast_selector_for`. Do not "fix"
  selector behaviour in the same change as a performance change.
- **The error messages are behavioural contract**, not incidental. Malformed
  selectors must raise and name their replacement; well-formed selectors that
  match nothing must return nothing. That is the outcome of issues #89, #128
  and #184.
- **No semantics rewrite.** The selector language settled across v1.14.0 to
  v1.15.4 and is not in scope.
- If an implementation lands incrementally and both paths are live at once, prove
  they agree with an `EXCEPT`-both-ways differential over the fixture corpus —
  the pattern used in #184 to prove `:scope` and `::scope` identical.
- Route builds and tests through the `blq` MCP tools and git/GitHub through
  `jetsam`. **Heavy or long-running work goes through Slurm** — load the `slurm`
  skill before any big sweep that does not go through blq. Timing work is
  exactly the case where a loaded machine gives a meaningless number, so check
  what else is running before you measure, and take two runs of identical code
  before believing a ratio: two runs of the same code once differed 1.56× in
  this repo's continuous integration.

## The first question to settle

**Settled on 2026-11-09. It is both, in measured proportions.** Of the 18.2 s,
about 12 s arrived with the v1.5.x → v2.0 jump and has not moved since
September; about 5.5 s is this project's own macro growing 22%. The upstream
part is filed as `duckdb/duckdb#26036`, which is open and labelled
`reproduced` — so there is no fix on duckdb `main` to be behind.

**The question now is which lever to pull first**, and this brief recommends the
cheap one: shrink the planned statement. It needs no C++ and no upstream help,
and it is worth ~5.5 s on today's numbers. Spec 046 remains worth doing for the
remaining ~12 s, and the ordering in `tracker/features/048` (call-graph join
first) still stands.

One caution about the earlier framing. The argument "if v1.5.x costs 0.5 s and
v2.0 costs 42 s then the 42 s is upstream's" was built on a figure that agent
mode had doubled, and on a comparison that did not hold the macro constant.
Both halves of that reasoning were wrong, and the conclusion it reached — that
an upstream report is the whole answer — does not follow.

## Links

- The spec: `tracker/features/046-cpp-selector-engine.md`
- Supporting: `tracker/features/042` (selector correctness plan, where the
  September numbers live), `tracker/features/048` (v2 work register and
  ordering), `tracker/bugs/040` (v2.0 planning-cost regression),
  `tracker/043` (diagnosis, Part A is the seeded call-graph join)
- The macro itself: `src/sql_macros/css_selectors.sql`
- Issue: `gh issue view 160`; upstream `duckdb/duckdb#26036`
- The 2026-11-09 re-measurement, written up with the paired tables and the
  harness error: https://claude.ai/artifact/1z3V6qoVG7nDoP8HuKWnoZ
- The standing lesson, in memory as `the-harness-was-in-the-measurement`
- Originating session: https://claude.ai/code/session_01FVcSpAnvP7udQhjpSvSPTE
