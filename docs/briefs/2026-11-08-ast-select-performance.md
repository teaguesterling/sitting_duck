# Brief — ast_select performance: the planning cost has got 3–7× worse

**Status: specced, not started.** The design already exists as
`tracker/features/046-cpp-selector-engine.md`, with acceptance criteria and
risks. **Your first job is not to implement it — it is to re-measure, because
the spec's central number is out of date and the gap has widened a lot.** Decide
from fresh numbers whether 046 is still the right shape, then get a plan
approved before writing code, per this repo's CLAUDE.md.

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

1. A fixed planning cost per call. `PREPARE` does not amortize it; each
   `EXECUTE` pays again. Anything issuing many selectors — a verifier, an
   evaluation harness, an agent loop — pays it per selector.
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

**Measured today, 2026-11-08, on `build/release` at commit `694ebef`, DuckDB
`v2.0.0-dev88609` (`d54cbbe3df`):**

```
SELECT count(*) FROM ast_select('test/data/python/simple.py', 'function_definition');
  -> 4 rows.   Run Time (s): real 41.951
  -> same call again:        real 42.413     (nothing amortizes)

the equivalent plain query:
SELECT count(*) FROM read_ast('test/data/python/simple.py') WHERE type='function_definition';
  -> 4 rows.   Run Time (s): real 0.005

planning alone, no execution:
EXPLAIN SELECT count(*) FROM ast_select(...);
  -> Run Time (s): real 20.586
```

So a single selector call costs **42 seconds** where the equivalent plain SQL
costs **5 milliseconds** — about four orders of magnitude — and **at least 20.6
of those seconds are planning**, with no execution at all. The second identical
call cost the same, confirming nothing is cached between calls.

**This is 3–7× worse than the figure spec 046 rests on.** The spec records ~6 s
per call on the v2.0 line, measured 2026-09-15 against build `e3946f2327`. It is
now 42 s per call, 20.6 s of it planning.

---

## Your prompt

Re-measure first, then decide. Specifically:

1. **Reproduce the numbers above**, and extend them where it is cheap: does the
   cost scale with selector complexity at all, or is it genuinely constant? Does
   it scale with file size or node count? A constant cost and a scaling cost
   call for different fixes, and the spec asserts constant.
2. **Find out what regressed between September and now.** The spec's ~6 s was
   build `e3946f2327`; today's is `v2.0.0-dev88609` / `d54cbbe3df`. A 3–7×
   planner regression in a DuckDB development build may be an upstream problem
   with an upstream fix, in which case the right move is to report it upstream
   and not to rewrite the engine. Check whether the shipped v1.5.x line still
   measures ~0.3–0.5 s; if it does, this is a v2.0 planner regression and not a
   sitting_duck defect, and that reframes the whole piece of work.
3. **Only then** judge whether to implement 046 as written, implement part of
   it, or do something else.

Read `tracker/features/046-cpp-selector-engine.md` before proposing anything —
it has acceptance criteria and a risk list you should not have to invent.

## Already measured — do not re-derive

From spec 046, which is itself careful about this:

| DuckDB line | planning cost per call | provenance |
|---|---|---|
| v2.0-dev (September) | ~6 s — about 5.2 s planner, 0.9 s optimizer, 0.2 s execution | `tracker/features/042`, 2026-09-15, build `e3946f2327` |
| shipped v1.5.x | ~0.3–0.5 s | `042` (v1.5.5); `048` and a 2026-10-08 re-measure (v1.5.6, 0.441–0.502 s) |
| **v2.0-dev (today)** | **~42 s, of which ≥20.6 s is planning** | this brief, build `694ebef` / `v2.0.0-dev88609` |

The spec's own warning, which still holds and is now more important:

> "The magnitude depends on which DuckDB line you are on, and the two differ by
> ~12×. Do not quote a number without saying which."

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

**Is this a sitting_duck problem or a DuckDB v2.0 planner regression?** If the
shipped v1.5.x line still costs ~0.3–0.5 s per call while the v2.0 development
build now costs 42 s, then the 42 s is upstream's and the right first action is
an upstream report with a minimal reproduction — not a C++ engine. Spec 046
would still be worth doing for the ~0.5 s constant, but it would stop being
urgent, and the ordering in `tracker/features/048` (call-graph join first) would
stand. One measurement on each line settles it, and everything else depends on
the answer.

## Links

- The spec: `tracker/features/046-cpp-selector-engine.md`
- Supporting: `tracker/features/042` (selector correctness plan, where the
  September numbers live), `tracker/features/048` (v2 work register and
  ordering), `tracker/bugs/040` (v2.0 planning-cost regression),
  `tracker/043` (diagnosis, Part A is the seeded call-graph join)
- The macro itself: `src/sql_macros/css_selectors.sql`
- Issue: `gh issue view 160`; upstream `duckdb/duckdb#26036`
- Originating session: https://claude.ai/code/session_01FVcSpAnvP7udQhjpSvSPTE
