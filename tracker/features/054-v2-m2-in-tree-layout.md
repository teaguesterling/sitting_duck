# 054 — M2: the in-tree `core/` + `languages/<lang>/` layout

**Status:** stages 0–2 EXECUTED 2026-11-03. Stage 3 deliberately not started.
Written 2026-11-03 as a plan; §10 records what executing it measured.

Teague approved "stages 0–2, stop before the macro split".

| stage | state | landed as |
|---|---|---|
| 0 — the invariance gate | merged | PR #306, `8cc5827` |
| 1 — `git mv` to `languages/<lang>/` | open | PR #307, `8532350` `7e5fa17` `b96e588` |
| 2 — `module.cmake` per language | open | PR #307, `525e083` `6037b17` |
| 3 — the macro split | **not started, by decision** | — |

**Scope source:** `docs/planning/v2-architecture.md` "Migration sequencing", M2.
`tracker/features/044-v2-m1-contract.md:262` makes M2 the next action.
`tracker/features/048-v2-work-register.md` row 13 is the register entry.

**Measured on:** `6003d2d` (main), DuckDB submodule `d54cbbe3` = v2.0.0,
`build/release/duckdb` reporting `v2.0.0-dev88609 ... d54cbbe3df`.

---

## 1. What the RFC asks for, and what is already done

The RFC defines M2 as four pieces. One of them is already complete.

| Piece | State | Measured how |
|---|---|---|
| `core/` + `languages/<lang>/` directories | **not started** | no such directory in the tree |
| grammar submodule refs replace vendored copies | **DONE** | 27 `grammars/tree-sitter-*` submodules in `.gitmodules`; zero `third_party/tree-sitter-*` directories; `CMakeLists.txt:36` sets `PARSER_SOURCE_DIR` to `${CMAKE_CURRENT_SOURCE_DIR}/grammars` |
| `module.cmake` per language | **not started** | all 28 declared centrally in `cmake/BuiltinLanguages.cmake` |
| macro split (core accessors vs add-ons) | **not started** | 13 files, 7369 lines, in `src/sql_macros/` |

So M2 is three pieces, not four. This is the second time the register has been
measured smaller than it reads: see `048`'s row 1.

## 2. The safety net the RFC names does not exist

`044:22` says:

> Safety net for later (M2): the #92 invariance test — default build
> byte-equivalent, canonical registration order — must stay green throughout.

**Nothing asserts either property.** Measured three ways:

1. `gh pr view 92 --json files` lists 8 files. None is a test.
   `scripts/test_language_subset.sh` is in that set, and it is a subset-build
   smoke test: it checks that a subset build lists exactly its subset, that a
   compiled-in language parses, and that a compiled-out language fails like an
   unknown one. It does not compare a default build against anything.
2. A content grep for `registration order`, `registration_order`,
   `byte.?equivalen` and `canonical order` across `test/`, `.github/` and
   `scripts/` returns nothing.
3. A filename grep for `invarian`, `byte_equiv`, `registration_order` over all
   tracked files returns nothing.

A gate that nothing asserts cannot "stay green". It was never red.

This is the whole reason stage 0 below exists. Moving 28 languages with no
oracle produces "no difference", which is the hardest result to doubt.

### 2a. "Canonical registration order" is not observable at run time

`ast_supported_languages()` does **not** return declaration order. Measured on
`build/release/duckdb`, same 28 languages, different sequence:

```
declaration order (build/release/.../sitting_duck_builtin_languages.def):
  python javascript cpp typescript sql duckdb go ruby markdown java php html
  css c rust json bash swift r kotlin csharp lua hcl graphql toml zig dart yaml

ast_supported_languages():
  graphql lua kotlin bash zig json csharp rust toml c python javascript html r
  swift cpp dart sql typescript ruby java hcl php duckdb go markdown yaml css
```

`diff` of the two sorted lists is empty, so the SET matches and only the ORDER
differs. The runtime order looks like hash-map iteration order.

**Consequence for the plan:** the oracle for registration order is the
generated `sitting_duck_builtin_languages.def`, not any SQL function. A test
that reads order from `ast_supported_languages()` would pin a hash order and
pass regardless of what the move did to registration.

## 3. The before-oracle, captured 2026-11-03

Captured from the current build before any move. These are the bytes the move
must not change.

| Artifact | Size | sha256 (first 12) |
|---|---|---|
| `builtin_languages.def` (generated, registration order) | 28 languages | `a274491266 89` |
| `declaration_order.txt` | 28 lines | `b86aeb0cd7f2` |
| `supported_languages_order.txt` | 28 lines | `bf26c4970b4b` |
| `type_map_all.txt` (every language × node type) | 3911 rows | `dca09978a804` |
| conformance kit matrix | pending — run in progress | — |

`ast_type_map()` accepts only literal parameters, so the type-map dump is 28
separate calls unioned, not a lateral join:

```
Binder Error: Table function "ast_type_map" does not support lateral join
column parameters - cannot use column "l.language" in this context.
```

**"Byte-equivalent" means these artifacts, not the `.duckdb_extension`
binary.** The binary cannot stay byte-identical across a `git mv`, because
`__FILE__` strings and debug-info paths carry the source path. Writing the
acceptance line as "binary byte-equivalent" would fail on the first move.

## 4. What moves, measured

28 languages, a clean three-way mapping, with three irregularities.

| Language | adapter | `.def` | grammar submodule |
|---|---|---|---|
| 25 of 28 | `<lang>_adapter.cpp` | `<lang>_types.def` | `grammars/tree-sitter-<lang>` |
| `sql` | `sql_adapter.cpp` | **none** — its `DEF_TYPE` table is inlined in the adapter | `tree-sitter-sql` |
| `duckdb` | `duckdb_adapter.cpp` | **none** | **none** — native DuckDB parser, not tree-sitter |
| `csharp` | `csharp_adapter.cpp` | `csharp_types.def` | `tree-sitter-c-sharp` — **name mismatch** |

The two seams `044:262` names stay where they are in this plan:
`src/include/language_adapter.hpp` (788 lines: the 10-virtual interface plus 27
concrete adapter classes plus `DynamicLanguageAdapter`) and
`src/include/native_context_extraction.hpp` (679 lines: dispatcher plus
per-language rules). Splitting those is a header refactor with real behaviour
risk. It is **not** in this plan; see §7.

## 5. The move forces test changes — a Golden Rule coordination point

6 test files, CI and 13 scripts hardcode `src/language_configs/`,
`src/language_adapters/` or `grammars/`.

| Consumer | Refs | Why it matters |
|---|---|---|
| `cmake/BuiltinLanguages.cmake` | 28 | the declaration table itself |
| `CMakeLists.txt` | 12 | `PARSER_SOURCE_DIR`, yaml's special-cased dir |
| `scripts/conformance_kit.sh` | 3 | **the safety net's own input** |
| `scripts/verify_module_abi.py` | 3 | the ABI 1.0.0 CI gate |
| `scripts/check_def_type_tables.py` | 5 | CI gate |
| `scripts/audit_leaf_text_gaps.py` | 4 | 047's scanner |
| `scripts/generate_def_file.py` | 2 | codegen |
| 6 further scripts | 6 | one-off audits |
| `.github/workflows/MainDistributionPipeline.yml` | 3 | CI |
| `test/sql/dynamic_languages/register_language*.test` | 15 | 3 files, `register_language()` paths |
| `test/sql/conformance/capability_declarations.test` | 3 | |
| `test/sql/conformance/builtin_set_under_runtime_grammars.test` | 1 | |
| `test/sql/bugs/issue_220_sql_native_parameters.test` | 1 | |

**The conformance kit is the sharpest case.** Its `DEFAULT_DEF_SOURCES` globs
`src/language_configs/*_types.def` and `src/language_adapters/*_adapter.cpp`.
After the move those globs match zero files, and the kit reports `UNDECL`
rather than `FAIL` — it goes blind and stays green. The kit's own header line
(`def scan: 3837 DEF_TYPE entries from 59 files, 31 languages`) is the thing to
assert, not just its exit code.

The Golden Rule says change implementation or tests, never both at once. This
move cannot obey that literally: a `git mv` that leaves the test paths stale
breaks the suite by construction. So the plan states what each change is for,
and separates them into different commits in the same PR:

- commit A: pure `git mv`, no content edit, nothing else
- commit B: path updates in scripts, cmake and CI
- commit C: path updates in the 6 test files

Commit A alone is expected to go **red**. That is the point: it proves the
paths were load-bearing. Commit C is the only commit that touches `test/`.

## 6. M3 constrains the commit shape

M3 extracts `duckling` and `languages` with `git filter-repo`, keeping
per-directory history. That only works if the renames are detectable, so every
move must be a pure `git mv` commit with no content edit mixed in. Moving the
grammar submodules also rewrites `.gitmodules`, so CI checkout and the registry
descriptor build must be re-measured against the new paths.

## 7. Stages, and where to stop

Each stage is separately shippable and separately reversible.

| Stage | Content | Risk | Test files touched |
|---|---|---|---|
| **0** | the invariance test: assert the §3 artifacts, and assert the kit's `def scan` header is non-zero | none — test only, no product change | 1 new |
| **1** | `git mv` to `core/` + `languages/<lang>/`; update the 13 scripts, cmake, CI and 6 tests | mechanical, but wide | 6 |
| **2** | `module.cmake` per language; `BuiltinLanguages.cmake` becomes an includer | mechanical | 0 |
| **3** | macro split: core accessors vs add-ons | **changes what registers — user-visible** | unknown |

**Recommended stop: after stage 2.** Stage 3 is the least-defined piece of M2.
"Core accessors vs add-ons" is not written down anywhere as a list, it changes
which macros a default build registers, and that is visible to every user of
the extension. It deserves its own decision and its own plan.

Stages 0–2 are what `v2.0.0-alpha` needs in order to mean "the layout is the
v2 layout".

## 8. What this plan does NOT do

- It does not split `language_adapter.hpp` or
  `native_context_extraction.hpp` (§4). Every v1.17.0 name-binding fix landed
  in the second file, so that seam is where the active work is, and moving it
  during the layout change would mix a file move with a behaviour risk.
- It does not do the macro split (§7, stage 3).
- It does not touch the DuckDB pin, `bugs/040`, or `#160`. M2 is an
  architecture split on the shipped line.
- It does not re-cut `048`'s near-term order. That is still open from
  2026-11-03.

## 9. Unknowns, stated

- **The kit's runtime on the v2.0 line is unmeasured.** The `ok=192 FAIL=0`
  baseline in `044` was taken at `cee95ef`, a v1.5.6 tree. The run started for
  this plan passed 15 minutes without finishing. `bugs/040` measures parse on
  the v2.0 line at 15–23× the v1.5.6 cost, so a kit that used to be quick may
  now be a long job. If so, stage 0's test cannot be a per-PR gate and must
  run on a schedule.
- **Whether `yaml` survives the move cleanly.** `CMakeLists.txt:297`
  special-cases `SD_YAML_PARSER_DIR`, and `#233` records that yaml's grammar
  cannot be regenerated under tree-sitter 0.26.
- **Whether the registry descriptor build reads any moved path.** Not yet
  measured.

---

## 10. What executing the plan measured, 2026-11-03

### 10a. Where the plan was wrong

| the plan said | measured |
|---|---|
| §5: "6 test files" need changes | **zero functional changes.** The `register_language*.test` files point at `grammars/`, which did not move. Two comment-only edits, in commit C |
| §1: M2 is four pieces | three. The grammar-submodule piece was already done |
| §2: nothing asserts the invariants | confirmed, three ways — and that is why stage 0 exists |

An over-estimate that quietly turns out fine is how a stated exception becomes a
habit, so the test-file figure is recorded rather than dropped.

### 10b. The gate caught the move twice, and both times by failing loudly

Stage 0's whole purpose was to make a silent change loud. It did, twice, and
neither was found by a content grep:

1. **Stage 1 — `scripts/verify_native_dispatch.py`.** It builds its path from
   components, so the literal `src/language_adapters` never appears in the file
   and the path inventory missed it. It exited 1 with *"no `*_adapter.cpp`
   found; this check is looking in the wrong place"*. Its non-vacuity guard is
   the only reason that was not a silent hole.
2. **Stage 2 — `scripts/verify_layout_invariance.py`.** Moving the declarations
   out of `cmake/BuiltinLanguages.cmake` left its regex matching nothing. Exit
   1: *"matched 0 `sitting_duck_language()` declarations ...; the pattern is
   wrong, not the code"*.

And one consumer failed **silently**, which is the shape worth remembering:
`docs/scripts/generate_language_reference.py` returned `[]` and carried on.
That list derives the not-built-in warning box, so an empty list DELETES the
box and the page looks finished — #235 exactly. It raises now, and both shapes
are proven to raise (list missing; list present but empty), with a control
returning 28.

### 10c. Stage 2's shape, and why the order list is written out

`cmake/BuiltinLanguages.cmake` keeps only ORDER. Each language declares itself
in `languages/<name>/module.cmake`, with its adapter path relative to **itself**
— measured: inside a cmake `macro()`, `CMAKE_CURRENT_LIST_DIR` is the included
module's own directory. That is what keeps the directory self-describing when
M3 extracts `languages/`.

The list is written out, not globbed. `file(GLOB)` returns directories sorted
alphabetically, which is not registration order, and **that reorder would be
invisible**: every language still present, so every count and every set
comparison still passes, and `ast_supported_languages()` cannot see it either
(§2a). The generated `.def` is the only observable record.

The includer checks each module instead of trusting the list. Five faults
planted against a skeleton tree, all five failing the configure, plus a control
that configures cleanly:

| planted fault | detected |
|---|---|
| module declares nothing | yes |
| module declares the wrong name | yes |
| listed module missing | yes |
| adapter file missing | yes |
| module declares two languages | yes |
| *control: no fault planted* | *configures cleanly* |

The gate repeats the name check statically (check 1b), because the list is an
INDEX and a module is the artifact: a module declaring a different language
would leave the index intact and every count unchanged, and the build would
register the wrong set in the right-looking order.

### 10d. The acceptance oracle, re-measured after each stage

| artifact | stage 1 | stage 2 |
|---|---|---|
| generated `builtin_languages.def` | `a27449126689c8ff` | `a27449126689c8ff` |
| `ast_type_map()` all languages, 3911 rows | `dca09978a8040157` | `dca09978a8040157` |
| `ast_supported_languages()` | 28 | 28 |
| build | 0 errors | 0 errors, 104 warnings |
| generated language reference | 30 pages identical but for 2 intended strings | all 31 identical |
| four static gates + self-tests | green | green |
| conformance kit | `ok=192 FAIL=0 dirty=1 void=7 UNDECL=5 n/a=19`, 8/8 controls, exit 0 | — |

Both sha256 figures match §3's before-oracle, captured before anything moved.

### 10e. The kit's denominator moved by exactly one file, and had to be explained

`def scan: 3837 DEF_TYPE entries from 58 files, 31 languages` — 58, where it was
59. `dynamic_language_adapter.cpp` stayed on the engine side and left the glob's
scope. It holds zero `DEF_TYPE` entries, which is why entries (4039 by the
gate's own count) and readable tables (31) did not move. The floor in
`spec/layout/layout.yaml` forced that one-file drop to be explained rather than
absorbed, which is the whole point of a floor.

### 10f. Still open

- **Stage 3 (the macro split) is not started**, by decision. §7 says why: "core
  accessors vs add-ons" is not written down anywhere as a list, and it changes
  which macros a default build registers, which is visible to every user.
- §9's unknowns about the registry descriptor build are still unmeasured.
- The kit's runtime on the v2.0 line: the clean run completed, so the "may now
  be a long job" worry in §9 is answered for the in-tree case.
