# M2 Stage 3: SQL Macro Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split sitting_duck's 96 SQL macros into a duckling core and add-ons, expressed as a manifest and one file split, and give every core macro a form that runs on a caller-supplied AST table.

**Architecture:** `src/ast_sql_macros.cpp:80` registers macros per file, so the boundary is a property of files. Measured, the boundary already matches the existing files except for `tree_navigation.sql`. So stage 3 splits that one file, renames nothing, and records the core/add-on tags in `spec/layout/layout.yaml`, which `scripts/embed_sql_macros.py` then reads in place of its hardcoded list. Nine new `_from` variants close the parity gap. A static call-graph gate enforces the boundary, because `query_table()` defers macro binding and `LOAD` therefore cannot catch a violation.

**Tech Stack:** DuckDB SQL macros, Python 3 (`scripts/`), CMake, sqllogictest (`test/sql/*.test`), GitHub Actions.

**Spec:** `tracker/features/057-v2-m2-stage3-macro-split.md`

## Global Constraints

- **No behaviour change to the 96 existing macros.** Their `macro_definition` in `duckdb_functions()` must be byte-identical before and after. Moving text between files is permitted; editing a body is not.
- **One canonical registration order**, held in `spec/layout/layout.yaml`. A subset build filters it while preserving relative order, following the precedent `spec/layout/layout.yaml:45-49` records for languages.
- `semantic_predicates.sql` registers first. Five files depend on it and scalar macros bind eagerly.
- **No file tagged `core` may reference a macro owned by a file tagged `addon`.** Measured today: zero such edges, transitive closure included.
- **Every new macro needs a `-- <name>: <summary>` banner** directly above it. `scripts/embed_sql_macros.py` lifts descriptions from that exact form; without one the macro reaches `duckdb_functions()` with no description.
- Pass `-no-agent` to every `duckdb` invocation. `CLAUDECODE` triggers agent mode, which plans each statement twice.
- Run builds and tests through blq, not Bash, and use no shell pipes or redirects in blq commands.
- Change implementation **or** tests in a step, never both at once.
- Commit only what each task's step names. If on the default branch, branch first.
- **Every build and test runs through the Slurm queue**, because this repo is not
  in blq's `slurm.toml` roots and so a bare blq command runs directly on the
  shared box. Run the `srun` line through `blq exec`, never through Bash, and
  never nest `blq` inside the job.

  Build:
  `srun -n 1 -c 8 --mem=24G -p batch -t 300 --export=ALL,CMAKE_BUILD_PARALLEL_LEVEL=8 make release`

  Tests:
  `srun -n 1 -c 4 --mem=8G -p ci -t 25 build/release/test/unittest <filter>`

  **Size the request to what is free, and check before you submit.** The
  queue's share of longbottom is 28 CPUs and 64 GB, shared with other sessions.
  Read `scontrol show node longbottom` for `CPUAlloc` and `AllocMem`, and keep
  `AllocMem + your --mem` under 64 GB (65536 MB).

  MEASURED 2026-11-10, with 36864 MB held by three other jobs: `--mem=28G` hit
  65536 MB exactly and was **reserved 73 minutes into the future**, while
  `--mem=24G` started at once. Always pass `-n 1`; on this cluster `-c 1` alone
  starts two tasks.

  **An over-sized request does not error — it waits, and it looks like a build
  that is simply slow.** Its whole output is one line,
  `srun: job <id> queued and waiting for resources`. After submitting, confirm
  `squeue --me` shows `RUNNING` and not `PENDING` before you start waiting on
  it. A job killed at its memory limit is different: rc 1 with `Out Of Memory`.
- **The banner rule is enforced by a test that already exists.**
  `test/sql/bugs/issue_170_macro_descriptions.test:99-104` asserts that **zero**
  non-system `table_macro` rows have a NULL or empty description. Every macro
  added by Tasks 4, 5 and 6 is a table macro, so one missing
  `-- <name>: <summary>` banner turns that existing test red.

## Review Focus

Five input classes the spec implies and no task above would otherwise exercise. Each line's test is added to the task that owns the code.

1. **A `_from` macro given a table name that does not exist.** A reasonable caller expects a named error, not 0 rows. The #89 principle makes 0 rows mean "searched correctly, not there". Test lands in Task 4.
2. **A `_from` macro given an empty-but-well-formed AST table.** Must return 0 rows and not error, and must stay distinguishable from case 1. Test lands in Task 4.
3. **A `_from` macro given a table whose required columns are present but NULL.** Measured separately: 10 of the 22 columns must be populated for a selector to be correct. A silently wrong answer is worse than an error. Test lands in Task 5.
4. **An AST table spanning several `file_path` values.** The path-taking twins accept globs, so equivalence must hold across files, not only within one. Test lands in Task 5.
5. **A core build where `ast_unparse_rules()` returns no rows for the language at hand.** §3.5 measured that the `duckdb` adapter errors by design, but a language with no rules is untested, and emitting wrong text would break the textual law. Test lands in Task 7.

---

### Task 1: Split `tree_navigation.sql`

**Files:**
- Create: `src/sql_macros/tree_analysis.sql`
- Modify: `src/sql_macros/tree_navigation.sql` (remove 3 discontiguous blocks)
- Modify: `scripts/embed_sql_macros.py:176-190` (add the new filename at position 4)
- Test: `test/sql/macro_split_oracle.test`

**Interfaces:**
- Consumes: nothing.
- Produces: `src/sql_macros/tree_analysis.sql` holding exactly 8 macros: `ast_resolve_entity`, `ast_function_metrics`, `ast_functions_containing`, `ast_nesting_analysis`, `ast_security_audit`, `ast_dead_code`, `ast_get_calls`, `ast_call_graph`. `tree_navigation.sql` keeps 11: `ast_children`, `ast_call_arguments`, `ast_definitions`, `ast_descendants`, `ast_ancestors`, `ast_siblings`, `ast_containing_line`, `ast_in_range`, `ast_function_scope`, `ast_class_members`, `ast_definition_parent`.

The 8 moving macros occupy these line ranges in today's file, which are not
contiguous: 89-133, 298-405, 406-484, 485-569, 570-688, 736-842, 843-927,
928-950 — collapsing to the 3 blocks 89-133, 298-688 and 736-950. The 11 core
macros keep 1-88, 134-297 and 689-735.

**Each span starts at the first line of the macro's comment block, not at its
`CREATE` line.** The `-- <name>: <summary>` banner sits above the `CREATE`, and
the generator lifts the description from it, so a cut at the `CREATE` line
silently strips all 8 descriptions. (This plan first gave `CREATE`-to-`CREATE`
ranges; corrected 2026-11-10 after Task 1's implementer caught it.)

- [ ] **Step 1: Capture the oracle from the current build**

**Do not save the CLI's rendered table.** MEASURED on this tree: the default
output ends in `97 rows (40 shown)`, so a text dump holds 40 of the 97 rows, and
two such dumps diff clean even when a body among the other 57 changed. Use
`COPY`, which writes every row:

```sql
LOAD sitting_duck;
COPY (
  SELECT function_name, function_type, parameters, description,
         length(macro_definition) AS deflen, md5(macro_definition) AS defhash
  FROM duckdb_functions() WHERE database_name = 'memory'
  ORDER BY function_name, function_type
) TO 'build/macro-oracle/macros-before.csv' (HEADER, DELIMITER ',');
```

Create `build/macro-oracle/` first; `build/` is gitignored. Invoke the binary
with `-unsigned -no-agent`.

`description` is in the dump deliberately. A macro moved without its
`-- <name>: <summary>` banner keeps its body and loses its description, so a
dump of the body alone is blind to the mistake this task is most likely to make.

Then confirm the file holds **97 data rows** (98 lines, with the header).
Expected: **97** = 45 scalar macros and 52 table macros. The 45 is 44 defined in
`src/sql_macros/` plus the `ast_dispatch_predicate` stub that
`src/ast_sql_macros.cpp:181` creates, which is also a scalar macro. Do not
expect 44. If the count is not 97, stop and report: the oracle is reading the
wrong catalog, and every later comparison inherits the error.

- [ ] **Step 2: Write the failing oracle test**

Create `test/sql/macro_split_oracle.test`, following the header style of `test/sql/bugs/issue_170_macro_descriptions.test`. Assert the three counts that must not move:

```
require sitting_duck

query III
SELECT
  count(*) FILTER (WHERE function_type = 'macro'),
  count(*) FILTER (WHERE function_type = 'table_macro'),
  count(*)
FROM duckdb_functions() WHERE database_name = 'memory';
----
45	52	97
```

Then assert that the 8 moved macros are still registered and still table macros, by name.

- [ ] **Step 3: Run it against the unsplit tree to confirm it passes now**

Run the test. Expected: PASS. This is a guard, not a red test: it must pass before and after, and its job is to fail if the split drops a macro.

- [ ] **Step 4: Move the 8 macros, with their banner comments**

Cut each of the 8 blocks from `src/sql_macros/tree_navigation.sql` into a new `src/sql_macros/tree_analysis.sql`. Carry each macro's `-- <name>: <summary>` banner with it, or its `duckdb_functions()` description is lost. Give the new file a header comment in the style of the file it came from. Copy the macro text verbatim; editing a body here breaks the Global Constraint.

- [ ] **Step 5: Register the new file at position 4**

In `scripts/embed_sql_macros.py:176-190`, insert `'tree_analysis.sql'` directly after `'tree_navigation.sql'`. Position 4 keeps the hub first and leaves every cross-file dependency satisfied.

- [ ] **Step 6: Regenerate the embedded header and rebuild**

Run `python3 scripts/embed_sql_macros.py`, then build through blq. The generated `src/include/embedded_sql_macros.hpp` is checked in and CI compares it, so it must be committed with this change.

- [ ] **Step 7: Prove the split changed nothing**

Re-run Step 1's `COPY` into `build/macro-oracle/macros-after.csv` and diff that
against `build/macro-oracle/macros-before.csv`.
Expected: **no differences.** A changed `defhash` means a body was edited; a
changed `description` means a banner was left behind.
Then run the Step 2 test. Expected: PASS.

- [ ] **Step 8: Prove the oracle can fail**

Delete one macro from `tree_analysis.sql`, regenerate, rebuild, and re-run the test. Expected: FAIL on the count row. Restore the macro, regenerate, rebuild, confirm PASS again. A guard nobody has seen fail is not a guard.

- [ ] **Step 9: Commit**

```bash
git add src/sql_macros/tree_navigation.sql src/sql_macros/tree_analysis.sql \
        scripts/embed_sql_macros.py src/include/embedded_sql_macros.hpp \
        test/sql/macro_split_oracle.test
git commit -m "refactor(057): split tree_analysis.sql out of tree_navigation.sql (M2 stage 3)"
```

---

### Task 2: The core/add-on manifest, read by the generator

**Files:**
- Modify: `spec/layout/layout.yaml` (new top-level key)
- Modify: `scripts/embed_sql_macros.py` (read the manifest; fail loud on a missing file)
- Modify: `.github/workflows/MainDistributionPipeline.yml` (PyYAML for the header-sync job — see Step 4b)
- Modify: `CMakeLists.txt` (surface the generator's stderr — see Step 4c)
- Test: `scripts/test_embed_manifest.py`

(The `test/sql/macro_split_oracle.test` entry that stood here was wrong: no step
in this task extends that test, and this task moves none of its counts. Step 7
only re-runs it.)

**Interfaces:**
- Consumes: Task 1's 14-file tree.
- Produces: `spec/layout/layout.yaml` key `sql_macros`, a list of 14 mappings in canonical registration order, each `{file: <name>, side: core|addon}`. `scripts/embed_sql_macros.py` reads it and exposes the order; nothing else parses it yet.

`scripts/verify_layout_invariance.py` reads layout.yaml by key name and uses `.get()` for optional keys, so a new top-level key is additive and will not break it.

The 14 entries, in order, with their side:

| # | file | side |
|---|---|---|
| 1 | `semantic_predicates.sql` | core |
| 2 | `file_utilities.sql` | core |
| 3 | `tree_navigation.sql` | core |
| 4 | `tree_analysis.sql` | addon |
| 5 | `pattern_matching.sql` | addon |
| 6 | `relational_operators.sql` | core |
| 7 | `parse_ast_list_table.sql` | core |
| 8 | `css_selectors.sql` | addon |
| 9 | `selector_for.sql` | addon |
| 10 | `ast_select_rules.sql` | addon |
| 11 | `scope_resolution.sql` | addon |
| 12 | `duck_blocks.sql` | addon |
| 13 | `ast_patch.sql` | addon |
| 14 | `ast_unparse.sql` | core |

- [ ] **Step 1: Write the failing generator test**

Create `scripts/test_embed_manifest.py`, run with `python3 -I`. Three cases:
- the manifest's 14 filenames equal the files present in `src/sql_macros/*.sql`, as sets;
- a manifest entry naming a nonexistent file raises;
- the generated header's file order equals the manifest order.

- [ ] **Step 2: Run it to verify it fails**

Run: `python3 -I scripts/test_embed_manifest.py`
Expected: FAIL — `layout.yaml` has no `sql_macros` key.

- [ ] **Step 3: Add the `sql_macros` key to `spec/layout/layout.yaml`**

Add the 14 entries above, in that order, after the existing `registration_order` block. Include a comment block in the house style of that file explaining that this list IS registration order, that a subset filters it while preserving relative order, and why a subset therefore needs no re-sort.

- [ ] **Step 4: Make the generator read the manifest, and fail loud**

In `scripts/embed_sql_macros.py`, replace the hardcoded list with a read of
`spec/layout/layout.yaml`'s `sql_macros` key. The list begins at **line 176**
(`sql_files = [`) and now holds 14 entries, because Task 1 inserted
`'tree_analysis.sql'` at line 180.

**Also remove the silent skip.** The two sites are now at **lines 212 and 273**
(the brief's earlier 211/272 predate Task 1's insertion). Each wraps a file in
`if os.path.exists(filepath)`, so a manifest naming a missing file is dropped
without a word and the extension loses those macros quietly. Raise instead,
naming the file.

**Do not touch the `os.path.exists` at line 90.** It is a different, legitimate
check and is not part of this task.

If `import yaml` fails, exit non-zero with a message naming PyYAML. Do not fall
back to a hardcoded list and do not carry on — a second code path that can
disagree with the manifest is the defect this task removes.

- [ ] **Step 4b: Give the header-sync CI job PyYAML**

This is required, not optional. `scripts/embed_sql_macros.py` runs during the
build (`CMakeLists.txt:252`) and, as of this step, imports `yaml`. MEASURED: the
`embedded-header-sync` job at `.github/workflows/MainDistributionPipeline.yml:311`
regenerates the header and verifies it, and it does **not** install PyYAML —
only `module-abi-sync`, `layout-invariance` and `taxonomy-tables-sync` do, at
lines 344, 388 and 405. Without this step that job fails on ImportError, and it
is the one gate that would otherwise catch a stale header.

Add the same step those three jobs use, verbatim:

```yaml
      - name: Ensure PyYAML is available
        run: python3 -c "import yaml" || python3 -m pip install --break-system-packages pyyaml
```

- [ ] **Step 4c: Make a failed generator name its own cause**

`CMakeLists.txt:251-259` runs the generator and, on failure, emits
`message(WARNING "Failed to generate embedded SQL macros header")` while
discarding the script's output. The committed header is a legitimate fallback,
so the build is right not to stop — but after this task a missing PyYAML would
produce a stale header and a warning that does not say why.

Add `ERROR_VARIABLE` and `OUTPUT_VARIABLE` to the `execute_process` call and
include the captured stderr in the warning text.

**Do not change `WARNING` to `FATAL_ERROR`.** The header is checked in and the
build correctly falls back to it; making this fatal would break every build on a
machine without PyYAML, which is a worse regression than the one being fixed.

- [ ] **Step 5: Run the generator test to verify it passes**

Run: `python3 -I scripts/test_embed_manifest.py`
Expected: PASS, all three cases.

- [ ] **Step 6: Prove the header did not move**

Run `python3 scripts/embed_sql_macros.py` and `git diff --stat src/include/embedded_sql_macros.hpp`.
Expected: **no change.** The manifest reproduces Task 1's order exactly, so a diff here means the order differs.

- [ ] **Step 7: Rebuild and re-run the oracle**

Build through blq, then run `test/sql/macro_split_oracle.test`. Expected: PASS, still 45/52/97.

- [ ] **Step 8: Commit**

```bash
git add spec/layout/layout.yaml scripts/embed_sql_macros.py \
        scripts/test_embed_manifest.py CMakeLists.txt \
        .github/workflows/MainDistributionPipeline.yml
git commit -m "feat(057): the macro manifest in layout.yaml owns registration order (M2 stage 3)"
```

---

### Task 3: The static boundary gate

**Files:**
- Create: `scripts/verify_macro_boundary.py`
- Modify: `scripts/embed_sql_macros.py` (harden `load_manifest` — see Step 0)
- Modify: `spec/layout/layout.yaml` (one stale comment — see Step 0)
- Modify: `CMakeLists.txt` (drop `OUTPUT_VARIABLE` — see Step 0)
- Modify: `.github/workflows/MainDistributionPipeline.yml` (new job, and wire Task 2's manifest test into CI)
- Test: `scripts/test_verify_macro_boundary.py`, `scripts/test_embed_manifest.py` (extend for the new validation)

**Interfaces:**
- Consumes: Task 2's `sql_macros` manifest.
- Produces: `scripts/verify_macro_boundary.py`, exit 0 when clean and exit 1 naming each violating edge. A violation is a macro defined in a `core` file that references a macro owned by an `addon` file, following transitive edges.

This gate exists because `LOAD` cannot catch the mistake. Measured: `CREATE MACRO` binds eagerly and rejects an unknown function, **except** when the body's `FROM` is `query_table(<parameter>)`, which defers the whole body. Every `_from` macro uses `query_table()`, so a mis-split in the core's foreign-AST surface fails inside a user's query rather than at load.

The script must strip SQL comments **and** single-quoted string literals before matching. A comment-only stripper reported two edges that were prose inside an error message.

- [ ] **Step 0: Harden `load_manifest`, because this task is the first to read `side`**

Task 2's review found `load_manifest` validates less than "fail loud" implies,
and this task's gate is the first consumer of the `side` field. MEASURED
against the committed code:

| input | `load_manifest` returns |
|---|---|
| `sql_macros: []` | `[]` — the generator then writes a valid header with **zero** macros, silently |
| a duplicate filename | `['a.sql', 'a.sql']` — the file is registered twice |
| an entry with no `file` key | bare `KeyError: 'file'`, naming no index |
| `side: adon` (a typo) | accepted silently |
| no `side` at all | accepted silently |

The last two matter most: your gate filters on `side`, so a typo there makes the
gate silently skip a file instead of checking it — a hole in a guard, which is
the one defect class this plan exists to remove.

In `scripts/embed_sql_macros.py`, add one validation loop to `load_manifest`
that raises, naming the offending entry and its index, on: an empty list; a
duplicate filename; an entry missing `file`; and a `side` that is not exactly
`core` or `addon`. Extend `scripts/test_embed_manifest.py` with a case per rule,
and run the planted failure for each so you have seen every new raise fire.

Three small corrections while you are in these files:

1. **Drop `OUTPUT_VARIABLE` from the `execute_process` call in `CMakeLists.txt`**,
   keeping `ERROR_VARIABLE`. Task 2 added both because this plan told it to, and
   that was my error: the generator's failures all go to stderr, so stdout adds
   nothing to the warning, while capturing it silences the "Generated …" and
   "macro docs extracted" progress lines during every successful build. Keep the
   message text; just stop swallowing stdout.
2. **`spec/layout/layout.yaml`'s comment saying nothing reads `side` yet** becomes
   untrue the moment your gate lands. Update that sentence.
3. **`scripts/test_embed_manifest.py`'s output is not pristine** — measured: the
   generator's progress lines interleave with its PASS lines, because case 3
   calls `generate_header`. Wrap that call in `contextlib.redirect_stdout`.

- [ ] **Step 1: Write the failing gate test**

Create `scripts/test_verify_macro_boundary.py`. Four cases, each against a temporary fixture tree:
- a clean tree exits 0;
- a `core` file calling an `addon`-owned macro exits 1 and names both macros;
- a transitive edge, core to core to addon, exits 1;
- **a non-vacuity guard:** a tree where the parser finds zero macros exits 1 with "found no macro definitions; the pattern is wrong, not the code". `scripts/verify_native_dispatch.py` and `scripts/verify_layout_invariance.py` both carry this shape, and 054 §10b records that it is the only reason two silent holes were caught.

- [ ] **Step 2: Run it to verify it fails**

Run: `python3 -I scripts/test_verify_macro_boundary.py`
Expected: FAIL — the script does not exist.

- [ ] **Step 3: Implement `scripts/verify_macro_boundary.py`**

Read the manifest for file-to-side tags. Parse `CREATE [OR REPLACE] [TEMP] MACRO <name>(` to find definitions and their bodies, after stripping comments and `'...'` literals, handling the `''` escape. Build macro-to-macro edges, take the transitive closure, and report every edge whose source file is `core` and whose target's owner is `addon`.

Measured expectation on today's tree: 96 macros, 27 macro-to-macro edges, 0 violations.

- [ ] **Step 4: Run the gate test to verify it passes**

Run: `python3 -I scripts/test_verify_macro_boundary.py`
Expected: PASS, all four cases.

- [ ] **Step 5: Run the gate against the real tree**

Run: `python3 -I scripts/verify_macro_boundary.py`
Expected: exit 0, and a summary line reporting 96 macros and 27 edges. If the macro count is not 96, the parser is wrong.

- [ ] **Step 6: Add the CI job**

Add a job named `macro boundary holds` to `.github/workflows/MainDistributionPipeline.yml`, in the style of the existing `Embedded SQL macros header in sync` job at line 312: checkout, then run the script. It needs no build.

It reads `spec/layout/layout.yaml`, so give it the same PyYAML step the other
YAML-reading jobs use:

```yaml
      - name: Ensure PyYAML is available
        run: python3 -c "import yaml" || python3 -m pip install --break-system-packages pyyaml
```

**Also prove the gate can fail in CI**, following the `Prove the gate can fail`
steps that `module-abi-sync` (line 346) and `layout-invariance` (line 390)
already carry. A gate whose red path CI has never executed is not a gate.

**And wire Task 2's manifest test in.** `scripts/test_embed_manifest.py` runs
nowhere in CI today, so it is a manual gate only. Run it from this same job.

- [ ] **Step 7: Commit**

```bash
git add scripts/verify_macro_boundary.py scripts/test_verify_macro_boundary.py \
        scripts/embed_sql_macros.py scripts/test_embed_manifest.py \
        spec/layout/layout.yaml CMakeLists.txt \
        .github/workflows/MainDistributionPipeline.yml
git commit -m "feat(057): static gate for the core/add-on macro boundary (M2 stage 3)"
```

---

### Task 4: The five relational `_from` variants

**Files:**
- Modify: `src/sql_macros/relational_operators.sql`
- Test: `test/sql/macro_from_parity_relational.test`

**Interfaces:**
- Consumes: Task 2's manifest, Task 3's gate.
- Produces, each a table macro whose first argument is a table name resolved with `query_table()`:
  - `ast_has_from(ast_table, ancestor_type, descendant_type, descendant_name := NULL)`
  - `ast_not_has_from(ast_table, ancestor_type, descendant_type, descendant_name := NULL)`
  - `ast_inside_from(ast_table, descendant_type, ancestor_type, ancestor_name := NULL, descendant_name := NULL)`
  - `ast_precedes_from(ast_table, node_type, before_type, before_name := NULL)`
  - `ast_follows_from(ast_table, node_type, after_type, after_name := NULL)`

Each drops the `language` parameter its twin carries, because a parsed table has no language to choose. Each existing path-taking macro becomes a thin wrapper that parses with `read_ast` and delegates to the `_from` form, which is how `ast_select` relates to `ast_select_from` (`src/sql_macros/css_selectors.sql:83`) and `ast_to_blocks` to `ast_to_blocks_from`.

Rewriting the five existing bodies as wrappers changes their `macro_definition`,
so Task 1's oracle must gain an exemption for each. The measured parity gap is
**nine**: these five, **three** in Task 5, and one in Task 6. Each task adds only
its own exemptions, so the oracle keeps full strength over every macro not yet
touched. Do not pre-exempt Task 5's or Task 6's twins here.

- [ ] **Step 1: Write the failing parity tests**

Create `test/sql/macro_from_parity_relational.test`. For each of the five, three queries:

```
require sitting_duck

# the baseline must not be empty, or EXCEPT proves nothing
query I
SELECT count(*) > 0 FROM ast_has('test/data/python/sample_app.py', 'class_definition', 'function_definition');
----
true

statement ok
CREATE TABLE t AS SELECT * FROM read_ast('test/data/python/sample_app.py');

# both directions must be empty
query I
SELECT count(*) FROM (
  SELECT * FROM ast_has('test/data/python/sample_app.py', 'class_definition', 'function_definition')
  EXCEPT SELECT * FROM ast_has_from('t', 'class_definition', 'function_definition')
);
----
0
```

Add the symmetric `EXCEPT` in the other direction, which catches a `_from` form that returns extra rows.

Then the two Review Focus cases:
- `ast_has_from('no_such_table', …)` must raise, with the table name in the message. Use `statement error`.
- an empty but well-formed AST table must return 0 rows without raising.

- [ ] **Step 2: Run the tests to verify they fail**

Expected: FAIL — `ast_has_from` does not exist.

- [ ] **Step 3: Add the five `_from` macros, each with a `-- <name>: <summary>` banner**

Write each `_from` body by moving the logic out of its path-taking twin, replacing `read_ast(source, language)` with `query_table(ast_table)`. **Leave the five twins untouched in this step.**

- [ ] **Step 4: Run the parity test NOW, before the twins become wrappers**

Regenerate, rebuild, run `test/sql/macro_from_parity_relational.test`.
Expected: PASS.

**This is the only run of this test that proves anything, and the reason the
step order matters.** Right now `ast_has` and `ast_has_from` hold two
independent bodies, so an `EXCEPT` between them compares two implementations.
After Step 6 the twin delegates to the `_from` form, both sides of the `EXCEPT`
evaluate the same body, and the comparison is self-referential forever after.

Record the pinned expectation from this run into the test, so it keeps power
once the twin is a wrapper: for each of the five, the absolute row count and
`md5(string_agg(node_id::VARCHAR, ',' ORDER BY node_id))` of the `_from` result.

- [ ] **Step 5: Prove the parity test discriminates, while it still can**

Change one `_from` macro to filter one extra row. Regenerate, rebuild, re-run.
Expected: FAIL on the `EXCEPT` count. Revert, rebuild, confirm PASS.

Doing this after Step 6 would plant the defect in the body that BOTH sides call,
leave the `EXCEPT` empty, and prove the guard works when it had in fact gone
blind.

- [ ] **Step 6: Rewrite the five path-taking macros as wrappers**

Each becomes a `read_ast` CTE delegating to its `_from` twin, following `ast_select`'s shape.

- [ ] **Step 7: Add this task's five exemptions to Task 1's oracle**

In `test/sql/macro_split_oracle.test`, record that these five macros' `macro_definition` changes by design, naming each and saying why. Raise the expected counts to 45 scalar / 57 table / 102 rows. All five new macros are table macros, so only the table count moves. Exempt only these five — Task 5 and Task 6 add their own.

- [ ] **Step 8: Regenerate, rebuild, run both test files**

Expected: `macro_from_parity_relational.test` PASS, including the Step 4 pinned counts; `macro_split_oracle.test` PASS at 45 / 57 / 102.

- [ ] **Step 9: Run the boundary gate**

Run: `python3 -I scripts/verify_macro_boundary.py`
Expected: exit 0, now reporting 101 macros.

- [ ] **Step 10: Commit**

```bash
git add src/sql_macros/relational_operators.sql src/include/embedded_sql_macros.hpp \
        test/sql/macro_from_parity_relational.test test/sql/macro_split_oracle.test
git commit -m "feat(057): _from variants for the five relational operators (M2 stage 3)"
```

---

### Task 5: The three navigation `_from` variants

**Files:**
- Modify: `src/sql_macros/tree_navigation.sql`
- Test: `test/sql/macro_from_parity_navigation.test`

**Interfaces:**
- Consumes: Task 4's wrapper pattern.
- Produces:
  - `ast_in_range_from(ast_table, range_start, range_end)`
  - `ast_containing_line_from(ast_table, line_num)`
  - `ast_definitions_from(ast_table)`

`ast_definitions_from` takes only the table, because its twin's remaining parameter is `language`.

- [ ] **Step 1: Write the failing parity tests**

Same three-query shape as Task 4, plus the two Review Focus cases this task owns:
- **a table whose required columns are present but NULL.** Build it as `SELECT * REPLACE (NULL AS parent_id) FROM read_ast(…)` and assert the macro either errors or returns a result the test pins. A silently different answer must not pass.
- **a multi-file table.** Parse a glob matching at least two files into one table, and assert the `EXCEPT` is empty in both directions against the glob-taking twin, so equivalence holds across `file_path` values and not only within one.

- [ ] **Step 2: Run to verify they fail**

Expected: FAIL — the three macros do not exist.

- [ ] **Step 3: Add the three `_from` macros with banners. Leave the twins untouched**

- [ ] **Step 4: Run the parity tests NOW, before the twins become wrappers**

Regenerate, rebuild, run `test/sql/macro_from_parity_navigation.test`.
Expected: PASS. As in Task 4 Step 4, this is the only run that compares two
independent bodies; after Step 6 the `EXCEPT` is self-referential. Record the
pinned row count and `node_id` hash for each of the three into the test.

- [ ] **Step 5: Prove the multi-file case discriminates, while it still can**

Restrict one `_from` macro to a single `file_path`, rebuild, re-run. Expected:
FAIL on the multi-file `EXCEPT`. Revert and confirm PASS.

- [ ] **Step 6: Rewrite the three twins as wrappers**

Following Task 4's shape.

- [ ] **Step 7: Add this task's three exemptions, regenerate, rebuild, run the tests**

Expected: PASS. `macro_split_oracle.test` PASS at 45 scalar / 60 table / 105 rows.

- [ ] **Step 8: Commit**

```bash
git add src/sql_macros/tree_navigation.sql src/include/embedded_sql_macros.hpp \
        test/sql/macro_from_parity_navigation.test test/sql/macro_split_oracle.test
git commit -m "feat(057): _from variants for three navigation accessors (M2 stage 3)"
```

---

### Task 6: `ast_source_of_from`, which needs a second argument

**Files:**
- Modify: `src/sql_macros/file_utilities.sql`
- Test: `test/sql/macro_from_parity_source_of.test`

**Interfaces:**
- Consumes: Task 5.
- Produces: `ast_source_of_from(ast_table, files, target_name, kind := NULL)`.

This one breaks the one-argument parity pattern, and must. Measured at `src/sql_macros/file_utilities.sql:113-120`, `ast_source_of` joins `read_text(file_patterns)` to get the lines, and a node table carries no source text. `read_text` also accepts only literal arguments. So the `files` argument comes along, exactly as `ast_unparse_exact_from(ast_table, files, language := NULL, file_path := NULL)` already does. Dropping the `source` column instead would break the `EXCEPT` oracle, which needs both twins to return the same columns.

- [ ] **Step 1: Write the failing parity test**

Both twins must return the same six columns: `file_path`, `name`, `definition_kind`, `start_line`, `end_line`, `source`. Assert a non-zero baseline, then `EXCEPT` in both directions, passing the same glob as `files`.

- [ ] **Step 2: Run to verify it fails**

- [ ] **Step 3: Add `ast_source_of_from` with a banner. Leave `ast_source_of` untouched**

- [ ] **Step 4: Run the parity test NOW, before the twin becomes a wrapper**

Regenerate, rebuild, run `test/sql/macro_from_parity_source_of.test`.
Expected: PASS. As in Task 4 Step 4, this is the only run that compares two
independent bodies. Record the pinned row count and a hash of the sorted
`name` column into the test.

- [ ] **Step 5: Prove it discriminates, while it still can**

Make `ast_source_of_from` drop one row, rebuild, re-run. Expected: FAIL on the
`EXCEPT`. Revert, rebuild, confirm PASS.

- [ ] **Step 6: Rewrite `ast_source_of` as a wrapper**

- [ ] **Step 7: Add the one exemption, regenerate, rebuild, run the test**

Expected: PASS. `macro_split_oracle.test` PASS at 45 scalar / 61 table / 106 rows.

- [ ] **Step 8: Commit**

```bash
git add src/sql_macros/file_utilities.sql src/include/embedded_sql_macros.hpp \
        test/sql/macro_from_parity_source_of.test test/sql/macro_split_oracle.test
git commit -m "feat(057): ast_source_of_from, carrying a files argument (M2 stage 3)"
```

---

### Task 7: The core-alone test, driven by a committed fixture

**Files:**
- Create: `test/data/fixtures/core_alone_ast.csv`
- Create: `test/sql/core_alone.test`
- Create: `scripts/test_core_only_build.sh`
- Modify: `.github/workflows/MainDistributionPipeline.yml` (new build leg)

**Interfaces:**
- Consumes: every core macro from Tasks 1 to 6.
- Produces: a build-and-test script proving the core works with no tree-sitter language compiled in.

**Why a fixture and not a live parse.** The RFC gives duckling the DuckDB SQL module and no tree-sitter languages. Measured, that module cannot exercise much of core: `src/unparse_rules.cpp` has 53 rule-map entries and neither `duckdb` nor `sql` is among them; `languages/duckdb/` holds no `.def` files; and `src/sql_macros/ast_unparse.sql:502-515` carries a note headed "NOT SERVED: the `duckdb` language adapter", because every node reports `start_byte = end_byte = 0` and the tree is an AST rather than a CST. A dedicated guard errors. `bugs/011` separately records that SQL `CREATE MACRO` is not classified as a function definition, so `ast_definitions` and the `ast_has` family may return 0 rows there for unrelated reasons.

A test that accepts 0 rows is the vacuous gate `~/.CLAUDE.md` forbids. So the fixture is a committed AST table from a tree-sitter language, loaded into a build that has no tree-sitter language at all. That is Teague's case 3, and it is what the `_from` variants exist for.

`scripts/test_language_subset.sh` cannot be reused: it is hardcoded to `python;cpp` and asserts that a compiled-out language errors.

- [ ] **Step 1: Generate and commit the fixture**

From a full build, write `test/data/fixtures/core_alone_ast.csv` as the complete `read_ast('test/data/python/sample_app.py')` result, all 22 columns. Record in a header comment which commit and which source file produced it, so a drift is explainable.

- [ ] **Step 2: Write the failing core-alone test**

Create `test/sql/core_alone.test`. Load the fixture into a table, then **call** every core macro against it, asserting a non-zero row count for each one that can return rows. Enumerate the core macros by name, so adding a core macro without covering it is a visible omission rather than a silent one.

For the macros that remain path-only after Task 6, add an explicit exemption block naming each and giving its reason. **No exemption may be "it returns 0 rows."**

Add the Review Focus case this task owns: assert that an unparse macro given a language with **no** rules in `ast_unparse_rules()` raises rather than returning text. Emitting normalised text there would violate the textual law.

- [ ] **Step 3: Run it against a full build to verify the assertions are right**

Expected: PASS. This proves the test is not vacuous before it is pointed at a restricted build.

- [ ] **Step 4: Measure whether a duckdb-only build configures and links**

Run CMake with `-DSITTING_DUCK_LANGUAGES=duckdb`. **This is unmeasured and may fail.** `cmake/BuiltinLanguages.cmake` validates that the requested set is declared and non-empty, and `duckdb` is declared, so it is expected to pass validation. If it does not configure, stop and report: the fallback is a two-language subset, and the spec's §9.3 needs amending rather than working around.

- [ ] **Step 5: Write `scripts/test_core_only_build.sh`**

Configure with `-DSITTING_DUCK_LANGUAGES=duckdb`, build, then run `test/sql/core_alone.test` against that binary. Print the language list the binary reports, so the script cannot pass against a full build by accident.

- [ ] **Step 6: Run it, and prove it is not passing by accident**

Run the script. Expected: PASS, and the printed language list contains `duckdb` and nothing else. Then point it at the full build and confirm the language-list assertion fails, so the guard is proven to discriminate.

- [ ] **Step 7: Add the CI leg and commit**

Add a build leg running `scripts/test_core_only_build.sh`, following the existing `Language subset build (python;cpp)` job.

```bash
git add test/data/fixtures/core_alone_ast.csv test/sql/core_alone.test \
        scripts/test_core_only_build.sh .github/workflows/MainDistributionPipeline.yml
git commit -m "test(057): the core runs with no tree-sitter language, from a fixture (M2 stage 3)"
```

---

## Out of scope, recorded

The spec's §14 found four gaps while assessing deprecation, and §14's closing line keeps them out of stage 3. None blocks this plan.

- `ast_resolve` has no behavioural test. Measured not superseded: 166 rows against `ast_find_references`'s 2, with two columns its sibling lacks.
- 14 of the 31 semantic predicates have no direct test. Task 7 exercises them in the core-only context, which closes most of the gap as a side effect; a table-driven test over all 31 is the complete fix.
- `duck_blocks` has 45 test call sites and zero docs. The inverse defect.
- `ast_select_rules` and `ast_select_list` wrap `ast_select`, which has changed a lot, and neither has been checked against current selector semantics.

`src/sql_macros/CMakeLists.txt` is dead code carrying its own two-file macro
list, and spec §10.2 asks for it to be deleted "in its own change". It is not a
task here, deliberately: nothing includes it, so deleting it changes no build,
and bundling it would put an unrelated deletion inside a commit whose oracle is
about macro text. **A builder who sees it while editing the real list at
`scripts/embed_sql_macros.py` should leave it alone and not follow it.**

Two documentation decisions are also outstanding and do not block: formally descoping `core/` from M2 (`RFC:241`, `054:30`), and correcting `054:190` by one clause.
