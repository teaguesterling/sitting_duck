# 043 — duckdb nodes report no source location: every node claims line 1

**Status:** open, **test written and failing, implementation deliberately not
started.** Scoped to v1.18.0 under `042`. Found by the 25-language fan-out
(2026-10-31), confirmed and sized by the scheduled continuation (2026-11-01).

## The defect

`DuckDBAdapter::CreateASTNode` hardcodes all four location fields
(`src/language_adapters/duckdb_adapter.cpp:1051-1055`):

```cpp
// Source location (placeholder values for now)
node.source_start_line = 1;
node.source_end_line = 1;
node.source_start_column = 1;
node.source_end_column = 1;
```

So **every node of every duckdb parse reports line 1, column 1**. `read_ast` and
`parse_ast` cannot say where anything is, in the one language the extension is
embedded in.

Measured, with `sql` on byte-identical input as the oracle:

```
duckdb   4 CREATE statements, one per line  ->  every node start_line = 1
sql      3 statements, one per line         ->  1, 2, 3
```

## Why no test caught it

`test/sql/duckdb_parser_test.test:22` **does** assert `start_line` — on
`SELECT id, name FROM users`. That is a **one-line** source, where `1` is the
correct answer for every node. The only duckdb test that reads the column cannot
distinguish a real line number from the placeholder.

This is not a test pinning a wrong value. It is a test whose input cannot
exercise the thing it asserts, which is the same class of gap as a corpus with
no lambdas in it.

## The pin test

Written, run, and **failing for the stated reason** on `cee95ef`: expected
`1,2,3,4`, got `1,1,1,1` — `assertions: 4 | 3 passed | 1 failed`. The runnable
file is at `workspace/042/duckdb_node_locations.test` (gitignored); it is
reproduced here so the expectation is durable, and it should land in
`test/sql/bugs/` **with** the fix, not before it.

```
# description: duckdb nodes report real source lines, not the CreateASTNode placeholder
require sitting_duck

query II
SELECT type, start_line
FROM parse_ast('CREATE TABLE s.t2(a INT);
CREATE MACRO m(x) AS x+1;
CREATE SCHEMA sc;
CREATE VIEW v AS SELECT 1;', 'duckdb')
WHERE depth = 1
ORDER BY start_line;
----
create_table	1
create_macro	2
create_schema	3
create_view	4

# A one-row result means every node claims the same line.
query I
SELECT count(DISTINCT start_line)
FROM parse_ast('SELECT 1;
SELECT 2;
SELECT 3;', 'duckdb');
----
3
```

## Measured size — this is not a small fix

| fact | measured |
|---|---|
| `CreateASTNode` call sites in the file | **37** |
| location data threaded today | **none** — no `stmt_location`, `stmt_length` or `query_location` appears anywhere in `duckdb_adapter.cpp` |
| statement-level location available? | yes — `SQLStatement::stmt_location` and `stmt_length` (`duckdb/src/include/duckdb/parser/sql_statement.hpp:32,34`) |
| expression-level location available? | **optionally** — `BaseExpression::query_location` is an `optional_idx` (`base_expression.hpp:92`) |
| do the converters hold the parser objects? | yes — `ConvertStatement(const SQLStatement &)`, `ConvertExpression(const ParsedExpression &)`, and the per-node `Handle*` methods all receive them |

So the data is reachable at each converter and simply never reaches
`CreateASTNode`. The fix needs three parts: thread a byte offset through to
`CreateASTNode`, convert offset → line/column against the original query text
(which the adapter must therefore also hold), and decide what to do when a
location is absent.

## The open design decision — why implementation stopped here

`query_location` is an `optional_idx`. Some expressions will not have one.
Whatever a node without a location reports is a decision, not a detail:

1. **Inherit the parent's line.** Plausible output, sometimes wrong, and
   indistinguishable from a correct answer.
2. **Report the statement's line.** Coarser, honest at statement granularity,
   still wrong for a multi-line statement.
3. **Report NULL / 0 as "unknown".** Visibly missing. Costs a schema question —
   `source_start_line` is not nullable today, and every consumer of the column
   would need to tolerate it.

Option 1 is the trap this whole investigation is about: a populated value that
looks right. `042` records the same shape in swift, where a missing container
produced one plausible blob instead of two arguments, and it passed every
non-emptiness check in the repository. **A partial fix here — statement nodes
correct, expression nodes still 1 — would be worse than the uniform placeholder,
because the uniform placeholder is at least obviously wrong.**

That is a call for Teague, so the scheduled run stopped at the failing test.

## What was done

- The failing test, confirmed failing for the stated reason and no other.
- This record, on branch `fix/042-duckdb-node-locations` off `main`, not pushed.
- **No implementation. No change to `duckdb_adapter.cpp`.**
