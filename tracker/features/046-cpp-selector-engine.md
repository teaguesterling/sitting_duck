# 046 — C++ CSS selector engine

**Status:** Not started. On the v2.0.0 list; no prior tracker entry (this is it).
**Related:** #160 (planning cost), #89 (silent-empty, closed by the guard approach),
upstream duckdb/duckdb#26036 (test-timeout symptom), 043 (diagnosis).

## Why

`ast_select` / `ast_select_from` are implemented as a SQL macro. Measured today:

| | |
|---|---|
| `src/sql_macros/css_selectors.sql` | **2071 lines** |
| CTEs | **88** (11 of them validation) |
| predicate dispatch branches | **34** `WHEN '<name>' THEN` arms |
| generated `embedded_sql_macros.hpp` | **324 KB** |

Three separate costs follow from that shape, and all three are the same root cause —
the selector is *re-planned as a 88-CTE query on every call*:

1. **~6 s of planning per call, independent of table size or selector** (#160):
   ~5.2 s planner + ~0.9 s optimizer against ~0.2 s of execution. `PREPARE` does not
   amortize it — each `EXECUTE` pays again. Anything issuing many selectors (a
   verifier, an eval harness, an agent loop) pays it per selector.
2. **The v2.0 canary runs `skip_tests`.** `css_selectors_multilang.test` takes ~40 min
   on the v2.0 line against seconds on v1.5.6, so the repo's own cyanoptera leg builds
   but does not test (upstream duckdb/duckdb#26036). We ship a compile-only v2.0
   signal because of this.
3. **The registry's `test_against_latest` needed a 7200 s batch budget** to fit the
   ast_select-heavy suites (measured: one leg at 1h20m57s against a 600 s default).

## What it is

Move selector *parsing, validation and planning* into C++, keeping the row-producing
work in SQL/DuckDB where it belongs. The selector string is parsed once into a
structure; validation runs against that structure rather than as 11 correlated CTEs;
the engine emits a bound plan instead of a 2071-line macro expansion.

Explicitly **not** a rewrite of the semantics. v1.14.0–v1.15.4 settled the hard part
— exact-vs-prefix type matching, `:has`/adjacency, direct-and-lambda-aware call graph,
the three scope operations, the no-silent-empty guards. Those behaviours and their
tests are the specification; this is an implementation change underneath them.

## Why it gates v2.0.0

- It is the only fix for #160, and #160 is what makes selector-driven tooling
  (verifiers, eval harnesses, agents) impractical today.
- It retires the `skip_tests` compromise, so the v2.0 line gets a *tested* signal
  rather than a compile-only one.
- The v2.0 architecture (docs/planning/v2-architecture.md) puts the selector engine in
  `sitting_duckling`, the slim core. A 324 KB generated SQL header is a poor engine
  boundary; a C++ engine is the thing M3 would actually extract.

## Acceptance

1. `ast_select_from` planning cost is O(selector), not ~6 s fixed — #160's repro drops
   to the execution floor (~0.2 s CPU).
2. `css_selectors_multilang.test` runs in comparable time on both DuckDB lines, and
   the repo's v2.0 canary drops `skip_tests`.
3. **Every existing selector test passes unchanged.** The suite is the spec: 7138
   assertions / 133 cases as of v1.15.4, including `ast_select_pseudo_classes`,
   `css_selectors`, `css_selectors_multilang`, `ast_selector_for`.
4. The error messages stay. The no-silent-empty work (#89, #128, #184) is behavioural
   contract, not incidental: malformed selectors raise and name their replacement;
   well-formed selectors that match nothing return nothing.

## Risks

- **Semantics drift during port.** Mitigation: the suite is the spec and must pass
  unchanged; do not "fix" behaviour in the same change.
- **Scope creep into a semantics rewrite.** The selector language is settled; this is
  a planning-path change.
- **Two implementations in flight.** If the port lands incrementally, both paths must
  agree — an `EXCEPT`-both-ways differential over the fixture corpus is the cheap
  check (the pattern used in #184 to prove `:scope` and `::scope` identical).
