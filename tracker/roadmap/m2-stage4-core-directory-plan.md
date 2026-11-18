# 060 — v2 M2 stage 4: the `core/` directory

**Status:** APPROVED and EXECUTED. Teague approved the plan and Option A on
2026-11-17. Executed 2026-11-18 as `ad15d50` and `30c4f88`; awaiting CI and
review. Written 2026-11-17, after `054` §11 measured stage 4 as the one M2
piece that never landed.

**Read §12 onward first if you want the corrections.** Sections 1–11 are the
plan as approved. Sections 12–16 record where the plan was wrong: the oracle
was invalid as specified (§12), the blast radius was three times what §10b
claimed (§13), and two further reference forms were found only after the move
was committed (§15).

**Depends on:** `tracker/features/054-v2-m2-in-tree-layout.md` §11 (the
re-measurement), `docs/planning/v2-architecture.md` (the M1–M4 sequencing),
`spec/layout/layout.yaml` (the manifest the layout gate reads).

**Gate:** CLAUDE.md forbids a large refactor without explicit instruction, and
requires an approved plan before the first line of code. This document is that
plan. It ends in one decision that is Teague's, not mine.

---

## 1. What stage 4 is, and what it is not

M2's target layout, from the RFC, has two halves:

  - `languages/<lang>/` — everything specific to one language. **Done**, in
    stages 1 and 2.
  - `core/` — the engine, with no knowledge of any single language. **Never
    started.**

Stage 4 creates `core/` and moves the engine into it. It is a **move**, not a
rewrite. No function changes behaviour. The success condition is that the built
extension is byte-equivalent, or that every difference is explained.

Stage 4 is **not** the repository split. That is M3, and the split-after-stability
rule governs it: M3 waits until the M1 contract has not broken ABI for a full
milestone. `spec/abi/module-abi.yaml` has changed 8 times since 2026-10-30 and
read `version: 1.0.0` at every one, but no `v2.0.0-alpha` tag exists yet, so
"a full milestone" still has only one end. Stage 4 is what closes M2 and makes
that tag possible.

---

## 2. Measured inventory of `src/`

Measured 2026-11-17 against `origin/main` at `2e61132`, with
`git ls-tree -r --name-only origin/main -- src/`. **110 files.**

| Group | Count | Where it goes |
|---|---:|---|
| `src/include/*_native_extractors.hpp` — per-language | **20** | **the decision, §4** |
| `src/include/` — other headers | 33 | `core/include/` |
| `src/*.cpp` — loose engine sources | 27 | `core/` |
| `src/sql_macros/` | 26 | `core/sql_macros/`, split by `side` |
| `src/language_adapters/` | 2 | one engine, one orphan — §3 |
| `src/language_configs/unparse/universal_unparse.def` | 1 | `core/` |
| `src/CLEANUP_SUMMARY.md` | 1 | delete or move to `docs/` |

The four groups sum to 110. The count is the denominator for every later claim
in this document.

---

## 3. The classification, and the two leftovers

`src/language_adapters/` holds two files, and they are not the same kind of thing:

  - `dynamic_language_adapter.cpp` — engine. It is the runtime-registration path,
    language-agnostic, and belongs in `core/`.
  - `duckdb_adapter_backup.cpp` — **orphan.** Measured 2026-11-17: no CMake file
    references it. It is dead weight that the move would otherwise carry into the
    new layout. Delete it in this stage, as its own commit, before the move.

`src/CLEANUP_SUMMARY.md` is a stray document inside a source directory. It is not
engine code and should not land in `core/`.

The SQL macros need no new judgment. `spec/layout/layout.yaml` already records a
`side` for each of the 14 `.sql` files — **6 `core`, 8 `addon`** — and
`scripts/verify_macro_boundary.py` enforces that a `core` macro never reaches an
`addon` one. Stage 4 changes where those files sit. It does not re-open the
boundary, which `057` settled and CI gates.

---

## 4. THE DECISION: the 20 per-language native extractor headers

This is the only judgment in the plan that is not mine to make.

**What is measured.** All 20 `*_native_extractors.hpp` files sit in
`src/include/`. **Zero** live under `languages/`. A `languages/<lang>/` directory
today holds exactly `<lang>_adapter.cpp`, `<lang>_types.def`,
`<lang>_unparse.def` and `module.cmake`. So stage 1 moved the per-language
*adapters* and left the per-language *extractors* behind.

**Why it is not obvious.** By the M2 rule — language-specific code lives in
`languages/<lang>/` — all 20 belong there. Two measured facts push back:

  1. `src/include/native_context_extraction.hpp` is an engine header that
     **includes** the per-language natives. It is the coupling point. Moving the
     20 means changing that hub, which is engine surgery, not a move.
  2. The set is ragged. **12 language directories have no native extractor header
     at all**: `duckdb`, `fsharp`, `graphql`, `haskell`, `hcl`, `json`, `julia`,
     `markdown`, `scala`, `toml`, `tsx`, `yaml`. Four of those (`fsharp`,
     `haskell`, `julia`, `scala`) are not built at all. So the move would make
     `languages/` *less* uniform, not more, until the gaps are understood.

**Option A — engine only (my default).** Move the 33 engine headers, the 27
sources and the macros. Leave the 20 natives in place, in what becomes
`core/include/`. Stage 4 stays a pure move with a byte-equivalence oracle.
File the natives as their own follow-up, with the include hub as its subject.
Cost: `core/` knowingly holds 20 language-specific headers, so M2's "no language
knowledge in the engine" claim is not yet literally true, and the tracker must
say so rather than claim a clean finish.

**Option B — natives too.** Move all 20 into `languages/<lang>/` in the same
stage and rework `native_context_extraction.hpp` into a generated or
per-language include. Cost: it mixes a move with an engine change, so a failure
cannot be isolated to either — the exact thing the byte-equivalence oracle
exists to prevent. It is also a much bigger diff and needs its own test plan.

**My recommendation: A.** It keeps the oracle meaningful. B's include-hub rework
deserves to fail on its own evidence, not inside a 110-file move.

**If you do not answer, I do nothing.** No files move without your word.

---

## 5. Sequencing constraints, measured

Two pieces of live work touch files that stage 4 moves. Each must land or be
dropped first, or the move turns them into rebase pain.

| Work | State 2026-11-17 | Collides with |
|---|---|---|
| `fix/061-duckdb-adapter-file-path` | 2 commits, local only, never pushed, 46 behind main | the duckdb adapter path |
| `fcd5003` (parked WIP) | local only, on `fix/323-kotlin-class-dispatch` | `src/include/kotlin_native_extractors.hpp`, `spec/abi/module-abi.yaml` |

`fcd5003` collides only under Option B. Under Option A the natives do not move,
so only `fix/061` must be settled first.

---

## 6. Staged execution, once approved

Each step is its own commit, and each ends with a green gate. The order exists so
that a failure names its own cause.

  1. **Delete the orphan.** Remove `src/language_adapters/duckdb_adapter_backup.cpp`
     and its header. Positive evidence required: a build before and after, and a
     grep over every CMake file showing no reference. A deletion that breaks
     nothing must be *shown* to break nothing, because a passing build is also
     what an unreferenced file produces.
  2. **Record the oracle.** Build `origin/main` and keep the artifact's hash and
     the registered-function list. This is the before half. Without it the move
     has no oracle, and a same-twice reading would be indistinguishable from
     success.
  3. **Create `core/` and move the engine**, with `git mv` only, in one commit per
     group: headers, then sources, then macros. No content edits in these commits.
  4. **Fix the include paths**, as a separate commit, so that step 3 stays a pure
     rename in the history.
  5. **Update the manifest.** `spec/layout/layout.yaml` gains the `core/` paths.
     The layout gate reads this file, so it changes with the tree, not after it.
  6. **Re-run the oracle.** Compare the built artifact and the registered-function
     list against step 2. Every difference is explained in the PR body or the
     move is wrong.
  7. **Update `054` and `048`** to record stage 4 as merged, in the same PR as the
     move. The lesson from `054` §11a is that tracker status not updated at merge
     time is what produced four stale status lines.

---

## 7. What the oracle is, and what it is not

A green CI run is not the oracle. The gates prove the tree matches the manifest;
they do not prove the extension behaves identically.

The oracle has two halves, and both need a non-trivial baseline:

  - **Artifact equivalence.** The built extension before and after. A move should
    change nothing a user can observe.
  - **Registered surface.** The full list of registered functions and macros,
    before and after, compared as sets.

A same-twice or stale binary fails as "no difference", which is the hardest wrong
answer to doubt. So step 2 records the before half from a *separately built*
`origin/main`, and the comparison states which build produced each side.

---

## 8. Out of scope

  - **M3, the repository split.** Stage 4 closes M2 and makes `v2.0.0-alpha`
    possible. It does not move anything to another repository.
  - **The macro core/addon boundary.** Settled in `057`, gated in CI.
  - **Any behaviour change.** If the move needs a behaviour change to work, that
    is a finding to report, not a thing to fold in.
  - **The 059 bug phases** (`#317`, `#324`, `#342`, `#316`, `#322`, `#255`,
    `#321`). Independent of layout.

---

## 9. Open questions

  1. **§4, the 20 native extractor headers.** Option A or B. My default is A.
  2. Should `src/CLEANUP_SUMMARY.md` be deleted, or moved under `docs/`?
  3. Is `fix/061-duckdb-adapter-file-path` still wanted? It is 2 unpushed
     commits and it blocks step 3 under either option.

---

## 10. Measured blast radius (added 2026-11-17, after the plan was approved)

Measured against `origin/main` at `2e61132`. This section replaces the guesswork
in §6 with the actual list of edits. The move is far smaller than the file count
suggests.

### 10a. What needs no change at all

  - **Every `#include` of an engine header.** They are written by bare filename,
    `#include "language_adapter.hpp"`, and resolved through
    `include_directories(src/include)`. Moving the directory and changing that one
    CMake line keeps all of them correct.
  - **The 13 prefixed includes**, `#include "include/semantic_types.hpp"` and
    siblings in 7 files. They resolve relative to the including file. Because the
    move preserves `<dir>/` beside `<dir>/include/`, they still resolve. Measured,
    not assumed: the 13 lines are in `src/ast_semantic_aliases_function.cpp`,
    `ast_source_bytes_function.cpp`, `ast_supported_languages_function.cpp`,
    `ast_type_map_function.cpp`, `semantic_type_functions.cpp`,
    `semantic_type_logical_type.cpp` and `semantic_types.cpp`.

This is why §7's oracle is realistic. If no include changes, the compiler sees the
same translation units and a byte-equivalent artifact is a reasonable expectation
rather than a hope.

### 10b. What must change

| Where | What | Count |
|---|---|---|
| `CMakeLists.txt:243` | `include_directories(src/include)` → `core/include` | 1 line |
| `CMakeLists.txt:313-341` | explicit source paths `src/*.cpp` → `core/*.cpp` | ~27 lines |
| `.github/workflows/MainDistributionPipeline.yml:318,322,323` | the `embedded_sql_macros.hpp` sync gate's paths | 3 lines |
| `.github/workflows/MainDistributionPipeline.yml:447` | the taxonomy sync gate's 5-file path list | 1 line |
| `.github/workflows/MainDistributionPipeline.yml:540` | the macro boundary gate's comment | 1 line |
| `scripts/embed_sql_macros.py` | reads `src/sql_macros/`, writes `src/include/embedded_sql_macros.hpp` | paths |
| `scripts/verify_native_dispatch.py:76` | `NOT_TREE_SITTER` holds `duckdb_adapter_backup.cpp` | 1 entry |
| `scripts/verify_module_abi.py:77` | comment naming `duckdb_adapter_backup.hpp` | 1 comment |
| `spec/layout/layout.yaml:198` | entry for `src/language_adapters/duckdb_adapter_backup.cpp` | 1 entry |
| `cmake/BuiltinLanguages.cmake:18,20` | comments naming `src/` paths | 2 comments |

`CMakeLists.txt` lists its sources **explicitly**, not by glob. That is the single
most important fact for this move: a glob would have moved silently and a missing
file would have surfaced only at link time. An explicit list fails loudly, in
CMake, naming the file.

### 10c. Correction to §3 — the orphan is not free to delete

§3 said no CMake file references `duckdb_adapter_backup.cpp`. That is true and it
is not the whole truth. The file is wired into three live places, found by grepping
the name across all of `origin/main` rather than only the CMake files:

  - `scripts/verify_native_dispatch.py:76` — it is a member of the
    `NOT_TREE_SITTER` exclusion set, so the script expects it to exist.
  - `spec/layout/layout.yaml:198` — a manifest entry, and the layout gate reads
    this file.
  - `scripts/verify_module_abi.py:77` — a comment.

Deleting the two files without updating `layout.yaml` reds the layout gate.
Step 1 of §6 therefore has four parts, not one: delete `*.cpp` and `*.hpp`, remove
the manifest entry, and remove the exclusion-set member. This is the §11a pattern
from `054` again — a file's real dependents are not where its build reference is.

### 10d. Sequencing, resolved 2026-11-17

Teague chose **land `fix/061` first**. It is pushed and open as PR #344, with two
commits and 8 files. The stage-4 branch starts from `main` only after #344 merges,
so the move carries `fix/061`'s content rather than forcing a rebase across four
renames.

---

## 11. Is byte equality a valid oracle? (measured 2026-11-18)

§7 assumes the built extension can be compared byte-for-byte before and after
the move. A file move changes a file's path, so the question is whether the
path reaches the binary. If it does, a CORRECT move fails the oracle.

**Subject measured:** `build/release-v1.5.6-familyc/extension/sitting_duck/sitting_duck.duckdb_extension`
(a release build, 68981910 bytes, 2026-09-29). This is the same build type CI makes.

| Reading | Command | Result |
|---|---|---|
| `src/` path strings | `strings -a | grep -c '/src/\|^src/'` | **0** |
| source basenames | `strings -a | grep -cE '\.cpp|\.hpp'` | 274 |
| form of those names | sample | `ast_type.cpp`, never `src/ast_type.cpp` |
| DWARF sections | `readelf -S | grep '\.debug'` | **none** |
| sections present | `readelf -S` | `.symtab`/`.strtab` only, no debug info |

**Finding:** the move changes no embedded string. The three ways a path could
reach the binary are each absent or path-free:

1. DWARF `DW_AT_comp_dir` and the line-table directory list — **no debug
   sections exist** in a release build.
2. `__FILE__` in assertions and error messages — embedded as **basenames
   only**, and a move keeps the basename.
3. `.symtab` — mangled C++ symbols, which encode namespaces, not directories.

So §7's oracle stands, for a release build. Status: **measured**, on one
binary, not on the two builds the oracle will actually compare.

### 11a. The caveat this exposes

Byte equality is now sensitive to something else: **link order**. The engine's
sources are an explicit list at `CMakeLists.txt:313-341`. If the move reorders
that list, symbol addresses shift and the binary differs although behaviour is
identical — a false red.

**Constraint for step 4:** edit each line's path prefix in place. Do not sort,
regroup, or realphabetise the list. The diff of that hunk must be 27 lines
changed, 0 lines moved.

### 11b. The control the oracle needs

Byte equality has the failure mode that a stale or twice-copied binary reads as
"no difference" — the hardest result to doubt. So the oracle gets a third
build as a discrimination control:

1. Build `main` → hash A.
2. Build the moved tree → hash B. Expect **A == B**.
3. Build the moved tree again with one deliberate one-line behavioural change
   → hash C. Expect **C != A**.

Reading 3 is what proves the comparison can see a difference at all. Without
it, `A == B` is also what a broken build script prints.

---

## 12. Correction to §11: whole-file byte equality is INVALID (measured 2026-11-18)

§11 asked whether the source PATH reaches the binary. That is the wrong
question. The oracle needs to know whether two builds of the same source are
byte-identical. They are not, and the reason is not a path.

**The extension carries a metadata footer that embeds this repo's own commit.**

```
$ tail -c 512 build/release-v1.5.6-familyc/extension/sitting_duck/sitting_duck.duckdb_extension | strings
fea334e
v1.5.6
linux_amd64
$ git log -1 --format=%s fea334e
Merge pull request #181 from teaguesterling/chore/bump-duckdb-v1.5.6
```

`fea334e` is a commit in THIS repository. The moved tree is a different commit,
so a CORRECT move gives a different whole-file hash. Worse, §11b's
discrimination control (C != A) would then pass on the commit stamp alone and
prove nothing about behaviour.

### 12a. The footer's exact size

`extension-ci-tools/scripts/append_extension_metadata.py` writes, in order:

| Part | Bytes |
|---|---|
| `start_signature()` — 1+1+1+1+16+1+1 | 22 |
| 8 fields, each `padded_byte_string` to 32 | 256 |
| zero padding reserved for a signature | 256 |
| **total** | **534** |

FIELD4 is `extension_version`, the git describe that the move changes. FIELD3
(`duckdb_version`), FIELD2 (`duckdb_platform`) and FIELD5 (`abi_type`) do not
change.

Verified against the binary — the footer begins exactly 534 bytes from the end:

```
$ tail -c 534 <ext> | head -c 22 | xxd
00000000: 0093 0410 6475 636b 6462 5f73 6967 6e61  ....duckdb_signa
00000010: 7475 7265 8004                           ture..
```

### 12b. The corrected oracle

Compare the **body**, not the file:

```bash
body_hash() { head -c $(( $(stat -c %s "$1") - 534 )) "$1" | sha256sum | cut -d' ' -f1; }
```

The body is commit-independent, measured:

| Reading | Body | Footer |
|---|---|---|
| occurrences of `fea334e` | **0** | 1 |

A `1.5.6` string does remain in the body, but that is the DuckDB version, fixed
by the submodule pin and identical across the two builds.

### 12c. Build plan and the controls

Three builds, each a release build through Slurm. Hash the body of each.

| Build | Tree | Expectation | What it proves |
|---|---|---|---|
| A | `main` at `6c9ceb8` | — | the baseline |
| B | after deletion + move | **body(B) == body(A)** | the move changed nothing |
| C | B plus one deliberate one-line behaviour change | **body(C) != body(A)** | the comparison can see a difference |

Reading C is not optional. Without it, `body(A) == body(B)` is also what a
stale binary or a broken build script prints.

**If body(A) != body(B)**, do not start a determinism project. Diff the two,
record what differs, and downgrade the oracle to the registered-surface
comparison plus the full suite. Record the downgrade and its reason here.

---

## 13. Correction to §10b: the blast radius is 3x what I wrote (measured 2026-11-18)

§10b listed 10 locations. It was built from locations I PREDICTED, not from a
grep. An exhaustive grep finds **34 tracked files** that contain the string
`src/`:

```bash
git grep -ln 'src/' -- CMakeLists.txt Makefile 'cmake/*' '.github/*' \
                        'scripts/*' 'spec/*' ':!.worktrees'
```

| File | `src/` occurrences | §10b said |
|---|---|---|
| `CMakeLists.txt` | **62** | "~27 explicit source paths" |
| `scripts/generate_taxonomy.py` | 15 | not listed |
| `scripts/generate_all_parsers.sh` | 9 | not listed |
| `spec/layout/layout.yaml` | 8 | 1 line |
| `.github/workflows/MainDistributionPipeline.yml` | 6 | 5 lines |
| `scripts/test_core_only_build.sh` | 6 | not listed |
| `scripts/verify_parser_reproducibility.sh` | 6 | not listed |
| `spec/abi/module-abi.yaml` | 5 | not listed |
| `scripts/embed_sql_macros.py` | 4 | listed |
| `scripts/verify_layout_invariance.py` | 4 | not listed |
| `scripts/verify_native_dispatch.py` | 4 | listed |
| `cmake/BuiltinLanguages.cmake` | 2 | 2 comments |
| `Makefile`, `spec/taxonomy/taxonomy.yaml`, +2 more live | 1 each | not listed |
| 18 one-off scripts | ~35 | not listed |

This is the "precise measurement of the wrong subject" shape again: §10b's
figures were each defensible and the set they came from was invented.

### 13a. Live vs dead, by what actually invokes a script

The discriminator is whether CI or the build runs it:

```bash
grep -ohE 'scripts/[A-Za-z0-9_.-]+' .github/workflows/*.yml | sort -u
grep -ohE 'scripts/[A-Za-z0-9_.-]+' CMakeLists.txt Makefile cmake/*.cmake | sort -u
```

**Live, and in scope (16 files):** `CMakeLists.txt`, `Makefile`,
`cmake/BuiltinLanguages.cmake`, `.github/workflows/MainDistributionPipeline.yml`,
`spec/layout/layout.yaml`, `spec/abi/module-abi.yaml`,
`spec/taxonomy/taxonomy.yaml`, and the 9 scripts CI or CMake invokes:
`embed_sql_macros.py`, `generate_taxonomy.py`, `generate_all_parsers.sh`,
`test_core_only_build.sh`, `test_verify_macro_boundary.py`,
`verify_layout_invariance.py`, `verify_macro_boundary.py`,
`verify_native_dispatch.py`, `verify_parser_reproducibility.sh`.

`scripts/verify_module_abi.py` holds **no** `src/` string. Its only tie to this
work is the orphan comment at line 77.

**Dead, and out of scope (18 one-off scripts, ~35 references).**

### 13b. Ruling: leave the 18 dead scripts stale

The precedent is already set, and measured. Stage 1 moved the per-language
adapters and broke a reference that is still broken today:

| Script | Cites | Exists? |
|---|---|---|
| `scripts/audit_leaf_text_gaps.py` | `src/language_adapters/sql_adapter.cpp` | **no** — stage 1 moved it |
| `scripts/ast_tool_refactored.sh` | `src/main.cpp` | **no** |

Nobody repaired either. So stage 4 follows the same precedent: it does not
touch the one-off scripts. This is a **stated omission, not an oversight** —
it goes in the PR body.

Cost if wrong: somebody later runs a one-off script and it fails on a path.
That is already true of these scripts today.

**Not** proposed here: moving the 18 into `workspace/`, where the repo's
CLAUDE.md says one-off tools belong. That is a separate cleanup and bundling
it into stage 4 would hide the move inside an unrelated diff.

### 13c. Why a blind sed on CMakeLists.txt would be a disaster

Of `CMakeLists.txt`'s 62 `src/` occurrences, **32 are not sitting_duck's
`src/` at all**. They are the grammar submodules' own source directories:

```
generate_parser("tree-sitter-rust" "tree-sitter-rust/src/parser.c" "tree-sitter-rust/src/scanner.c")
list(APPEND EXTENSION_SOURCES third_party/tree-sitter/lib/src/lib.c)
```

`sed -i 's|src/|core/|g' CMakeLists.txt` would rewrite all 32 and point every
grammar at a parser that does not exist. The explicit source list means CMake
would fail loudly rather than build something wrong, but the diff would be
unreviewable.

**The discriminating pattern**, validated against the file:

```bash
grep -nE '(^|[[:space:]"(])src/' CMakeLists.txt
```

| Reading | Count | Required |
|---|---|---|
| lines the pattern matches | 28 | — |
| of those, touching `tree-sitter` or `third_party` | **0** | must be 0 |
| sitting_duck `src/` lines the pattern MISSES | **0** | must be 0 |

Two of the 28 matches are comments about the GRAMMARS' src directories, not
about sitting_duck's. Identify them by text, never by line number, because
this commit moves lines in this same file:

- the comment containing `Each grammar's src/tree_sitter/ directory`
- the comment containing `get their own src/ prepended`

Both stay unchanged. **Net genuine edits in `CMakeLists.txt`: 26** — which
reconciles with the independent categorisation (24 top-level `.cpp` + 1
`include_directories` + 1 `language_adapters`).

Apply the same discrimination to every live file, not just CMakeLists: the
grammar directories contain `src/` throughout, and
`scripts/generate_all_parsers.sh` (9 occurrences) is almost certainly ALL
grammar paths, not sitting_duck's. Check each of the 16 live files with the
pattern and read the matches before editing.

### 13d. The real step-4 edit set, after reading every match

Counting matches is not classifying them. Three files whose counts looked
alarming need **zero** edits, because their `src/` is a grammar's own
directory, reached after a `cd`, with no `tree-sitter` string on the line:

| File | raw count | real edits | why |
|---|---|---|---|
| `scripts/generate_all_parsers.sh` | 9 | **0** | all `src/parser.c`, `src/scanner.c`, `src/tree_sitter/` inside a grammar dir |
| `scripts/verify_native_dispatch.py` | 4 | **0** | `base.glob(f"{d}/src/parser.c")` and comments about it |
| `Makefile` | 1 | **0** | `generated_parsers/tree-sitter-json/src/parser.c` |

One more false positive: `scripts/embed_sql_macros.py:72` is a **usage example
for a user**, `ast_callers('src/**/*.py')`. It is not a path in this repo and
must not be rewritten.

**The edit set:**

| File | edits | notes |
|---|---|---|
| `CMakeLists.txt` | 26 | 24 source paths + `include_directories` + 1 adapter; 2 grammar comments excluded |
| `scripts/generate_taxonomy.py` | 15 | 7 are live target paths in its output table |
| `scripts/test_core_only_build.sh` | 6 | 1 live `git diff` guard, 5 messages/comments |
| `spec/abi/module-abi.yaml` | 5 | `enforced_in:` / `consumed_by:` metadata |
| `.github/workflows/MainDistributionPipeline.yml` | 5 | line 384's `generated_parsers/**/src/parser.c` excluded |
| `spec/layout/layout.yaml` | 4 + 1 delete | the delete is the orphan entry |
| `scripts/embed_sql_macros.py` | 3 | the usage example excluded |
| `cmake/BuiltinLanguages.cmake` | 2 | both comments |
| `spec/taxonomy/taxonomy.yaml` | 1 | comment |
| `scripts/verify_macro_boundary.py` | 1 | help text |
| `scripts/verify_layout_invariance.py` | 1 | docstring; 3 grammar paths excluded |
| `scripts/test_verify_macro_boundary.py` | 1 | docstring |
| **total** | **70 edits, 12 files** | |

Two historical comments stay as they are, because they describe the state
BEFORE stage 1 and rewriting them would make them false:

- `spec/layout/layout.yaml`: "Was 59 before 054 M2 stage 1, when the globs were
  src/language_configs and src/language_adapters"
- `scripts/verify_native_dispatch.py`: the comment noting the string
  `src/language_adapters` never appeared in the code before that move

### 13e. Generated files that must be regenerated, not hand-edited

`scripts/generate_taxonomy.py` writes regions INTO five files that are
themselves moving, and `scripts/embed_sql_macros.py` writes a header:

| Generator | Writes |
|---|---|
| `generate_taxonomy.py` | `semantic_types.hpp`, `semantic_types.cpp`, `node_config.hpp`, `language_config_json.cpp`, `ast_type_map_function.cpp` |
| `embed_sql_macros.py` | `embedded_sql_macros.hpp` |

Both generators emit a comment naming their source directory, so their OUTPUT
changes when the directory changes. After the move, run both and commit the
result, or the "taxonomy tables in sync" and "Embedded SQL macros header in
sync" gates go red.

**This does not threaten the §12 oracle.** The changed text is a `//` comment
in a header. A comment does not reach the compiled binary, so `body(A)` and
`body(B)` are unaffected. Status: reasoned from how C++ comments work, NOT yet
measured — build B is what measures it.

### 13f. The blind spot: paths built from COMPONENTS (measured 2026-11-18)

Everything above greps for the string `src/`. Several live scripts never
contain that string, because they build the path from components:

```python
# scripts/embed_sql_macros.py:320
sql_dir = project_root / "src" / "sql_macros"
output_file = project_root / "src" / "include" / "embedded_sql_macros.hpp"
```

**The codebase warned me about this and I read the warning without acting on
it.** `scripts/verify_native_dispatch.py` carries a comment saying the path
"is built from components, so the string `src/language_adapters` never
appeared". I quoted that comment in §13d as something to preserve, and did not
notice it described a hole in my own method.

Search that finds them:

```bash
git grep -nE '"src"|'"'"'src'"'"'' -- . ':!.worktrees' ':!tracker'
```

**Live component paths, all previously missed:**

| File | count | note |
|---|---|---|
| `scripts/verify_module_abi.py` | 5 | incl. the glob for the 20 `*_native_extractors.hpp` |
| `scripts/verify_native_dispatch.py` | 3 | lines for `function_call_extractor.hpp`, the `rglob`, the extractor glob |
| `scripts/test_verify_macro_boundary.py` | 3 | builds a fixture tree |
| `scripts/embed_sql_macros.py` | 2 | **the generator's real input and output paths** |
| `scripts/verify_macro_boundary.py` | 1 | |
| `scripts/test_core_only_build.sh` | 1 | |
| `scripts/test_embed_manifest.py` | 1 | CI-invoked; has NO `src/` string at all, so it never appeared in any earlier list |
| **total** | **16** | |

**What this would have cost.** `embed_sql_macros.py` is the macro generator. My
move script, as first written, left those two lines alone, so the generator
would have looked for a `src/sql_macros` that no longer exists. The script's
own assertions would have **passed**, because they only checked for leftover
`src/` strings. A gate that incomplete information can satisfy is not a gate.

**Not paths, despite matching** — do not touch:

| Location | What `src` is |
|---|---|
| `src/include/html_native_extractors.hpp:509` | the HTML `src` **attribute** name, in a set with `href`, `action` |
| `tools/ast-cli/ast`, `ast.py` | a **CLI subcommand** name |
| `scripts/verify_macro_boundary.py:241` | a **variable**: the source node of a macro edge |
| `scripts/conformance_kit.sh:517` | a shell variable |

**Grammar components — leave:** `audit_anon_token_collisions.py`,
`audit_leaf_text_gaps.py`, `audit_unreachable_declarations.py`,
`generate_all_parsers.sh` (`find "src"` after a cd),
`verify_native_dispatch.py:183` (`base / "src" / "parser.c"`), and all 6 refs
in `verify_parser_reproducibility.sh`, which is **entirely** grammar paths and
needs zero edits despite being live.

### 13g. Corrected total

| Reference style | Files | Edits |
|---|---|---|
| string `src/...` | 12 | 70 |
| component `/ "src" /` | 7 | 16 |
| **total** | **15 distinct** | **86** |

Searched for and found absent: `os.path.join("src", ...)`, CMake
`${VAR}/src`, and any `SRC_DIR` variable.

---

## 14. Execution log (2026-11-18)

Branch `feat/m2-stage4-core-directory` off `6c9ceb8` (main, after #344 merged).

| Step | Result |
|---|---|
| Baseline build A | rc 0 in **9m54s**, 16 cores, Slurm job 4308 |
| Orphan deleted | 2 files + 3 live references |
| `git mv src core` | 108 tracked files |
| Path edits | 74 string (4 restored as historical) + 16 component = **86 genuine** |
| Assertions | all 6 groups passed |
| Generators re-run | `embedded_sql_macros.hpp` 2 lines; taxonomy **no change** |
| `CLEANUP_SUMMARY.md` | deleted |
| Commit | `ad15d50`, 125 files, 107 renames, +91 −681 |

### 14a. The bug my own assertions did not catch

Step 3 failed on first run:

```
sed: -e expression #1, char 27: unknown option to `s'
FATAL: sed failed on CMakeLists.txt
```

The pattern contains `|` alternations and I used `|` as the sed delimiter. sed
validates the expression before touching the file, so `CMakeLists.txt` was
unchanged and the run stopped at the right place. Delimiter changed to `#`.

This is a reminder that the script's `set -o pipefail` and explicit `|| fail`
were what made the failure loud. A `sed -i` without a status check would have
left the move half-applied and reported success.

### 14b. The separately-built-baseline guard, measured

| Build | Commit | Tree hash |
|---|---|---|
| A | `6c9ceb8` | `36e75046094819dbff304268b7d651a354cd21f2` |
| B | `ad15d50` | `c984ce4ffab4397fdf374f9c97be9f05f01a6dbc` |

Different commits and different trees. `body(A) == body(B)` therefore cannot be
the same-artifact-twice failure. Both builds also reported `ccache? [none]`, so
no shared object cache could hand both builds the same objects.

### 14c. Correction to my own §9 reasoning on CLEANUP_SUMMARY.md

I wrote that "every path it names is gone", citing `src/archive/` as absent.
That is true of the path it names. But the commit output revealed
`src/sql_macros/archive/` **does** exist, holding 11 numbered `.sql` files. My
earlier check listed `src/sql_macros` with `basename`, which hid the
subdirectory, and I then described the directory as holding "a completely
different numbered scheme" when those numbered files ARE the archive.

The deletion still stands: the document names five specific files and one
directory path, and none of those exist. But the archive concept survives at a
different path, and my sentence invited the reading that it does not.

---

## 15. Two more reference forms, found after the move was committed

§13f added the component form to the string form. Two more exist, and each was
found only after I thought the sweep was complete.

### 15a. Functional paths in the TEST suite (fixed, commit `30c4f88`)

I swept build, CI, scripts and spec. I did not sweep `test/`. Three test files
parse a real file from this repository by path:

```sql
FROM read_ast('src/unified_ast_backend.cpp', 'cpp', context := 'native')
```

11 references across `edge_cases_and_robustness.test`,
`memory_corruption_prevention.test` and `multi_execution_stability.test`. After
the move that file does not exist, so these tests fail. CI would have caught
it; my own assertions would not.

**Golden-rule note.** This changes tests in the same branch as an
implementation change, so each is stated separately: the implementation change
is the move; the test change is the path only. The file is a 100% rename, so
its content is byte-identical and every expected value stays valid. 11
insertions, 11 deletions, each a path. Committed apart from the move.

**Deliberately not changed:** `'src/app.py'` in `ast_select_file_line.test` and
`'src/main.py'` in `detect_language.test`. Neither is a file. `src/app.py` also
appears in that test's **expected result rows**, so rewriting it would have
broken the assertions it is there to prove.

### 15b. A bare `src` with no slash (`Makefile:152`)

```makefile
FORMAT_DIRS := src test/sql test/unittest
```

Every pattern so far required `src/` or `"src"`. This has neither. Unfixed,
`make format` and `make format-check` silently stop covering the engine — no
error, no output, just nothing formatted. Absence of signal.

`grep -nw 'src'` finds it. That is the fourth search shape needed:

| Shape | Example | Found in |
|---|---|---|
| string | `src/include/foo.hpp` | 12 files |
| component | `ROOT / "src" / "include"` | 7 files |
| functional, in tests | `read_ast('src/unified_ast_backend.cpp')` | 3 files |
| bare word | `FORMAT_DIRS := src` | 1 file |

`languages/` is also absent from `FORMAT_DIRS`, so stage 1 created the same gap
and it is still open. **Not fixed here** — that is stage 1's debt, and bundling
it would widen this diff. Flagged instead.

### 15c. A stale runtime error message

`core/ast_sql_macros.cpp:162` throws:

```
EMBEDDED_MACRO_DOCS is out of sync with src/sql_macros/ (re-run scripts/embed_sql_macros.py)
```

The directory it names no longer exists. This is a compiled string literal, so
fixing it **changes the binary** — which makes it the discrimination control
build C needs, instead of a throwaway perturbation. See §12c.

### 15d. Compiled strings that must NOT change

These match every pattern and are user-facing documentation, not paths in this
repository:

| Location | What it is |
|---|---|
| `core/read_ast_streaming_function.cpp` ×5 | the `examples` argument to `RegisterDocumentedTableFunctionSet` — `read_ast('src/main.cpp')`, about the USER's file |
| `core/semantic_type_functions.cpp:748` | `detect_language('src/main.rs')` example |
| `core/ast_copy_function.cpp:130`, `core/ast_source_bytes_function.cpp:83` | example data |
| `core/include/html_native_extractors.hpp:509` | the HTML `src` attribute, beside `href` and `action` |
| `core/include/embedded_sql_macros.hpp` | generated from the macros' own usage banners |
| `AI_AGENT_GUIDE.md`, `API_REFERENCE.md` | `read_ast('src/**/*.py')` — a reader's own tree |

### 15e. My sweep pattern was silently wrong

One sweep reported two hits and missed `Makefile:152`, which it matched
correctly when tested on that line alone. The shell mangled the embedded quotes
in the pattern, so `git grep` searched for something other than what I wrote,
and reported a clean result for it.

**Standing fix:** for a sweep whose result will be trusted, use the simplest
pattern that can work (`grep -nw`), and prove it on a line you know must match
before believing a short answer.

---

## 16. Oracle results (measured 2026-11-18)

### 16a. Half 1: the artifact body

| Build | Commit | Tree | Body sha256 | Full sha256 |
|---|---|---|---|---|
| A | `6c9ceb8` | `36e7504` | `b3cc9054db1522ea…` | `b7733fccdfb4382a…` |
| B | `ad15d50` | `c984ce4` | `b3cc9054db1522ea…` | `4dd7e3e0fc1f4536…` |

**`body(A) == body(B)`.** The move changed no compiled byte. Both builds are
84733934 bytes.

The full-file hashes differ, exactly as §12 predicted, and the difference is
fully attributed — the footers are identical but for field 4:

```
A: duckdb_signature|6c9ceb80f1|d54cbbe3df|linux_amd64
B: duckdb_signature|ad15d50c91|d54cbbe3df|linux_amd64
```

Field 3 (`d54cbbe3df`, the DuckDB submodule) is unchanged, as the pin requires.
Had the oracle used whole-file equality, this would have read as a regression.

### 16b. Half 2: the registered surface, and the hole in my first capture

My original capture queried `function_name LIKE 'ast%' OR LIKE '%sitting%'`.
That filter **omits the extension's primary entry points**:

| Function | In the filtered capture |
|---|---|
| `read_ast` | **no** |
| `parse_ast` | **no** |
| `detect_language` | **no** |

78 rows, and the three functions a user actually calls were not among them. An
anomaly probe with no denominator.

Two attempts to fix it failed first, each for a reason worth recording:

1. Differencing `duckdb_functions()` before and after `LOAD` returned **0
   registered**. The dev shell has sitting_duck **statically linked**, so
   `read_ast` is already present before the LOAD. My `WARN` on a low count is
   what caught it.
2. Loading the extension into the standalone `duckdb` failed: the extension
   declares duckdb version `d54cbbe3df` (the submodule commit) and refuses to
   load into `v2.0.0` release.

The capture that works compares the **complete** catalogue of each build's own
dev shell, with signatures:

```sql
SELECT function_type||'|'||function_name||'|'||
       coalesce(list_aggregate(parameter_types,'string_agg',','),'')||'|'||
       coalesce(return_type,'')
FROM duckdb_functions() ORDER BY 1;
```

| Build | Rows | sha256 | read_ast | ast_* |
|---|---|---|---|---|
| A | 3391 | `c15945e39d8b8e58…` | 2 | 77 |
| B | 3391 | `c15945e39d8b8e58…` | 2 | 77 |

**Identical.** No registration, signature or return type changed.

### 16c. Build C: the control that makes 16a and 16b mean anything

Two equal readings are also what a stale binary or a broken script prints. C
perturbs both halves at once, with one change I want anyway and one temporary:

| Change | Expected effect |
|---|---|
| `core/ast_sql_macros.cpp` error message `src/sql_macros/` → `core/sql_macros/` | body differs (compiled string literal) |
| `Makefile` `FORMAT_DIRS := src` → `core` | no effect on the binary |
| **temporary:** register `ast_supported_languages_CONTROL` | body AND surface differ |

Required: `body(C) != body(B)` and `surface(C) != surface(B)`, with the surface
diff naming exactly the renamed function. The rename is reverted afterwards;
the other two are kept.

The oracle for the MOVE is already complete at A vs B, on the move commit
itself. C tests the instrument, not the move.
