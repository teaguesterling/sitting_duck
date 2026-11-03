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

---

# MEASURED 2026-11-02 — what duckdb can and cannot tell us about locations

Teague settled the design (NULL for a node with no location, first NULL in
`start_line`) and asked for telemetry alongside it. He then asked a specific
design question: **could an expression's end come from the end of the parent, or
from the start of the next sibling at the same level?**

Both answers are measured, not reasoned about. Scripts in `workspace/043/`
(gitignored): `measure_query_location.py`, `measure_token_ends.py`,
`measure_spans_v2.py`. Corpus for all of them: `duckdb/benchmark/**/*.sql` — the
submodule's own clickbench / JOB / tpch / tpcds queries, **232 files that parse**
(44 skipped: `json_serialize_sql` serializes SELECT only, so DDL and DML are NOT
covered by these figures).

**Every figure below is labelled by duckdb version. They are not
interchangeable.**

## On the pinned version, 1.5.6

| measured | value |
|---|---|
| expression nodes | 12,431 |
| with `query_location` | 12,388 (**99.7%**) |
| without | 43 (0.3%) |
| any end or length on an expression | **none exists** — empty grep across all of `duckdb/src/include/duckdb/parser/` |
| leaves (no expression children) | 8,173 (**65.7%**) |

So **starts are nearly complete and ends do not exist**. That is the real shape
of this bug: not "locations are missing" but "half of every location is
missing".

### Are offsets absolute, or relative to their statement? ABSOLUTE.

This was an open question and is now settled, on the 1.5.6 CLI:

```
SELECT json_serialize_sql('SELECT 1; SELECT abc FROM t');
  -> "column_names":["abc"] with "query_location":17
```

`abc` sits at offset 17 of the whole string; per-statement it would be 7. So the
offset -> line/column conversion runs against the full parsed text, once.

### Answer: NO, an end cannot be derived from the next sibling

Checked on the 996 single-identifier `COLUMN_REF`s that have a next sibling and
whose true extent could be **verified against the source text**:

| derived end | result |
|---|---|
| exactly right | **0 of 996 (0.0%)** |
| over-reach, characters | min 1, median 3, mean 10.6, **max 776** |

It is never right, because the gap always contains at least the separator, and
at worst an operator, newlines and comments. Also **36.2%** of located
expressions are the last located sibling, so they would have no next sibling and
stay NULL regardless.

### Answer: NO, and there is no parent end to take either

In 1.5.6 no expression has an end, so "end of the parent" recurses up to
`SQLStatement::stmt_location + stmt_length` — a **statement-level** bound, never
an expression end.

Worse, the parent is not even positionally above its children: **3,451 of 4,212
located parents (81.9%) have their own `query_location` AFTER their first
child's start.** The grammar is explicit about why — `select.y:2293` builds
`a + b` with `makeSimpleAExpr(PG_AEXPR_OP, "+", $1, $3, @2)`, and `@2` is the
**operator token**. Measured on `SELECT a + b * c`: `+` at 9, its children at 7
and 13.

### THE OPEN PROBLEM FOR M2, not covered by the DECISIONS block

That 81.9% is about **starts**, and it is a *wrong* value, not a missing one.
The decision recorded above covers nodes with **no** location. It does not cover
a composite node whose location is the operator's position rather than its own
start: on 1.5.6, `a\n+ b` would report `start_line = 2` for the whole
expression.

This is precisely the trap this bug was opened about — a populated value that
looks right. **It must be settled before M2.** Options: report the anchor as-is,
take the minimum start over the subtree, or report NULL. It does **not** block
M1 (the `location_known` plumbing).

### The one derivation that DOES work: token ends, for leaves only

`Parser::Tokenize` exists on the pinned version (`parser.hpp:47`) and returns
`vector<SimplifiedToken>`, where `SimplifiedToken` is `{type, start}` — a start
and no end. But tokens are contiguous, so a token's end is the next token's
start with trailing whitespace removed:

| measured | value |
|---|---|
| verifiable leaves with a token at their location | 1,323 |
| token-derived end exactly right | **1,322 (99.9%)** |
| wrong | 1 (by +16) |
| no token at the location | 0 |

Leaves are 65.7% of nodes, so this is a real option for a real end on most
nodes, at the cost of one tokenize pass per parsed string. **Composite ends stay
NULL** under it — bracket matching would be writing a second parser.

## On duckdb 2.0.0, which changes the answer

2.0 replaces `optional_idx query_location` with a `QueryLocation` struct
(`duckdb/common/query_location.hpp`):

```cpp
struct QueryLocation {
    uint32_t offset = INVALID_OFFSET;   // start byte
    uint32_t length = 0;                // "0 for a point / unknown extent"
    bool IsValid() const;  uint32_t Start() const;  uint32_t End() const;
    //! Returns the smallest QueryLocation enclosing both this and other.
    QueryLocation Merge(const QueryLocation &other) const;
};
```

| measured (2.0.0) | value |
|---|---|
| expression nodes | 12,474 |
| with a `query_location_length` field | 12,474 (100%) |
| length > 0, i.e. a usable end | 12,263 (**98.3%**) |
| length == 0, no end | 211 (1.7%) — of which FUNCTION 202 of 1,229 (16.4%) |
| exact on verifiable leaves | **1,323 of 1,323 (100%)** |

Note the scope of that 100%: it is **leaf exactness**, on single-identifier
`COLUMN_REF`s. Composite spans were checked for *containment*, not exactness.

Composite starts improve but do not become sound. Of 9,980 parent/child pairs
where both carry a span, **445 (4.5%) are not contained**:

| parent class | violations |
|---|---|
| FUNCTION | 206 (46.3%) |
| OPERATOR | 188 (42.2%) — `COMPARE_IN` 129, `IS_NOT_NULL` 27, `IS_NULL` 24 |
| BETWEEN | 38 (8.5%) |
| SUBQUERY | 8 |
| CAST | 5 |

**In 445 of 445 the parent's END still covers the child.** Every violation is a
*start* problem. Example: `o.order_serverId in (1, 3, 5)` gives the OPERATOR the
span `(1, 3, 5)` — correct end, wrong start. `Merge` is exactly the fix upstream
did not apply at these sites.

So the 1.7% length-0 cases still need the NULL path, which is why **M1 is
invariant** across the pin decision.

## Why this did not become M2

`040` was re-measured against the v2.0.0 release tag the same day and is
**confirmed ship-blocking** (13.05s exec, 72.23s plan, for an 11-node table). So
the better location data in 2.0 is not reachable yet. See
`tracker/bugs/040`, RE-MEASUREMENT 2026-11-02.
