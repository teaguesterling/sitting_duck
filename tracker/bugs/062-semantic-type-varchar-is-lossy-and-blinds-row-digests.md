# 062 — `SEMANTIC_TYPE::VARCHAR` is lossy for 192 of 256 codes, so an `md5(r::VARCHAR)` row digest cannot see a retype inside a block of four

**Status:** Not fixed. Measured, and recorded as a KNOWN LIMITATION of a test
convention rather than a user-facing defect — see "Who this hurts". Not fixed
inside M2 stage 3 task 7: the stage is constrained to no behaviour change, and
every candidate fix is either a change to how a type prints or a change to a
convention four test files rest on.
**Found by:** M2 stage 3 task 7, while building a committed CSV fixture of a
parsed AST. Writing semantic types as their NAMES silently re-typed 25 values,
which is how the asymmetry surfaced. Confirmed and measured in both directions
by the task-7 code review (2026-11-10). Every figure below was measured against
`build/release/duckdb` at `ca0b7fd`.
**Related:** `tracker/bugs/060` is the other `SEMANTIC_TYPE` naming defect from
the same sweep and is a DIFFERENT one — 060 is names that map to no code at all,
this is names that map to four codes each.

## What is wrong

`SEMANTIC_TYPE` is a 1-byte logical type (`duckdb_types()` reports
`logical_type = UTINYINT`). Casting a value to `VARCHAR` prints a name, and
each name covers a **block of four consecutive codes** — the low two bits are a
refinement the name does not show. Parsing the name back returns the **lowest**
code of its block.

    244, 245, 246, 247   all print   DEFINITION_VARIABLE
    'DEFINITION_VARIABLE'  parses back to  244

So `x::VARCHAR::SEMANTIC_TYPE = x` is false for three of every four values.

## The measurement

Over the whole domain:

```sql
WITH dom AS (SELECT i::UTINYINT::SEMANTIC_TYPE AS st FROM range(0,256) t(i))
SELECT count(*), count(*) FILTER (st::VARCHAR::SEMANTIC_TYPE IS DISTINCT FROM st) FROM dom;
-- 256   192
```

```sql
SELECT count(DISTINCT i::UTINYINT::SEMANTIC_TYPE::VARCHAR),
       count(*) FILTER (i::UTINYINT::SEMANTIC_TYPE IS NULL)
FROM range(0,256) t(i);
-- 64    0
```

256 codes, 0 unmapped, 64 distinct names: exactly four codes per name, and 192
of them lossy. Consistent with every semantic predicate measuring `dom_hits = 4`
in `test/sql/core_alone.test` section 2.

A worked example of each direction, from four live blocks:

| code | prints | parses back to |
|---|---|---|
| 76, 77, 78 | `LITERAL_STRUCTURED` | 76 |
| 148, 149 | `FLOW_LOOP` | 148 |
| 152, 154 | `FLOW_JUMP` | 152 |
| 244, 246 | `DEFINITION_VARIABLE` | 244 |

## Who this hurts: the `md5(r::VARCHAR)` digest convention

This branch's parity tests pin a whole row by casting the row alias to `VARCHAR`
and hashing it:

    md5(string_agg(r::VARCHAR, ',' ORDER BY <key>))

That convention was adopted for a good measured reason — task 4 showed that a
count plus a key digest cannot see a dropped or renamed COLUMN, while the cast
prints every column name and value and so does. It already documents one blind
spot: a widened or narrowed numeric type is invisible, because
`{'x': 5::INTEGER}::VARCHAR = {'x': 5::BIGINT}::VARCHAR`.

**This is a second blind spot, and it is specific to `semantic_type`.** A row
whose `semantic_type` changes WITHIN a block of four renders identically, so the
digest does not move.

Measured both ways by the review, on the task-7 fixture:

| change | `semantic_type::VARCHAR` after | whole-row digest |
|---|---|---|
| node 0, code **252 -> 253** (within a block) | `DEFINITION_MODULE`, unchanged | **unchanged** |
| node 2, code **68 -> 72** (across a name boundary) | `LITERAL_STRING` -> `LITERAL_ATOMIC` | **moves** |

So the convention is not broken — it catches any retype that changes the printed
name, which is the shape a renumbering or a mis-classification usually takes. It
is blind only to a permutation inside a block of four.

**Blast radius, measured:** `grep -rln 'r::VARCHAR' test/` returns **4** files.

    test/sql/core_alone.test
    test/sql/macro_from_parity_relational.test
    test/sql/macro_from_parity_navigation.test
    test/sql/macro_from_parity_source_of.test

All four now carry a one-line note citing this id. Before this bug was filed the
note existed only in `core_alone.test`, which is a file a parity-test author has
no reason to open.

## What does NOT fix it

Recorded because it is the obvious move and it is wrong:

- **Hashing `semantic_type::UTINYINT` instead.** That catches a code
  permutation, but it does NOT catch an enum **renumbering** — a renumbering
  leaves a committed fixture's codes untouched and changes only what they mean.
  Swapping one blind spot for another is not progress.
- **Adding a second digest beside the first.** Two digests over the same rows
  cost two assertions and still cannot distinguish the two cases above.

**What does catch a renumbering** is a fixture-versus-live comparison on a full
build: regenerate from `read_ast()` and `EXCEPT ALL` both ways. That is already
prescribed in `test/data/fixtures/core_alone_ast.csv`'s "REGENERATING IT"
section, and the task-7 review ran it (0 rows in both directions). It is a
generation-time and review-time check, not something a `.test` file can do in a
parser-free build.

## The intended shape

There may be nothing to fix in the extension. The options, in order of
preference:

1. **Leave the type alone and keep the limitation documented.** The four notes
   added by this bug are the cheapest correct outcome, and the only one with no
   behaviour risk. This is the recommendation.
2. **Give the refinement bits a rendering.** If the low two bits are meaningful
   to a user — and they are at least meaningful enough for 192 codes to exist —
   then `SEMANTIC_TYPE::VARCHAR` arguably should show them, e.g.
   `DEFINITION_VARIABLE` versus `DEFINITION_VARIABLE+2`. That is a
   user-visible output change affecting every test that prints a semantic type,
   so it needs its own before/after and is far larger than this bug.
3. **Expose the refinement as its own accessor** (`semantic_refinement(st)` or
   similar), leaving the name as it is, and let a test that cares pin that
   column too. Additive, but it adds a macro to a stage that is counting them.

Whoever decides this should first establish what the low two bits MEAN. That is
not recorded in this file because it was not measured: `RFC:106` names the
semantic type taxonomy and is the place to look.

## Tests

No red-in-waiting, because there is no agreed correct behaviour to assert yet.
What exists is documentation, in four places:

- `test/sql/core_alone.test` section 0, the long form, beside the explanation of
  why the fixture carries codes rather than names.
- One line each in the three `macro_from_parity_*.test` files, naming this id.

If option 2 or 3 is ever taken, the tests it needs are: the 256-code sweep above
with its post-fix figures, and a within-block retype that a digest can now see —
which is the planted mutant this bug's measurement already describes.

## Cost

Option 1: zero, and already done. Option 2: a type-printing change plus
re-measuring every test that renders a `semantic_type`, which is most of the
suite. Option 3: one new scalar macro, the oracle counts in
`test/sql/macro_split_oracle.test` moving from 45 / 61 / 106, and the boundary
gate's 105 with them.
