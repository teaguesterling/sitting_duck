# 041 — Cross-line drift in the `duckdb` language: deparser rendering, parse-tree drift, and bind-time rejection

**Status:** open — **accepted cross-line differences, no fix needed upstream or here.** What is
open is a POLICY decision about test expectations (below). Not ship-blocking.
**Found:** 2026-10-06, while fixing the family-F `Parser` constructor break (PR #192).
Axes 1–3 below were each **already recorded in-tree on 2026-09-06** by `b8c06a8` (PR #113);
this entry consolidates them and states the undecided part.
**Scope:** the `duckdb` language only (the native DuckDB-SQL adapter). Tree-sitter languages
are unaffected — they do not go through DuckDB's parser or deparser.

## Correction to an earlier framing

An earlier draft of this entry claimed **"only the `peek` column differs."** That is **false**
and is retired. There are three independent drift axes, and only the first is cosmetic.

| Axis | What moves | Visible to | Is it a bug? |
|---|---|---|---|
| 1. Deparser / `peek` | `SQLStatement::ToString()` rendering | `parse_ast` / `read_ast` | No — upstream cosmetics |
| 2. Parse-tree drift | the upstream expression *class*, so `type` + `name` + `semantic_type` change | `parse_ast` / `read_ast` | Behavioural change; queries and tests can break |
| 3. Bind-time rejection | SQL that parses but no longer binds | **nothing in this extension** | Yes, for anyone executing that SQL |

## Axis 2 — parse-tree drift is real

```sql
parse_ast('SELECT u.name FROM users u WHERE u.active = true', 'duckdb')  -- node_id 10
```

| | `type` | `name` | `semantic_type` |
|---|---|---|---|
| v1.5.6 | `cast_expression` | *(empty)* | `COMPUTATION_EXPRESSION` |
| DuckDB v2.0 | `literal` | `true` | `LITERAL_ATOMIC` |

v2.0 stops wrapping a boolean literal in an implicit cast. Three columns change, with **no
build break**.

**A corpus that finds no drift is not evidence of no drift.** The 43-statement corpus
(`test/corpus/duckdb_sql/kitchen_sink.sql`) and the 120 KB real-world file showed structure
unchanged — but neither contained a boolean literal as a comparison operand, so neither could
have caught this. Any "no drift observed" claim needs to say what the corpus *would* have
caught.

### Node types can drift without a build break

An earlier draft argued they could not, reasoning that sitting_duck's node type strings are its
own hardcoded literals (`"select_statement"`, `"create_macro"`, …) selected by `switch` on
DuckDB enums (`StatementType`, `CatalogType`). **That reasoning is fallacious.** Owning the
literals means an upstream enumerator *rename* breaks the build — genuine and worth keeping.
It says nothing about **which** literal is selected: that follows the upstream expression class,
which is precisely what moved above. Different `switch` arm, different hardcoded string, silent.

## Axis 3 — bind-time rejection, invisible to this extension

The deprecated lambda arrow `x -> ...` is a **warning** on v1.5.6 and a **hard
`Binder Error: Deprecated lambda arrow (->) detected`** on DuckDB v2.0. `lambda x:` works on
both lines.

`parse_ast` / `read_ast` parse but **never bind**. So this extension cannot see axis 3 at all,
and **a parse-only corpus can contain SQL that cannot execute on v2.0.** `b8c06a8` migrated the
affected test SQL to `lambda x:`; see also bug #039 for the downstream cost when the shipped
macros carried an arrow.

## Axis 1 — known deparser differences

There is no single direction: v2.0 renders more explicitly in #2 and #5, more tersely in #1 and
#4, and in #3 it stops normalising at all (detail below). Read the table, not a rule.

| # | v1.5.6 | DuckDB v2.0 | Note |
|---|---|---|---|
| 1 | `... );;` (doubled trailing semicolon) | `... );` | |
| 2 | `FROM t INNER JOIN v ON (...) LEFT JOIN t2 USING (a)` | `FROM ((t INNER JOIN v ON (...)) LEFT JOIN t2 USING (a))` | |
| 3 | `CAST(x AS "DATE")` — canonical UPPERCASE, quoted, **whatever the source wrote** | `CAST(x AS date)` / `CAST(x AS DATE)` — **the source's own casing**, unquoted | See below: `replace(peek, '"', '')` cannot fix the casing |
| 4 | `main.list_value(1, 2, 3)[1]` | `list_value(1, 2, 3)[1]` | Only **implicitly generated** function references; an explicitly written `main.upper('x')` is unchanged on both lines |
| 5 | `SET  profiling_output TO '...'` (deparsed `PRAGMA`) | `SET  "profiling_output" TO '...'` | The doubled space after `SET` is on **both** lines |

**Difference #3 in detail**, because its mechanism is easy to get wrong. v1.5.6 **normalises the
cast type name to canonical UPPERCASE and quotes it, regardless of how the source wrote it.**
v2.0 **preserves the source's own casing and does not quote.** It is de-quoting plus case
*preservation*, not re-casing:

| Source written | v1.5.6 `peek` | v2.0 `peek` |
|---|---|---|
| `CAST(x AS date)` | `CAST(x AS "DATE")` | `CAST(x AS date)` |
| `CAST(x AS DATE)` | `CAST(x AS "DATE")` | `CAST(x AS DATE)` |
| `CAST(1 AS double)` | `CAST(1 AS "DOUBLE")` | `CAST(1 AS double)` |

Consequence: `replace(peek, '"', '')` removes the quotes but **cannot fix the casing**, so an
assertion whose source SQL writes a lowercase type name still differs across lines even with
that normalisation. Only a source that already writes the canonical uppercase name is
normalised by quote-stripping alone.

*Hypothesis, not established:* cyanoptera's `ParserOptions` carries
`IdentifierCaseMode identifier_case_mode = IdentifierCaseMode::PRESERVE_CASE` by default
(`duckdb/src/include/duckdb/parser/parser_options.hpp` — the same struct family F had to go
through), which is consistent with case preservation. But v1.5.6's `ParserOptions` has
`preserve_identifier_case = true`, which points the other way, and a cast type name is a type
name rather than an identifier. **The causal link is not established** — do not repeat it as
fact.

`peek` is `SQLStatement::ToString()` — see `DuckDBAdapter::CreateASTNode` in
`src/language_adapters/duckdb_adapter.cpp` (`node.peek = value`), whose call sites pass
`stmt.ToString()` / `expr.ToString()`. So every `peek` is upstream's deparser output verbatim.

### Caveat on the structural columns

On this adapter only `node_id`, `type`, `name`, `semantic_type`, `parent_id`, `depth` and
`descendant_count` carry real per-node data. `CreateASTNode` hardcodes `source_start_line`,
`source_end_line`, `source_start_column` and `source_end_column` to `1`, and `sibling_index` /
`children_count` to `0`. Giving this adapter real source positions would **add** a drift axis —
positions derived from a changed deparse move too.

## The open question

**A test that asserts on `peek` for the `duckdb` language cannot have one expected value that is
correct on both lines.** A policy is needed:

| Option | Cost |
|---|---|
| **Normalise** — canonicalise known differences in the query before asserting | Hides genuine deparse regressions; one rule per difference; and #3 above shows quote-stripping alone leaves a casing difference behind |
| **Tolerate** — stop asserting on `peek` text; assert `type` / `name` / `semantic_type` / structure instead | Loses the only coverage that the adapter reconstructs statements at all — and axis 2 shows those columns are *also* not line-invariant |
| **Pin** — keep v1.5.6 expectations and accept failures on the next leg | De-facto today while the canary runs `skip_tests: true` |

The decision is **explicitly unmade.** Do not assume one has been taken.

### Precedent already in the tree — both tactics, used ad hoc

- `test/sql/duckdb_parser_test.test:50` — **tolerates** axis 2 with
  `WHERE type NOT IN ('cast_expression', 'literal')`, with a comment explaining the boolean
  literal. From `b8c06a8`.
- `test/sql/duckdb_advanced_features.test`, Test 7 PRAGMA block — **normalises** axis 1 with
  `replace(peek, '"', '')`.
- `test/sql/duckdb_advanced_features.test`, Test 1 block — asserts **raw `peek`**.

So both tactics are already in use, each in one place, chosen per-incident. That is why the
policy needs stating once rather than being rediscovered per failing test.

## Measured exposure — smaller than first predicted

An earlier draft predicted "an immediate, noisy failure set" when tests are re-enabled on the
v2.0 leg. **That is wrong.** A test-impact run found **zero** deparser-caused failures across
the `duckdb`-language test files (six files parse with the `duckdb` language — the
`duckdb_*.test` set; a seventh, `test/sql/core/supported_languages.test`, only asserts the
language's registry metadata and never parses with it). The only v2.0 failures were missing
tree-sitter grammars in a duckdb-language-only build — an artifact of how that build was
configured, not drift.

Actual exposure is **two assertions in one file**
(`test/sql/duckdb_advanced_features.test`): one line-invariant, one already locally normalised.
Axis 2 is already absorbed at `duckdb_parser_test.test:50`. So this is a tidy-up with a policy
attached, not a fire.

## Why none of this shows in CI today

The `duckdb-next-build` canary runs `skip_tests: true`
(`.github/workflows/MainDistributionPipeline.yml`) because `ast_select` planning cost on DuckDB
v2.0 times the suite out — bug #040, upstream duckdb/duckdb#26036. The canary proves
**compiles**, not **right answer**.

## Next steps when picked up

1. **`git log --grep` first.** All three axes were already in `b8c06a8`'s commit message
   (2026-09-06). Re-deriving them cost a session. Check for a known break before investigating.
2. Enumerate every `peek` assertion for the `duckdb` language across `test/sql/`
   (`duckdb_advanced_features.test` carries them; `duckdb_hierarchical_parser.test` mentions
   `peek` only in a `DESCRIBE` column list and is drift-immune).
3. Pick one policy and apply it uniformly; record the choice here. Note it must cover axis 2 as
   well as axis 1 — the two call for different tactics.
4. If "normalise", put the rules in one place (a macro or test helper), not inline per test.
   Difference #3 (quoted canonical UPPERCASE vs unquoted source casing) is the one that
   defeats a naive `replace` — it needs a case fold too, or source SQL that writes the
   canonical uppercase type name.
5. `scripts/compare_duckdb_lines.sh` distinguishes structural drift (fails) from `peek` drift
   (reports, exits zero). Any policy chosen should agree with that script rather than
   re-deciding the question — and note that axis 3 is outside what it can see.

## Related

- `tracker/bugs/040-duckdb-v2-parse-perf-regression.md` — why the canary is build-only.
- `tracker/bugs/039-community-extensions-ships-pre-lambda-fix.md` — axis 3 downstream.
- `docs/development/duckdb-version-compatibility.md` — the full v1.5-vs-v2.0 picture.
- `b8c06a8` (PR #113) — the commit that first recorded all three axes.
