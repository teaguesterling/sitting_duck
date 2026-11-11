# 058 — `ast_source_of_from` returns a silent 0 when `files` and `ast_table` disagree on a path spelling

**Status:** Not fixed. Measured, pinned as a KNOWN DEFECT, and deliberately not
fixed inside M2 stage 3 task 6 — see "Why it is not fixed here". Ships to the
public `duckdb/community-extensions` registry with the stage, on a macro the
path-taking twin cannot reach.
**Found by:** my own concern note during M2 stage 3 task 6, confirmed and given
this owner by the task-6 code review (2026-11-10). Every figure below was
measured against `build/release/duckdb` at `e7b910e`, not taken from the review
— with ONE exception, added later and attributed where it appears: the twin's
four-spelling figures, used in "Why it is not fixed here" and again in "The
intended shape", are the whole-branch reviewer's measurement, not mine.
**Related:** #89 (silent-empty) is **closed** — `tracker/features/046-cpp-selector-engine.md:4`
records it as "silent-empty, closed by the guard approach" — so #89 is cited
here and in the code only as the PRECEDENT `ast_unparse_exact_from` follows,
never as this fix's home. This file is the home.

## What is wrong

`ast_source_of_from(ast_table, files, target_name, kind := NULL)` is the one
`_from` variant of the stage that takes both a table and a path. It needs both:
the result carries the source TEXT of each match, which comes from
`read_text(files)`, and a node table holds no source text.

The two arguments can therefore name the same file in two spellings, and then
nothing matches. The mechanism is one line:

    src/sql_macros/file_utilities.sql:164
        JOIN read_text(files) fc ON fc.filename = m.file_path

An **inner** join on **exact string equality**. A match whose file is absent
from `files`, or spelled differently there, is dropped. No error, no warning,
no diagnostic column — the caller gets fewer rows, or none.

## The measurement

`read_ast` and `read_text` do not normalise path spellings the same way for
globs and for exact paths. Measured, four calls:

| argument | spelling returned by both `read_ast` and `read_text` |
|---|---|
| `'./test/data/python/q*.py'` (a GLOB) | `test/data/python/…` — the `./` is **stripped** |
| `'test/data/python/q*.py'` (a GLOB) | `test/data/python/…` |
| `'./test/data/python/qualified_names.py'` (EXACT) | `./test/data/python/…` — **verbatim** |
| `'test/data/python/qualified_names.py'` (EXACT) | `test/data/python/…` |

So two spellings of the same GLOB are interchangeable, and the trap is pairing a
`./`-prefixed EXACT path with an unprefixed glob. With
`tq = read_ast('test/data/python/q*.py')` (2 files, 4 matches for `'__init__'`)
and `tqd = read_ast('./test/data/python/qualified_names.py')`:

| call | rows |
|---|---|
| `ast_source_of_from('tq', './…/q*.py', '__init__')` | 4 — glob spellings agree |
| `ast_source_of_from('tq', '…/qualified_names.py', '__init__')` | 3 — narrowed, and correct |
| `ast_source_of_from('tq', './…/qualified_names.py', '__init__')` | **0 — silent** |
| `ast_source_of_from('tqd', './…/qualified_names.py', '__init__')` | 3 — spellings agree |
| `ast_source_of_from('tqd', '…/q*.py', '__init__')` | **0 — silent** |

Pinned as one row, `4 3 0 3 0`, in
`test/sql/macro_from_parity_source_of.test` (section 5, "THE MIXED-SPELLING
TRAP"). The 4 and the 3 are positive controls in the same row, so the two zeros
cannot be produced by a wholly broken query.

**Note for whoever fixes this:** `ast_unparse.sql:562-563` states the opposite
about globs — "DuckDB's globber returns './'-prefixed relative paths while an
exact path passes through verbatim". The table above was **not** able to
reproduce the glob half of that on this build, and the pinned 0 in column 3
decides it: if the globber added `./`, both sides of that call would carry it
and the cell would read 3. The Windows native-separator case at
`ast_unparse.sql:564-569` is a third spelling and is **not measured here**.
`ast_unparse.sql` is not edited by this entry.

## The contract it violates

- `tracker/features/044-v2-m1-contract.md:16` — the conformance kit requires
  "raw-AST-join agreement, and no silent-empty (#89)".
- `tracker/roadmap/m2-stage3-macro-split-plan.md:60` — this stage's own review
  focus states the principle: "The #89 principle makes 0 rows mean 'searched
  correctly, not there'."

A 0 from a path-spelling mismatch does not mean "searched correctly, not
there". It means "I could not match the file to its text", which is exactly the
case the contract forbids. The same plan line already made task 4 give a
missing TABLE name a named error rather than 0 rows; this is the same class of
input, reached through the second argument instead of the first.

## Why it is not fixed here

Not YAGNI, and not scope discipline alone. The reason is structural — but it
constrains **one half** of the fix, not both, and `R39` was half wrong to apply
it to both. `R69` corrects it, and this section is the correction.

**The join now lives in the shared body.** Task 6 rewrote `ast_source_of` as a
thin wrapper that delegates to `ast_source_of_from`, so
`file_utilities.sql:164` is the only copy of that join and the twin reaches it
too. Anything added there is added to the twin as well.

**Which half that actually constrains, MEASURED.** The whole-branch review
(2026-11-10) measured the twin over the same four path spellings as the table
above: `read_ast`'s `file_path` and `read_text`'s `filename` agree in all four,
the twin returns non-zero in all four (4 / 4 / 3 / 3), and a LEFT JOIN in place
of the inner join leaves **0** unmatched rows.

- **Half 1, normalising the join key: a NO-OP for the twin** over those four
  spellings. The twin passes `file_patterns` to both sides of the join, so both
  sides carry the same spelling, and normalising both cannot change which rows
  match. This half could have landed inside this stage.
- **Half 2, raising on an unreadable file: a REAL twin behaviour change.** It
  turns a call that returns rows today into an error. That is what M2 stage 3's
  no-behaviour-change constraint forbids, and it is the half the deferral is
  actually about.

The deferral still stands as a decision; it now stands on the right reason.
**Whether to land half 1 on its own is Teague's call, not this file's.** The two
halves were specced together, and the measured no-op covers four spellings, not
the Windows separators of `ast_unparse.sql:564-569`, which were never measured
here.

(The weaker reason I gave first — "it is its own change with its own test" —
is true but would not have justified deferral on its own.)

## The intended shape: loud failure

Follow `ast_unparse_exact_from`, which solved this class for its own surface
and is the precedent #89 left behind. Measured in that implementation:

- it normalises for MATCHING only — leading `./` stripped, backslashes folded
  to forward slashes — at `src/sql_macros/ast_unparse.sql:593`, while returning
  the node table's own spelling untouched so the result still joins back;
- it RAISES on a file it cannot read, at `src/sql_macros/ast_unparse.sql:737`,
  rather than returning nothing.

So the two halves of the fix are:

1. **Normalise the join key** on all three sides (the node table's
   `file_path`, `read_text`'s `filename`, and any future `file_path :=`
   argument): strip a leading `./`, fold backslashes to forward slashes. Match
   on the normalised value; return the spelling the node table carried.
2. **Raise when a match's file is not readable through `files`.** A match that
   survives the filter but finds no text is not an empty search — it is a
   failure to locate the source, and it must say so and name the file.

Both must be measured on `ast_source_of` as well as on `ast_source_of_from`,
because of the shared body. This paragraph first said that step 1 alone can
change the twin's output "for a caller who passes a `./`-prefixed exact path
today"; measured, it cannot. The review ran the twin over all four spellings in
the table above, the `./`-prefixed exact path among them, and it returned
non-zero in every one — because the twin passes one argument to both sides of
the join, so normalising both sides cannot change which rows match. That is four spellings and not a proof, so the before/after
still needs the twin's own digests rather than only the `_from` form's, and the
Windows separators of `ast_unparse.sql:564-569` are unmeasured for both halves.

## Tests

`test/sql/macro_from_parity_source_of.test` already holds the red-in-waiting.
The `4 3 0 3 0` row is labelled a KNOWN DEFECT carrying this bug's id, and the
comment above it states what the assertion must become once the fix lands:
columns 3 and 5 go to 3 and 4 if the normalising join lands alone, or the whole
statement becomes a `statement error` if the readability guard raises first.
That file's header rule ("a pin that moves is a parity failure to investigate,
never a value to update") does **not** apply to those two columns, and the
comment says so — otherwise it would tell the fixer their correct change is a
regression.

New tests the fix needs:

1. The five-count row above, with the measured post-fix values.
2. The same five calls through the TWIN where it can express them, so the
   shared body's change is measured on both macros.
3. A file present in `ast_table` but genuinely absent from `files` (not a
   spelling difference) must raise and name the file, not return 0 rows.
4. Windows separators, if a Windows leg is available; otherwise record it as
   not covered rather than assume the fold handles it.

## Cost

A macro-body change in a core `.sql` file, so: regenerate
`src/include/embedded_sql_macros.hpp`, a full extension rebuild, and the parity
test plus `test/sql/file_utilities.test` (44 assertions, the twin's own
regression net, which must be re-measured because the twin's behaviour can
move). The boundary gate must stay at 0 violations; the edge count moves only
if the fix calls another macro.
