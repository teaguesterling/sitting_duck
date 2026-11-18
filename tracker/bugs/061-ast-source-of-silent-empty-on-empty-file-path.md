# 061 — `ast_source_of` returns a silent empty result when `file_path` is the empty string, which the `duckdb` adapter always produces

**Status: FIXED** on branch `fix/061-duckdb-adapter-file-path`, 2026-11-11, on
Teague's instruction ("Can you fix that?"). The adapter now reports the real
path. Read "What fixing it exposed" before closing this entry: the fix was
measured to turn one macro's loud failure into a WRONG ANSWER, and that needed a
second change in the same branch.

**MEASURED after the fix**, `build/release/duckdb` on this branch:

| call | `file_path` | was |
|---|---|---|
| `read_ast('test/data/sql/example.sql','duckdb')` | `test/data/sql/example.sql` | `''` |
| `parse_ast('SELECT 1','duckdb')` | `<inline>` | `''` |
| `read_ast('test/data/python/simple.py')` (control) | unchanged | unchanged |

The four-column pin moved from `1  20  20  0` to `blank_paths = 0`, with
`control_defs = 1` and `construct_flagged = 20` unchanged — so the change is in
the `file_path` column and nowhere else. `parse_ast` now reports the SAME
`<inline>` sentinel every tree-sitter language reports, which was not designed
for: it falls out of passing the caller's value through instead of inventing
one, and it makes the macros' existing `<inline>` messages apply.

**Status update (058 fix):** the loud guard landed in the shared body via 058
(the `__sof_guard` CTE in `src/sql_macros/file_utilities.sql`), so `ast_source_of` over a `duckdb`
parse RAISED instead of returning 0 rows (core_alone.test section 7).

**Status (original):** Not fixed. Measured, pinned as a KNOWN DEFECT, and deliberately not
fixed inside M2 stage 3 task 7 — see "Why it is not fixed here". Ships to the
public `duckdb/community-extensions` registry with the stage.

## What fixing it exposed

**061 was shielding tracker/bugs/043 by accident, and removing the shield was
measured to be worse than the bug.** With a real `file_path` the `read_text`
join in `ast_source_of_from` SUCCEEDS, and the next thing the macro uses is the
node's line range. 043 is the defect that every duckdb node reports
`start_line = end_line = 1`. Measured on the fixture: `degenerate_lines = 20`,
`count(DISTINCT start_line) = 1`, while `customers` is defined on line 15.

So immediately after the adapter fix, and before any guard existed:

```
SELECT start_line, end_line, encode(source)::VARCHAR
FROM ast_source_of('test/data/sql/example.sql','customers', language := 'duckdb');
-- 1  1  "   1: -- Example SQL file for testing"
```

That is the file's comment header returned as the definition of `customers`,
with no error. A silent wrong answer is worse than the silent empty that
`tracker/features/044-v2-m1-contract.md:16` forbids.

**The second change, in the same branch.** `__sof_guard` gained a last branch
that raises and names 043, keyed on `language = 'duckdb'`. It is keyed on the
language and not on the line values because `start_line = end_line = 1` is
legitimate for a real one-line definition, so a data-shaped guard would refuse
correct input. The node row carries no "this adapter has no line information"
column, which is what would make a general guard possible. **DELETE THAT BRANCH
WHEN 043 LANDS** — the comment above it says so.

**`ast_patch` is exposed and is NOT guarded.** It takes an edit set, not an AST
table, so it has no `language` column, and an edit at `start_line = end_line = 1`
is indistinguishable from a legitimate insertion at the top of a file. Before
this fix an empty `file_path` made its join miss; now the join succeeds, so an
edit set built from a duckdb AST would patch the wrong lines. That exposure
belongs to 043 and cannot be closed from inside `ast_patch`.

## The fix, as landed

`DuckDBAdapter::ParseSQL` took a second parameter, `file_path`, and became the
single writer of `source.file_path` on the native path. The prescription earlier
in this file said to assign at each of the three call sites with no signature
change; that was not followed, for two measured reasons:

1. The three sites all funnel through `ParseSQL`, so one write replaces three.
2. `CreateErrorResult` wrote no `file_path` either, so an error row under
   `ignore_errors` could not name its file. Stamping the result inside
   `ParseSQL` covers the error path too, which the three-site version would not.

The parameter has NO DEFAULT. A defaulted parameter is exactly how a fourth call
site would reintroduce this bug silently; without one the compiler refuses it.

Files: `src/include/duckdb_adapter.hpp`, `languages/duckdb/duckdb_adapter.cpp`
(the `ParseSQL` body, the removed `result.source.file_path = ""` in
`ConvertStatementsToAST`, and the `GetParsingFunction()` lambda),
`src/language_adapter_registry_init.cpp` (both `SD_BUILTIN_LANGUAGE_NATIVE`
dispatch macros), `src/sql_macros/file_utilities.sql`,
`src/include/embedded_sql_macros.hpp` (regenerated), and
`test/sql/core_alone.test`.

**Which path serves `read_ast` is now MEASURED**, and this file previously said
it was not. `read_ast` reaches the `GetParsingFunction()` lambda through
`src/ast_parsing_task.cpp:78`; `parse_ast` reaches the registry dispatch through
`src/unified_ast_backend.cpp:133`. Both were fixed, and the two different
`file_path` values above prove both paths run.

**Verification.** 7 affected test files, 600 assertions, 0 failures;
`core_alone.test` alone completes at 191 assertions. The macro boundary gate is
unchanged at 105 macros / 69 edges / 0 violations. The built extension was
grepped for the new guard text (present) and the removed claim (absent), with a
sentinel control returning 0.
**Found by:** M2 stage 3 task 7, while calling every core macro in a build whose
only language is `duckdb`. Given its own id by the task-7 code review
(2026-11-10), which measured that neither of the two nearby bugs covers it.
Every figure below was measured against `build/release/duckdb` at `ca0b7fd`.

**THIS IS NOT `tracker/bugs/058`, and the distinction is the reason this file
exists.** 058 is titled "returns a silent 0 when `files` and `ast_table`
disagree on a **path spelling**", and the fix it prescribes is a **normalising
join** — strip a leading `./`, fold backslashes. An empty string normalises to
an empty string and still joins to nothing. A reader who fixes 058 and closes
it will reasonably believe this door is shut; it will not be. The two share one
line of code and one symptom, and have different causes.

**IT IS NOT `tracker/bugs/043` EITHER.** 043 covers the same adapter
hardcoding `source_start_line`, `source_end_line`, `source_start_column` and
`source_end_column` (`languages/duckdb/duckdb_adapter.cpp:1051-1055`). Measured:
`grep -n file_path tracker/bugs/043-duckdb-nodes-report-no-source-location.md`
returns **nothing**. The empty `file_path` is outside its scope. 043 is the
right owner for the *line numbers*, and this file is cross-referenced from
there in spirit but does not amend it.

## What is wrong

`ast_source_of` and `ast_source_of_from` locate a definition's source TEXT by
joining the node table's `file_path` against `read_text()`'s `filename`. AS
MEASURED when this entry was written (before 058's fix) the join was:

    src/sql_macros/file_utilities.sql:164
        JOIN read_text(files) fc ON fc.filename = m.file_path

an **inner** join on **exact string equality**. When `file_path` is the empty
string the join matched nothing, for every row, and the caller got zero rows
with no error, no warning and no diagnostic column. NOW (after 058) the
`file_contents` join in `ast_source_of_from` normalises both sides and the
`__sof_guard` CTE raises on an empty or NULL `file_path`, naming this entry; the
adapter still writes the empty path.

The `duckdb` adapter always produces an empty `file_path` — including through
`read_ast()`, which was handed the path and knows it:

| call | `file_path` on every node | nodes |
|---|---|---|
| `read_ast('test/data/sql/example.sql','duckdb')` | `''` (empty string) | 20 |
| `parse_ast('SELECT 1','duckdb')` | `''` | 6 |
| `read_ast('test/data/python/sample_app.py')` (control) | `test/data/python/sample_app.py` | 1246 |

The control is the point: a tree-sitter language populates the column, so this
is adapter behaviour and not a property of `read_ast`. Note also that the
duckdb adapter does not even use the `'<inline>'` sentinel that
`ast_unparse.sql:729-733` has a dedicated error message for — it uses the empty
string, which that message cannot recognise.

## The measurement

One statement, four columns, so the zero has three controls beside it and
cannot be produced by a broken query:

```sql
SELECT (SELECT count(*) FROM ast_definitions('test/data/sql/example.sql', language := 'duckdb')),
       (SELECT count(*) FILTER (is_construct(flags)) FROM read_ast('test/data/sql/example.sql','duckdb')),
       (SELECT count(*) FILTER (file_path = '' OR file_path IS NULL) FROM read_ast('test/data/sql/example.sql','duckdb')),
       (SELECT count(*) FROM ast_source_of('test/data/sql/example.sql','customers', language := 'duckdb'));
-- 1    20    20    0
```

Read it left to right:

- **1** — the definition the macro is asked for IS in the tree.
  `ast_definitions` finds `customers`, a `create_table` node with
  `semantic_type = DEFINITION_CLASS`.
- **20** — the `is_construct(flags)` filter the macro applies passes on every
  node, so the filter chain is not what drops the row.
- **20** — every node carries an empty `file_path`. This is the cause.
- **0** — and the macro returns nothing, silently.

So the search succeeded, the filter passed, and the join threw the answer away.

## The root cause, measured by reading the code (2026-11-11)

The empty `file_path` is not a decision anywhere. It is a parameter that three
call sites accept and discard.

- `src/include/unified_ast_backend_impl.hpp:114` does
  `result.source.file_path = file_path`. It is the ONLY correct writer in the
  tree, and it is on the tree-sitter path. That is why every tree-sitter
  language carries a real path.
- The native path never reaches it. `SD_BUILTIN_LANGUAGE_NATIVE` in
  `src/language_adapter_registry_init.cpp:72-76`, and again at `:105-109` in the
  legacy overload, calls `adapter.ParseSQL(content)` and drops the `file_path`
  argument the function was given.
- `DuckDBAdapter::GetParsingFunction()`'s lambda,
  `languages/duckdb/duckdb_adapter.cpp:1240-1245`, takes `file_path` in its
  parameter list and calls `ParseSQL(content)`, dropping it the same way. That
  lambda path is invoked by `src/ast_parsing_task.cpp:78`, which fetches it and
  calls it at `:81-82`. (`src/language_adapter.cpp:984` also calls
  `GetParsingFunction()`, but only as a capability check: the comment above it
  says "Test that the parsing function is available without calling it", so it is
  not evidence that anything parses.) WHICH of the two paths serves
  `read_ast(..., 'duckdb')` is UNMEASURED.
- `languages/duckdb/duckdb_adapter.cpp:324` then sets
  `result.source.file_path = ""`. (The comment "Source location (placeholder
  values for now)" at `:1051` of the same file is 727 lines away and is about the
  node line and column fields, not `file_path`; it is not evidence either way.)

So the fix is plumbing, not design: set `result.source.file_path = file_path`
after the native parse, at each of the three sites. No signature changes, no
header changes, about six lines.

**Blast radius, measured.** 15 test files parse with the `duckdb` language.
Three of them mention `file_path` (`test/sql/ast_unparse_exact.test`,
`test/sql/core_alone.test`, `test/sql/ast_copy_format.test`) and two pin an
empty one. Those pins move when the fix lands.

**Why this is not that fix.** M2 stage 3 is a macro reorganisation whose claim is
that no behaviour changed. Giving a whole language real file paths changes
`read_ast` output for every `duckdb` parse, which is a wider user-visible change
than the split itself, and it needs its own review and its own full-suite run.
It also makes 058's guard moot on this path: with a real `file_path`, the join
finds the file and `ast_source_of` returns source instead of raising. Teague
asked for the root cause to be found, not for it to ride along here
(2026-11-11: "Raise with real cause but also look into fixing the adapter").

## The contract it violates

- `tracker/features/044-v2-m1-contract.md:16` — the conformance kit requires
  "raw-AST-join agreement, and **no silent-empty (#89)**".
- `tracker/roadmap/m2-stage3-macro-split-plan.md:60` — "The #89 principle makes
  0 rows mean 'searched correctly, not there'." Here 0 rows means "I found it
  and could not locate its file", which is the case the contract forbids.
- Same clause 058 cites, reached through a different input.

## Why it is not fixed here

Two reasons, and the second is the binding one:

1. Task 7 is a TEST task. It changed no `src/` file by design, and
   `git diff --stat ca0b7fd..HEAD -- src/` is empty.
2. **The join lives in the shared body.** Task 6 rewrote `ast_source_of` as a
   thin wrapper over `ast_source_of_from`, so `file_utilities.sql:164` (then; now the
   `file_contents` join in `ast_source_of_from`) is the
   only copy and both macros reach it. Any change there moves the TWIN's
   behaviour, and M2 stage 3 is constrained to no behaviour change for existing
   macros. This is the same structural reason 058 gives for its own deferral,
   and it applies identically.

## The intended shape: loud failure

The readability guard 058 already prescribes is most of the fix, but it must be
written so that an EMPTY path is caught rather than normalised away:

1. **Treat an empty or NULL `file_path` as unlocatable and RAISE, naming it.**
   Not "no match" — the node exists and its file is unknown. The register to
   follow is `src/sql_macros/ast_unparse.sql:707-709`, which already raises for
   "node rows have an empty file_path, so their bytes cannot be located", and
   `:729-733`, which raises a dedicated message for the `'<inline>'` sentinel.
   `ast_source_of` needs the same two messages, and the empty-string case needs
   to be recognised as well as `'<inline>'`.
2. **Order the guards so the CAUSE surfaces, not the symptom.** An empty
   `file_path` and a spelling mismatch both end in zero joined rows. The empty
   case must be checked FIRST and get its own message, or a duckdb user is told
   to check their path spelling when there is no path to spell.
3. **Say what to do instead.** A duckdb parse has no retrievable source through
   this macro; the actionable instruction is to parse the same file with a
   tree-sitter language, which is what `ast_unparse_exact`'s message already
   says for its own surface.

**The deeper fix is in the adapter, and is not this bug.** If the duckdb
adapter populated `file_path` under `read_ast` the way every tree-sitter
language does, this macro would work. That is an adapter change in the same
family as 043 and should be weighed with it; whoever takes 043 should read this
file. But the macro must fail loudly even so, because `parse_ast` has no path to
populate and will always reach this branch.

## Tests

`test/sql/core_alone.test` section 7 already holds the red-in-waiting. It pins

    1   20   20   0

labelled with this id, and the comment above it states what the assertion must
become when the fix lands: a `statement error` naming the unlocatable file, or
`0` in the `blank_paths` column and a row count in the last if the adapter is
fixed first. That file's header rule about pins that move does not apply to that
block, and the comment says so.

New tests the fix needs:

1. The four-column row above, with its post-fix values or as a
   `statement error`.
2. The same input through `ast_source_of_from`, because the shared body means
   the change must be measured on both twins — exactly as 058's "Tests" section
   requires for its own fix.
3. A node table whose `file_path` is `'<inline>'` (from `parse_ast` with a
   tree-sitter language), which must get the inline-specific message and not the
   empty-string one.
4. A file genuinely absent from `files`, which must stay distinguishable from
   both of the above.
5. `test/sql/file_utilities.test` (44 assertions) re-measured, because it is the
   twin's regression net and the shared body moves.

## Cost

A macro-body change in a core `.sql` file, so: regenerate
`src/include/embedded_sql_macros.hpp`, a full extension rebuild, and
`test/sql/file_utilities.test` plus `test/sql/macro_from_parity_source_of.test`
(50 assertions) and `test/sql/core_alone.test` (190 assertions). The boundary
gate must stay at 105 macros / 69 edges / 0 violations; the edge count moves only
if the guard calls another macro. Best landed together with 058, since both
change the same join and each one's before/after needs the other's cases.
