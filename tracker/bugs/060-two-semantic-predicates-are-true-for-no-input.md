# 060 — `is_boolean_literal` and `is_type_definition` are true for no input at all, and five shipped branches depend on them

**Status:** Not fixed. Measured, pinned as a KNOWN DEFECT, and deliberately not
fixed inside M2 stage 3 task 7 — see "Why it is not fixed here". Ships to the
public `duckdb/community-extensions` registry with the stage, on two macros the
feature spec lists as **core**.
**Found by:** M2 stage 3 task 7, by sweeping the whole 256-value `SEMANTIC_TYPE`
domain rather than only the test fixture. A fixture-only sweep would have
reported these two as "python has no such node", which is the wrong diagnosis
and the reason the domain sweep exists. Every figure below was measured against
`build/release/duckdb` at `ca0b7fd`.
**Related:** `tracker/bugs/062` is the other `SEMANTIC_TYPE` naming defect found
in the same sweep, and the two are NOT the same: 062 is about names that map to
four codes each, this one is about names that map to none.

## What is wrong

Two of the 30 semantic predicates in `src/sql_macros/semantic_predicates.sql`
test for semantic type names that do not exist:

    semantic_predicates.sql:26   is_type_definition(st)  -> is_semantic_type(st, 'DEFINITION_TYPE')
    semantic_predicates.sql:54   is_boolean_literal(st)  -> is_semantic_type(st, 'LITERAL_BOOLEAN')

Neither `'DEFINITION_TYPE'` nor `'LITERAL_BOOLEAN'` is a `SEMANTIC_TYPE` name.
Both macros therefore return false for **every possible input**. They are dead
as written, and they are not marked as such anywhere:
`tracker/features/057-v2-m2-stage3-macro-split.md:375` and `:396` list both as
**core**, with the note "RFC:106 names it".

## The measurement

Swept over the whole domain, not over a fixture:

```sql
WITH dom AS (SELECT i::UTINYINT::SEMANTIC_TYPE AS st FROM range(0,256) t(i))
SELECT count(*) FILTER (is_boolean_literal(st)),
       count(*) FILTER (is_type_definition(st))
FROM dom;
-- 0   0
```

Every one of the other 28 predicates returns 4 (or 16 for `is_literal`, which
matches a prefix spanning four names). These two return 0.

The cause, measured the same way:

```sql
SELECT unnest(['DEFINITION_TYPE','LITERAL_BOOLEAN','LITERAL'])::SEMANTIC_TYPE;
-- NULL
-- NULL
-- NULL
```

(`'LITERAL'` is the third and is NOT a defect: `is_literal` matches on the
`LITERAL` *prefix*, so it works and measures 16. It is listed here only so the
next person does not "fix" it.)

All 256 codes are non-NULL and map onto exactly **64** distinct printed names,
so the enum is dense and these two names simply are not in it.

## The blast radius is wider than two predicates

`is_type_definition` has **five consumer branches in shipped macro bodies**, and
every one of them is unreachable:

| site | what is dead |
|---|---|
| `src/sql_macros/tree_navigation.sql:86` | `ast_definitions_from`'s `definition_type = 'type'` |
| `src/sql_macros/file_utilities.sql:140` | `ast_source_of_from`'s `definition_kind = 'type'` |
| `src/sql_macros/file_utilities.sql:155` | **`ast_source_of(..., kind := 'type')`** — a documented argument value that can never match a row |
| `src/sql_macros/tree_analysis.sql:33` | the add-on definition classifier's `'type'` |
| `src/sql_macros/duck_blocks.sql:130` | `duck_blocks`' `'type'` |

So the user-visible consequences are not only "a predicate returns false". They
are that **`'type'` can never appear in a `definition_type` or
`definition_kind` column**, and that `ast_source_of(..., kind := 'type')` is a
supported-looking filter that silently returns nothing — the silent-empty class
`#89` closed and `tracker/features/044-v2-m1-contract.md:16` forbids.

`is_boolean_literal` has no consumer branch in `src/sql_macros/`; its exposure
is as a public macro that always answers false.

## The contract it violates

- `tracker/features/044-v2-m1-contract.md:16` — "no silent-empty (#89)".
  `kind := 'type'` is a silent empty reached through an argument value.
- The predicates are listed as **core** and language-agnostic in
  `tracker/features/057-v2-m2-stage3-macro-split.md:375` and `:396`. A core
  macro that cannot be true is not language-agnostic; it is inert.

## Why it is not fixed here

M2 stage 3 is constrained to **no behaviour change**, and every candidate fix is
one: a predicate that starts returning true changes the output of
`ast_definitions`, `ast_source_of`, `duck_blocks` and the tree-analysis
classifier at once, through the five branches above. The task that found it was
a test task and was told to pin it and not fix it.

The fix also needs a decision this bug cannot make on its own — see below.

## The intended shape

The fix is NOT simply "use the right name", because it is not established that
there is one. Two possibilities, and the first step is to find out which:

1. **The names are wrong and real equivalents exist.** Then correct the two
   strings and the five branches start working. Candidates to check against the
   enum's 64 names: a boolean literal may live under `LITERAL_ATOMIC` (measured:
   `true` in a duckdb parse carries `LITERAL_ATOMIC`), and a type definition may
   live under a `TYPE_*` or a `DEFINITION_*` name. **Check the enum, do not
   guess**: `SELECT DISTINCT i::UTINYINT::SEMANTIC_TYPE::VARCHAR FROM
   range(0,256) t(i)` lists all 64.
2. **The concepts have no semantic type.** Then the honest fix is to delete both
   predicates and the five `'type'` branches, and to say so in RFC:106, which is
   what put the names there. Deleting a public macro is its own decision with
   its own deprecation question.

Either way the five consumer branches must be settled in the same change as the
predicate, because they are what makes this user-visible.

**Do not fix this by making the predicates raise.** They are called per row in
filter chains; an error would be worse than a false.

## Tests

`test/sql/core_alone.test` section 2 already holds the red-in-waiting. The
30-row predicate matrix pins

    is_boolean_literal      0   0
    is_type_definition      0   0

where the first column is the domain sweep and the second the fixture count.
Both rows are labelled with this id and are MEANT to move when the fix lands:
the domain column goes to 4 (or the row disappears, if the predicate is
deleted). That file's header rule about pins that move does not apply to those
two rows, and the comment there says so.

New tests the fix needs:

1. The two matrix rows, with their post-fix domain counts.
2. A row that the corrected predicate actually matches, from a real parse, so
   the fix is proven to do more than change a constant.
3. `ast_source_of(..., kind := 'type')` returning a row on a fixture that has
   one — the user-visible half, which no current test covers.
4. `definition_type = 'type'` appearing in `ast_definitions` output for the same
   fixture.
5. If the resolution is deletion instead: `test/sql/macro_split_oracle.test`'s
   counts move from 45 / 61 / 106, and the boundary gate's 105 with them.

## Cost

If the names are corrected: a macro-body change in two core `.sql` files, so
regenerate `src/include/embedded_sql_macros.hpp`, rebuild, and re-measure every
suite that touches definitions — `ast_definitions`, `ast_source_of`,
`duck_blocks` and the tree-analysis tests — because the five branches change
output. If the resolution is deletion: the same plus the oracle and the boundary
gate counts, and a deprecation note.
