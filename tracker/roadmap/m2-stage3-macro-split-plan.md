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

The 8 moving macros occupy these line ranges in today's file, which are not contiguous: 107-139, 309-413, 414-491, 492-575, 576-696, 744-856, 857-934, 935-950.

- [ ] **Step 1: Capture the oracle from the current build**

Run, and save the output to `build/macro-oracle/macros-before.txt` (create the directory; `build/` is gitignored):

```sql
LOAD sitting_duck;
SELECT function_name, function_type, parameters, macro_definition
FROM duckdb_functions() WHERE database_name = 'memory'
ORDER BY function_name, function_type;
```

Expected: **97 rows** = 45 scalar macros and 52 table macros. The 45 is 44 defined in `src/sql_macros/` plus the `ast_dispatch_predicate` stub that `src/ast_sql_macros.cpp:181` creates, which is also a scalar macro. Do not expect 44.

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

Re-run Step 1's query into `build/macro-oracle/macros-after.txt` and diff it against `build/macro-oracle/macros-before.txt` (create the directory; `build/` is gitignored).
Expected: **no differences.** A difference means a body was edited.
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
- Test: `test/sql/macro_split_oracle.test` (extend)

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

In `scripts/embed_sql_macros.py`, replace the hardcoded list at lines 176-190 with a read of `spec/layout/layout.yaml`'s `sql_macros` key.

**Also remove the silent skip.** Lines 211 and 272 wrap each file in `if os.path.exists(filepath)`, so a manifest naming a missing file is dropped without a word and the extension loses those macros quietly. Raise instead, naming the file.

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
git add spec/layout/layout.yaml scripts/embed_sql_macros.py scripts/test_embed_manifest.py
git commit -m "feat(057): the macro manifest in layout.yaml owns registration order (M2 stage 3)"
```

---

### Task 3: The static boundary gate

**Files:**
- Create: `scripts/verify_macro_boundary.py`
- Modify: `.github/workflows/MainDistributionPipeline.yml` (new job)
- Test: `scripts/test_verify_macro_boundary.py`

**Interfaces:**
- Consumes: Task 2's `sql_macros` manifest.
- Produces: `scripts/verify_macro_boundary.py`, exit 0 when clean and exit 1 naming each violating edge. A violation is a macro defined in a `core` file that references a macro owned by an `addon` file, following transitive edges.

This gate exists because `LOAD` cannot catch the mistake. Measured: `CREATE MACRO` binds eagerly and rejects an unknown function, **except** when the body's `FROM` is `query_table(<parameter>)`, which defers the whole body. Every `_from` macro uses `query_table()`, so a mis-split in the core's foreign-AST surface fails inside a user's query rather than at load.

The script must strip SQL comments **and** single-quoted string literals before matching. A comment-only stripper reported two edges that were prose inside an error message.

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

- [ ] **Step 7: Commit**

```bash
git add scripts/verify_macro_boundary.py scripts/test_verify_macro_boundary.py \
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

Rewriting the five existing bodies as wrappers changes their `macro_definition`, so Task 1's oracle must gain an exemption list naming exactly these five plus the four in Task 5 and the one in Task 6.

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

Write each `_from` body by moving the logic out of its path-taking twin, replacing `read_ast(source, language)` with `query_table(ast_table)`.

- [ ] **Step 4: Rewrite the five path-taking macros as wrappers**

Each becomes a `read_ast` CTE delegating to its `_from` twin, following `ast_select`'s shape.

- [ ] **Step 5: Add the nine exemptions to Task 1's oracle**

In `test/sql/macro_split_oracle.test`, record that these macros' `macro_definition` changes by design, naming each and saying why. Raise the expected counts to 45 scalar / 57 table / 102 rows. All five new macros are table macros, so only the table count moves.

- [ ] **Step 6: Regenerate, rebuild, run both test files**

Expected: `macro_from_parity_relational.test` PASS; `macro_split_oracle.test` PASS at 45 / 57 / 102.

- [ ] **Step 7: Prove the parity test discriminates**

Change one `_from` macro to filter one extra row, rebuild, re-run. Expected: FAIL on the `EXCEPT` count. Revert, rebuild, confirm PASS.

- [ ] **Step 8: Run the boundary gate**

Run: `python3 -I scripts/verify_macro_boundary.py`
Expected: exit 0, now reporting 101 macros.

- [ ] **Step 9: Commit**

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

- [ ] **Step 3: Add the three `_from` macros with banners, and rewrite the twins as wrappers**

- [ ] **Step 4: Regenerate, rebuild, run the tests**

Expected: PASS. `macro_split_oracle.test` PASS at 45 scalar / 60 table / 105 rows.

- [ ] **Step 5: Prove the multi-file case discriminates**

Restrict one `_from` macro to a single `file_path`, rebuild, re-run. Expected: FAIL on the multi-file `EXCEPT`. Revert and confirm PASS.

- [ ] **Step 6: Commit**

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

- [ ] **Step 3: Add `ast_source_of_from` with a banner, and rewrite `ast_source_of` as a wrapper**

- [ ] **Step 4: Regenerate, rebuild, run the test**

Expected: PASS. `macro_split_oracle.test` PASS at 45 scalar / 61 table / 106 rows.

- [ ] **Step 5: Commit**

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
