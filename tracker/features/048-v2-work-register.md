# 048 — v2.0 work register: the explicit remaining pieces

**Status:** Living index. Created 2026-10-07 from a roadmap review with Teague.

`docs/planning/v2-architecture.md` holds the architecture and the M1→M4 sequencing.
That RFC is organised by *milestone*; this register is organised by *piece of work*,
because the pieces do not map one-to-one onto milestones and several have a v1.x half
that can land long before the repo split.

Its job is to be the answer to "what is actually left, and what can we do today?"

## The register

**Updated 2026-10-08:** rows 1, 2 and 10 landed on 2026-10-07 (PRs #199, #198, #195),
i.e. the first three items of the recommended order below are done. The order has been
re-cut accordingly; the original ordering rationale is preserved beneath it.

| # | Piece | Tracked in | Milestone | v1.x-doable now? |
|---|---|---|---|---|
| 1 | **Unparse: leaf-text coverage** (4a) | `047` | M4 cap. 3 | **LANDED** PR #199 — 134 of 348 grammar-derived gaps closed; 214 sql `keyword_*` outstanding |
| 2 | **Unparse: byte offsets under `source := 'full'`** | `047`, RFC substrate note | M1 contract | **LANDED** PR #198 — `start_byte`/`end_byte`, 0-based half-open |
| 3 | **Unparse: node templates** (4b) | `047` | M4 cap. 3 | Partly — design now, needs #2 first |
| 4 | **`COPY … TO (FORMAT ast)`** (level 5) | `045`, #174 | M4 cap. 3 | After #1 |
| 5 | **`ast_select` → C++** | `046`, #160, #117, #164 | independent | Yes, but do #6 first — see the #160 measurement below |
| 6 | **Call-graph rewrite** (seeded join) | `043` Part A | independent | **Yes — SQL only, fixes #160 + #164** |
| 7 | **Transitive call closure** (`:reaches`) | `043` Part B | M4 | Deferred — C++ or depth-capped SQL |
| 8 | **Cross-module name resolver** | `043` Part C | M4 | No — the epic; enables precise everything |
| 9 | **Module ABI** | `044` | M1 | Partly — can be specified against current layout |
| 10 | **Taxonomy spec → codegen** | `044` first slice | M1 | **LANDED** PR #195 — `spec/taxonomy/taxonomy.yaml` is now the source of truth; CI job `taxonomy-tables-sync` gates it |
| 11 | **Conformance kit** | `044`, generalises #91 | M1 | **Yes — pre-emptive tests, no product risk** |
| 12 | **Trust-boundary doc + merge #80** | `044` | M1 | Yes — #80 is the runtime `.so` door |
| 13 | **In-tree `core/` + `languages/<lang>/`** | RFC M2 | M2 → alpha | No — gated on #9 stabilising |
| 14 | **Repo split** (duckling / languages / proper) | RFC M3, #87, #71 | M3 → v2.0.0 | No — split-after-stability rule |
| 15 | **Ad-hoc language creation** (`create_parser`, WASM) | RFC M4 cap. 1–2, #80 | M4 | Partly — #80 now, WASM later |
| 16 | **Incremental reparse** | RFC M4 cap. 4 | M4 | No |
| 17 | **`ast_to_blocks` / duck_blocks interop** | RFC M4 cap. 6 | M4 | **Yes — pure producer, spec not dependency** |
| 18 | **Selector correctness backlog** | `042`, #139–#191 | ongoing | **Yes — already the active line** |

## How Teague's own four map onto it

Recalled off the top of the head, 2026-10-07: finishing unparse (#1–4), moving
`ast_select` into C++ (#5), refactoring into submodules (#13–14), ad-hoc language
creation (#15). The fifth that would not come to mind is most likely **#8, the
cross-module name resolver** — the piece that turns `:calls` from a global name match
into actual resolution, and the one that reserves `dependencies`/`dependents`.

Runners-up for "the fifth", all genuinely outstanding: **#10/#11** (the M1 contract's
own deliverables, easy to forget because they are infrastructure rather than
features), **#16** (incremental reparse), **#17** (`ast_to_blocks`).

## Why the v1.x column matters

The split-after-stability rule means M3 cannot start until the M1 contract has stopped
moving for a full milestone. That is a *waiting* constraint, not a working one — so the
question "what can we do on v1.x right now" is the whole question for the near term.

Eight of the eighteen pieces have a real v1.x half:

- **#1, #2** — unparse correctness and the substrate under it. Both additive.
  **Both landed 2026-10-07** (PRs #199, #198).
- **#6** — the seeded call-graph join. Pure SQL; retires the global name self-join that
  is simultaneously the imprecision, the #164 OOM, and the #160 bind tax.
- **#10** — taxonomy spec → codegen. Safe *by construction*: the gate was byte-equivalence
  against the then hand-written tables, so it could not change behaviour, and the CI
  check mirrors the existing "Embedded SQL macros header in sync".
  **Landed 2026-10-07** (PR #195): `spec/taxonomy/taxonomy.yaml` is now the source of
  truth and the `taxonomy-tables-sync` CI job enforces it. Note the scope precisely —
  the *semantic-type / flag / strategy-enum tables* are generated into marked blocks;
  the per-language `.def` files still name node types **by hand**.
- **#11** — the conformance kit. Pure test writing against the current layout; it is also
  what makes #13/#14 verifiable later, so writing it early is not speculative.
- **#5** — `ast_select` in C++. Larger. #160's constant per-call overhead is real and felt,
  but measured at ~0.5s on the shipped DuckDB v1.5.x line rather than the ~6s the issue
  claims — the 6s belongs to the DuckDB v2.0 line (see below) — so it argues for
  #6 first, not for jumping straight to C++.
- **#17** — `ast_to_blocks`. A producer of conforming STRUCTs; no LOAD, no dependency.
- **#18** — the selector backlog, already in flight.

## Measured 2026-10-07: #160's magnitude is ~12x smaller than the issue states

Issue #160 says `ast_select_from` "spends ~6 s planning per call regardless of table
size or selector". Measured on v1.15.x (`build/release/duckdb`, `.timer on`):

| table | nodes | selector | run 1 | run 2 |
|---|---|---|---|---|
| small | 795 | `.fn` | 0.461s | 0.486s |
| big | 155,559 | `.fn` | 0.551s | 0.607s |
| small | 795 | `.fn:calls(main)` | 0.490s | — |
| small | 795 | `.fn::callers` | 0.455s | — |

The **shape** in #160 is confirmed and still present: the cost is constant, independent
of both table size (795 vs 155k nodes differ by <0.15s) and selector complexity (plain,
pseudo-class and pseudo-element arms are indistinguishable). That is exactly what 043
Part A predicts — SQL macros always inline, so every arm including `pe_callers`/
`pe_callees` is bound on every call whether referenced or not.

But the **magnitude is ~0.5s, not ~6s** on the shipped DuckDB line.

**Cause (resolved — it is not a mystery): the ~6s is DuckDB-v2.0-line-specific.**
The speculation originally recorded here ("either it improved, or a different build or
selector") was already answered by `042`'s #160 investigation of 2026-09-15, which this
register had not picked up. `042` measured ~0.3–0.4s on DuckDB v1.5.5 and found the
15–20s figures came from a **v2.0-dev submodule build** (`e3946f2327`) — i.e. #160 is a
sibling of `tracker/bugs/040` (the v2.0 planning-cost regression) and reproduces only on
the v2.0 line. The shipped v1.5.x line is fine.

`046` independently corroborates the same split without drawing the conclusion: its own
points note `css_selectors_multilang.test` takes ~40 min on the v2.0 line against
seconds on v1.5.6.

Re-measured 2026-10-08 on `build/release/duckdb` (v1.5.6), 1,246-node table from
`test/data/python/sample_app.py`: `ast_select_from('.fn')` 0.441s then 0.494s,
`ast_select_from('.class .fn')` 0.502s. Consistent with both earlier measurements.

So: **~6s is real but v2.0-line-only; ~0.3–0.5s is the shipped-line cost.** #160 should
be annotated as v2.0-only rather than left as an unqualified 6s claim driving
prioritisation. Any doc quoting ~6s must say which DuckDB line it means.

Consequence for ordering: #6 is still worth doing — 0.5s of pure per-call overhead is
bad for interactive use and compounds badly in loops — but it is not the emergency a
6s figure implies. It does NOT by itself justify jumping #5 (`ast_select` → C++) ahead
of the safer items. Note also that #164's OOM was reported on a 78k-node table with
call-graph pseudo-classes; the measurements above used a 795-node table for the
call-graph arms, so they say nothing about that blowup, which remains unmeasured here.

## Measured 2026-10-08: upstream #26036 is ~5x less severe, and its startup half is FIXED

Re-ran the generator from duckdb/duckdb#26036 on a **quiet** box (load 1.7-3.3), three
builds on the same machine. Net of startup:

| N | SQL size | v1.5.6 | cyanoptera `d4c9dfd469f` | main `688937993a` |
|---|---|---|---|---|
| 400 | 46 KB | 0.09s | 0.35s | 0.57s |
| 800 | 93 KB | **0.19s** | **0.82s** | **1.36s** |

Startup (`SELECT 1`): v1.5.6 0.13s · cyanoptera **0.08s** · main 0.06s.

1. **Scaling regression persists but is much smaller.** At 93 KB cyanoptera is **4.3x**
   v1.5.6, down from the **20.6x** recorded 2026-09-22. Per-doubling cost is 2.3x on
   cyanoptera vs 2.1x on v1.5.6 -- still super-linear, now close to linear.
2. **The startup regression is FIXED on cyanoptera** (2.50s recorded -> 0.08s now,
   faster than v1.5.6). The separate upstream backport request that was pending
   Teague's OK is **no longer needed** -- do not file it.
3. **The "ratio is load-independent" claim in #26036 is wrong**, and it inflated the
   headline. Same main binary, same SQL, only load differs: **19.0x at load ~26 vs
   7.2x at load ~1.7**. The v2 line degrades disproportionately under contention, so
   the original 19-20x figures are an upper bound under load, not a fixed cost.
   #26036 was updated upstream with this correction on 2026-10-08.

**The canary stays off** (Teague, 2026-10-08). 4.3x on the flat-CASE repro, but the
real `css_selectors` case is recorded as ~3x worse per KB than that repro predicts
(~13x on the actual 52 KB statement), and CI runners have 2-4 cores against this box's
36. The `skip_tests: true` gate in `.github/workflows/MainDistributionPipeline.yml`
is unchanged.

**Note on provenance:** the 2026-09-22 three-way measurement this corrects lives only
in UNCOMMITTED working-copy edits to `043` in the shared checkout (~115 lines). It is
not on `main` and would be lost if that checkout were cleaned. Worth committing.

### Addendum 2026-10-08: the ~0.5s figure is v1.5.x-line-only; the v2.0 line is ~12.8s

Both halves of #160's story are now measured on one sitting_duck source (`6f82c98`,
all 27 languages), with v1.5.6 (`069cc9f9b5b`) and cyanoptera (`d4c9dfd469f`) built
side by side. Full record, every run, with load conditions: **`bugs/040`,
"RE-MEASUREMENT 2026-10-08"**.

- The ~0.5s above is **reproduced independently** (0.44–0.61s), from a separate build.
- On the **v2.0 line the same calls cost ~12.8s** — a ~27x penalty, and ~2x *worse*
  than the ~6s #160 claims rather than 12x better. So neither number in #160's thread
  is wrong; they are measuring different DuckDB lines, and #160 should say so.
- The constant-cost shape holds on both lines and is starker on v2.0: 303 nodes and
  156,169 nodes differ by 0.1s.
- `taskset -c 0,1` changes the v2.0 cost by under 2% — the expensive work is
  single-threaded binding, so neither a wider dev box nor a narrow CI runner moves it.

Consequence for the ordering above: the "it is not the emergency a 6s figure implies"
conclusion **holds for the v1.5.x line we ship on, and is false for the v2.0 line.**
Crucially, upstream duckdb/duckdb#26036 improved ~5x over the same three weeks and
**none of that reached this path** (`040` Result 3), so #5/#6 can no longer be
deferred on the expectation of an upstream fix landing for us.

## Recommended order (near term)

**Read the #160 addendum below before quoting these numbers.** The improvement above
is real for #26036's *own* generator and did **not** reach the `ast_select` macro
path; the measured three-way for that path is in the addendum and in `tracker/bugs/040`.

- `040`'s 2060s is **CURRENT, not stale**, and remains ship-blocking.
- **#160 on the v2.0 line is ~12.8s -- 2x WORSE than the ~6s the issue claims**, not
  better. The ~0.5s figure below is the v1.5.6 cost. Both numbers in that thread are
  right, about different DuckDB lines.
- Since #26036 improved and this path did not, the `ast_select` pathology is probably a
  *different* cost centre than statement-size super-linearity -- more likely the 28
  cross-products blowup `040` names. Hypothesis, not measurement.

## Recommended order (near term) — re-cut 2026-10-08

Steps 1–3 of the original order (#10, #1, #2) **landed on 2026-10-07**. What remains:

1. **#6 seeded call-graph join.** Highest felt-pain-per-line on v1.x. Pure SQL; retires
   the global name self-join that is simultaneously the imprecision, the #164 OOM, and
   the #160 bind tax.
2. **#3 unparse node templates (4b).** Now unblocked on both counts — the byte-exact-vs-
   normalized question was settled 2026-10-07 and the substrate (#2) is in. See `047`.
3. **#11 conformance kit**, continuously — it is the safety net for M2/M3.
4. **#4 `COPY … TO (FORMAT ast)`.** Gated on #1, which has landed, so it is now
   available to start — but it writes lossy output to disk, so it wants 4b's fidelity
   first in practice.
5. **#1 remainder:** sql's 214 `keyword_*` leaves, deferred for a per-keyword semantic
   classification pass; and #197, the `duckdb` adapter's unrelated `children_count`
   bug which breaks its unparse entirely.

<details><summary>Original order as written 2026-10-07 (steps 1–3 now done)</summary>

1. **#10 taxonomy spec → codegen.** Safest possible first move: provable, no behaviour
   change, retires a class of drift bug that has already bitten. Also the declared
   first slice of M1, so it advances the gate on everything downstream.
2. **#1 unparse leaf-text coverage.** A correctness bug (`comment` unparses to the
   literal word `comment`), independent, and must be derived from the grammars rather
   than hand-listed — same lesson as #184's zero-arity list.
3. **#2 byte offsets under `source := 'full'`.** Small and additive; turns exact slicing
   into `substring()` and unblocks #3 properly rather than approximately.
4. **#6 seeded call-graph join.** Highest felt-pain-per-line on v1.x.
5. **#11 conformance kit**, continuously — it is the safety net for M2/M3.

</details>

#8 (resolver) is the biggest prize and the right thing to *design* in parallel, but it
is C++ epic work and should not be started while #9 (module ABI) is still unspecified —
the resolver is exactly the kind of consumer whose needs should shape that ABI.

## Related

- `docs/planning/v2-architecture.md` — architecture, milestones, the `write_ast` laws
- `docs/development/duckdb-version-compatibility.md` — the two-DuckDB-line situation
- `043`, `044`, `045`, `046`, `047` — the per-piece specs
- `042` — selectors correctness plan

## What is authoritative where

Added 2026-10-08 after a documentation-consistency audit, so the next person does not
have to re-derive this. Four PRs landed on 2026-10-07 (#194, #195, #198, #199) and the
docs were written at different moments; when two docs disagree, the file named here
wins and the other one is stale.

| Question | Authoritative source | Notes |
|---|---|---|
| The `write_ast` laws | `docs/planning/v2-architecture.md` (§ capability 3) | Settled 2026-10-07. Byte-exact **only** at `source := 'full'`; **pseudo-inverse only** below it, where `write_ast(read_ast(x)) = x` is explicitly NOT required. `language :=` is an **override**, inferred from the `language` column. `047` restates it; anything stating a *universal* structural law predates the ruling. |
| What each `source :=` level provides | `src/include/ast_type.hpp` (`enum class SourceLevel`) + `src/unified_ast_backend.cpp` (`GetFlatDynamicTableColumnNames`) | `'full'` adds **four** columns since PR #198: `start_column`, `end_column`, `start_byte`, `end_byte`. Columns are **1-based**; bytes are **0-based, half-open**. `API_REFERENCE.md` is the best prose rendering of this. |
| Semantic types, node flags, extraction-strategy enums | `spec/taxonomy/taxonomy.yaml` | Generated by `scripts/generate_taxonomy.py` into marked blocks in five sources (`src/include/semantic_types.hpp`, `src/semantic_types.cpp`, `src/include/node_config.hpp`, `src/language_config_json.cpp`, `src/ast_type_map_function.cpp`); CI job `taxonomy-tables-sync` fails on drift. **Nothing in the build regenerates them** — regenerate and commit by hand. **Scope caveat:** the per-language `src/language_configs/*.def` files still map node-type names **by hand**; only the taxonomy tables are generated. |
| Which languages are built in | `cmake/BuiltinLanguages.cmake` (`sitting_duck_language(...)` declarations) | **27 total = 26 tree-sitter + the native `duckdb` adapter.** Registration is generated from this file into `sitting_duck_builtin_languages.def`; `src/language_adapter_registry_init.cpp` is pure X-macro expansion and is **not** hand-edited. Runtime check: `SELECT language FROM ast_supported_languages()`. |
| Unparse fidelity, today | `047` | 4a landed for **25 of 26** tree-sitter languages (PR #199). Still broken: sql's 214 `keyword_*` leaves, and the native `duckdb` adapter (#197). **Byte-identity under `source := 'full'` does not hold today** for any of the 26 corpus languages — it fails on inter-token whitespace. That is what 4b closes. |
| #160's magnitude | the measurement table above, plus `042`'s 2026-09-15 investigation | ~6s is **DuckDB-v2.0-line only** (sibling of `bugs/040`); the shipped v1.5.x line costs ~0.3–0.5s. Quote a line with the number or do not quote the number. |
| DuckDB version compatibility | `docs/development/duckdb-version-compatibility.md` | The two-line situation. |
