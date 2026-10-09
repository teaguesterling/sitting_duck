# DuckDB version compatibility (v1.5 vs DuckDB v2.0)

**"DuckDB v2.0" here means upstream DuckDB's next line** (branch `v2.0-cyanoptera`, and
`main`). It is unrelated to *sitting_duck* v2.0, which is this extension's own architecture
split — see [planning/v2-architecture.md](../planning/v2-architecture.md) and
`tracker/features/044-v2-m1-contract.md`.

sitting_duck builds against two DuckDB lines from one source tree:

| Leg | DuckDB | Where |
|---|---|---|
| Stable (shipped) | `v1.5.6`, CI tools `v1.5-variegata` | `duckdb-stable-build` in `.github/workflows/MainDistributionPipeline.yml` |
| Next (canary) | `v2.0-cyanoptera`, CI tools `main` | `duckdb-next-build`, same file, `skip_tests: true` |

## The two-line situation

DuckDB v1.5.6 **partially backported** the v2.0 API refactor. So "is this v2.0?" is not one
question — it is one question *per accessor*. The first version of the parser-accessor shim
used a single `HasQualifiedName<CreateInfo>` tag on the theory that the entry names moved as
one upstream change; v1.5.6 backported `CreateInfo::GetQualifiedName()` and the per-entry
getters (returning plain `string`, not `Identifier`) while **not** backporting
`CreateSchemaInfo::SchemaName()` or `BaseTableRef::Table()`. One tag then answered "v2.0" and
two call sites named members that did not exist. Fixed in `7d3a86f` by probing each accessor
separately. The same trap bit the sentinel choice for typed kwargs (see
[Sentinel rule](#sentinel-rule)).

**Expect further partial backports.** Add a probe; never widen an existing one.

## How to decide what mechanism to use

Four rungs, strongest first. Use the highest rung that works.

### 1. Derive the type from upstream's own declaration

Best, because it cannot drift from the thing that actually changed.

```cpp
using CompatName = typename std::remove_reference<decltype(
    std::declval<TableFunctionBindInput &>().input_table_names)>::type::value_type;
using CompatChildKey = typename child_list_t<LogicalType>::value_type::first_type;
```

Both in `src/include/duckdb_compat.hpp`. A `__has_include("duckdb/common/identifier.hpp")`
probe *was* used for the bind-name type and was wrong: v1.5 backported that header while binds
still took `vector<string>`. And the two containers above have already diverged in practice —
on one DuckDB the stable leg built, bind names were `Identifier` while `child_list_t` keys were
still `string`, so pairing a bind name straight into a `child_list_t` stopped compiling.
Deriving each from its own container makes that a non-event.

### 2. Plain overload resolution

Where the two lines differ in *shape* rather than in a name. `CompatStructEntry` takes
`Vector &` and `const unique_ptr<Vector> &`; only the viable overload is ever selected, so this
cannot get its polarity backwards.

### 3. SFINAE probe + tag dispatch

Use a probe when **both lines express the same idea with different spellings** — a probe can
absorb a spelling. Probe the *v2.0-only* API, never the field being replaced: a probe aimed at
the old field can be true on both lines.

Two template gotchas, both paid for:

- **Both tag-dispatch overloads must be templates.** A non-template `inline` overload is
  type-checked whether it is called or not, so the branch naming the absent member still
  breaks the build. See `DefaultParserOptionsImpl` in
  `src/language_adapters/duckdb_adapter.cpp`.
- **The entry point must *not* be a template.** A default template argument is inert when
  deduction succeeds, so `CompatWithAlias(LogicalType::VARCHAR, "x")` would deduce
  `TYPE = LogicalTypeId` (`LogicalType::VARCHAR` is a `static constexpr LogicalTypeId`) and
  hard-error on the member lookup. Take a concrete `LogicalType` by value instead. See
  `CompatWithAlias` in `duckdb_compat.hpp`.

Tag dispatch rather than `if constexpr` because this extension compiles at C++11 on the stable
leg; forcing C++17 against v1.5's C++11 internals produces multiple-definition errors at link
time (static const data members acquire implicit inline linkage in C++17 but not C++11).

### 4. `#if __has_include` — last resort

Use `#if` **only when the v2.0 spelling names a TYPE that does not exist on v1.5.** Non-dependent
names in a template are looked up when the template is *defined*, not when it is instantiated,
so no template can hide an absent type. The only shim in-tree that *requires* this is typed
kwargs (`TypedKwargs`) in `src/include/named_parameter_compat.hpp`, and the branch is confined
to that one header rather than sprayed across the four translation units that use it.

Several other shims in `duckdb_compat.hpp` (`CompatBoundBindInfo`, the parsed-expression
accessors, `CompatSetOutputCardinality`, `Compat{Unary,Binary}ExecuteWithNulls`,
`DUCKDB_SCALAR_BIND_PARAMS`) are `#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS` — one coarse sentinel
covering several independent upstream changes. That predates the per-change probe rule the same
header now states ("a version macro says *when* something changed, a probe says whether it
changed *here*"), and it is exactly the shape that picks the wrong branch when the changes land
in different releases. Candidate cleanup, not a current break.

### Sentinel rule

A `__has_include` sentinel must **co-vary with the API it gates**, not merely with the major
version.

| Candidate | Verdict |
|---|---|
| `duckdb/main/capi/capi_function_signature.hpp` | In use. Absent on v1.5.6; arrived on cyanoptera in the same work that gave `TableFunction` its signature. Still a *proxy* — `TypedKwargs` lives in `duckdb/function/function.hpp`, which both lines have. |
| `duckdb/common/enums/identifier_case_mode.hpp` | **Broke the build (99 errors).** v2.0-only, so it looked correct — but it had already landed at cyanoptera `e366461e30` while `TableFunction::GetSignature` had not, selecting the v2.0 path against a DuckDB with no kwargs API. |
| `duckdb/common/identifier.hpp` | Useless. v1.5.6 backported it, so it is present on both lines. |

**Verify a candidate at the commit you build against, not on the branch tip.**

## Shim inventory

Everything that differs between the lines is behind one of these. Rationale lives in the
comments next to each symbol — this table is the index, not a copy.

| Symbol / macro | File | What changed upstream | Rung |
|---|---|---|---|
| `CompatName`, `CompatMakeName`, `CompatNameStr`, `CompatAssignNames` | `duckdb_compat.hpp` | Bind-signature name type `string` → `Identifier` (explicit ctor from runtime string) | 1 |
| `CompatChildKey`, `CompatMakeChildKey` | `duckdb_compat.hpp` | `child_list_t` key type, derived *separately* from bind names | 1 |
| `CompatStructEntry`, `CompatStructGetField` | `duckdb_compat.hpp` | `StructVector::GetEntries` → `vector<Vector>&` (was `vector<unique_ptr<Vector>>&`) | 2 |
| `CompatWithAlias` | `duckdb_compat.hpp` | `LogicalType::SetAlias` removed → `WithAlias()` returns a copy | 3 |
| `CompatFlatDataMutable` | `duckdb_compat.hpp` | `FlatVector::GetData<T>` returns `const T*`; `GetDataMutable<T>` is the write accessor | 3 |
| `CompatFlatValidityMutable` | `duckdb_compat.hpp` | `FlatVector::Validity` const-split; `ValidityMutable` for writes | 3 |
| `CompatBoundBindInfo` | `duckdb_compat.hpp` | `BoundFunctionExpression::bind_info` private → `BindInfo()` / `BindInfoMutable()` | `#if` |
| `CompatConstantValue`, `CompatFunctionName`, `CompatFunctionArgExprs`, `CompatComparisonLeft/Right`, `CompatConjunctionChildren` | `duckdb_compat.hpp` | Parser expression members private + accessors; `ConstantExpression::value` → `Literal` | `#if` |
| `CompatSetOutputCardinality` | `duckdb_compat.hpp` | Per-vector size tracking: `SetChildCardinality` publishes index-written children | `#if` |
| `Compat{Unary,Binary}ExecuteWithNulls` | `duckdb_compat.hpp` | `ExecuteWithNulls` removed (`987ea2c409`); null-emitting overload takes `std::optional` | `#if` |
| `DUCKDB_SCALAR_BIND_PARAMS` / `_CONTEXT` / `_ARGS` | `duckdb_compat.hpp` | Scalar bind signature collapsed into `BindScalarFunctionInput &` | `#if` |
| `SetValueCasted` | `duckdb_compat.hpp` | `SetValue`'s fallback cast stopped seeing extension-registered casts | n/a (always) |
| `QualifiedNameTag`, `SchemaNameTag`, `TableAccessorTag`, `Create*EntryName` | `duckdb_adapter.cpp` | `CreateInfo` & friends: public name fields → `Get<Entry>Name()`; `BaseTableRef::table_name` → `Table()` | 3 |
| `IdentString` | `duckdb_parser_compat.hpp` | `string` → `Identifier` wherever upstream names a SQL identifier. **Moved out of `duckdb_adapter.cpp` 2026-10-09** when a second consumer appeared (`BoundStatement::names`, `CopyInfo::options` keys) | 2 |
| `DefaultParserOptions`, `HasBuiltinParserOptions` | `duckdb_parser_compat.hpp` | `ParserOptions()` default ctor made private; `Parser` lost its zero-arg ctor. **Moved out of `duckdb_adapter.cpp` 2026-10-09**, same reason | 3 |
| `SetCTEQuery`, `HasCTEQueryNode` | `duckdb_parser_compat.hpp` | `CommonTableExpressionInfo::query` (a `SelectStatement`) → `query_node` (a `QueryNode`) — **family H**, new 2026-10-09 | 3 |
| `DeclareNamedParameters`, `ExtendNamedParameters`, `FindNamedParameter`, `NamedParamMap` | `named_parameter_compat.hpp` | `TableFunction::named_parameters` removed → `GetSignature().WithTypedKwargs(...)`; bind map retyped `case_insensitive_map_t<Value>` → `named_argument_map_t` (keyed by `Identifier`) | 4 |
| `ArgumentTypesOf`, `GetArgumentTypes` | `function_doc_helper.hpp` | `SimpleFunction::arguments` removed → `GetSignature().GetParameters()[i].GetType()`; set elements became `shared_ptr<const F>` | 3 |

The **heavyweight** parser-object accessors live in `duckdb_adapter.cpp` rather than
`duckdb_compat.hpp` on purpose: they would drag ten `parser/parsed_data` headers into a header
that fourteen translation units include, and that adapter is their only consumer.

`src/include/duckdb_parser_compat.hpp` is the middle ground, added 2026-10-09: the parser shims
with **more than one** consumer, costing three parser includes in the two translation units that
want them rather than in all fourteen. `IdentString` and `DefaultParserOptions` were lifted into
it from `duckdb_adapter.cpp` when `COPY … TO (FORMAT ast)` hit the same two families. They were
**moved, not copied** — a second copy of a compat probe is how these drift, and they drift
silently, because the partial backports make "is this v2.0?" a question per accessor.

### Discovery-order cross-reference

The session notes name break "families" by discovery order. Mapping, for anyone reading them:

| Family | What | Landed in |
|---|---|---|
| A | `CreateInfo` getters — **partially** backported to v1.5.6 | `7d3a86f` |
| B | `QueryResult::GetNames()` | shimmed in **duck_hunt**, not here (`src/include/duckdb_compat.hpp`: `CompatResultNames` + a `HasGetNames` probe) |
| C | `TableFunction::named_parameters` → typed kwargs | PR #183 / `302a738` |
| D | `GetValue` changes | *no distinct shim located in-tree — see note below* |
| E | `SimpleFunction::arguments` removed | PR #175 (`114920e`, `8568563`); PR #177 relaxed the matching test |
| F | `Parser` / `ParserOptions` default construction | PR #192 / `0c49b2c` |
| G | Cross-line drift in the `duckdb` language — deparser, parse-tree and bind-time; *no* build break | tracker bug #041 |
| H | `CommonTableExpressionInfo::query` → `query_node` | issue #213 (`COPY … TO (FORMAT ast)`, `045`/#174) |

!!! note "The families are fleet-wide; the shims are per-repo"
    These families describe upstream DuckDB changes, so they bite several extensions — but each
    repo carries its own shim, and not every family bites every repo. Family **B** is shimmed
    in **duck_hunt** (`src/include/duckdb_compat.hpp`, `CompatResultNames` behind a
    `HasGetNames` probe), because duck_hunt reads a `QueryResult`'s column names; sitting_duck
    does not, so it has no B shim and needs none. When reading fleet-wide notes, ask which repo
    a family bites before looking for its shim here.

    Family **D** (`GetValue` changes) remains **unlocated**: no `GetValue` shim exists in this
    repo and the `Value::GetValue<T>()` call sites in `src/` are unshimmed. Do not treat it as
    having a shim here until one is found.

## Family F: why it was a build error and not a runtime failure

Worth keeping, because it is the clearest case of upstream doing the right thing.

- v1.5: `explicit Parser(ParserOptions options = ParserOptions())` — `make_uniq<Parser>()` works.
- v2.0: `ParserOptions`' default constructor is **private** (friending only `ClientContext`);
  `ParserOptions::Builtin()` is the sanctioned "parse without a `ClientContext`" configuration.
  `Parser` has no zero-argument constructor at all.

This is not cosmetic. `Builtin()` installs `CompiledGrammar::DefaultGrammar()`, and v2.0's
`Parser::GetGrammar()` throws `InternalException("ParserOptions requires a compiled grammar")`
when the options carry none. **Had upstream left the default constructor public, we would have
built cleanly and failed at parse time.** The private constructor is what turned a runtime
failure into a build error.

A probe, not an `#if`, because `ParserOptions` exists on both lines and only the spelling of
"the default options" differs.

Upstream landing window, recorded from the session that hit it and **not verifiable from this
tree**: between 2026-10-03 16:39 UTC and 2026-10-05 04:36 UTC on `v2.0-cyanoptera`. A canary
green before that window was stale, not safe.

## Family H: where a CTE keeps its query

Discovered 2026-10-09 by the `v2.0-cyanoptera` canary on `src/ast_copy_function.cpp`
(issue #213), and the only one of that build's five breaks with no existing shim.

| | Member | Holds |
|---|---|---|
| v1.5.6 | `unique_ptr<SelectStatement> query` | a statement wrapping the node |
| DuckDB v2.0 | `unique_ptr<QueryNode> query_node` | the node, directly |

v2.0 **deleted `query`**, so assigning it is a hard error rather than a deprecation. Two things
make this one easy to misdiagnose:

- **`SelectStatement` still appears in that header on v2.0**, in a
  `CommonTableExpressionInfo(unique_ptr<SelectStatement>, unique_ptr<QueryNode>)` constructor
  and in `GetQueryForSerialization()`, both for deserialization compatibility. A grep for the
  type finds hits on both lines; only the *member* moved.
- `aliases` changed type in the same struct (`vector<string>` → `vector<Identifier>`), which is
  family A, not this. Anything touching CTE aliases needs `IdentString` as well.

Shimmed with a probe (`SetCTEQuery` in `duckdb_parser_compat.hpp`), not an `#if`, by the rule
above: both `SelectStatement` and `QueryNode` exist on both lines, so no absent **type** is
named at template-definition time, and only the spelling of "the CTE's root query" differs. The
probe aims at the v2.0-only member — `query_node` — never at the one being replaced.

One wrinkle worth keeping: the v1.5 branch's `make_uniq<SelectStatement>()` is a
**non-dependent** expression inside the template, so it is checked when the template is
*defined*, not when it is instantiated. It is valid on both lines (v2.0's `SelectStatement` is
still default-constructible with a public `node`; it is just no longer what a CTE holds), which
is the only reason that branch can name it at all. Had v2.0 also sealed `SelectStatement`, this
would have had to become an `#if`.

## Cross-line drift in the `duckdb` language

A 43-statement corpus (`test/corpus/duckdb_sql/kitchen_sink.sql`) and a 120 KB real-world file
showed **no** structural drift between v1.5.6 and v2.0-cyanoptera, and only `peek` differing.
That result is real but **not general** — see [Axis 2](#axis-2-parse-tree-drift-is-real-counterexample),
where a one-line query does produce structural drift. Treat the corpus result as a statement
about that corpus.

`peek` is `SQLStatement::ToString()` — see `DuckDBAdapter::CreateASTNode` (`node.peek = value`)
and its call sites, which pass `stmt.ToString()`. So every `peek` on this adapter is upstream's
deparser output verbatim.

On this adapter only `node_id`, `type`, `name`, `semantic_type`, `parent_id`, `depth` and
`descendant_count` carry real per-node data. `source_start_line`, `source_end_line`,
`source_start_column` and `source_end_column` are hardcoded to `1`, and `sibling_index` /
`children_count` to `0`, in `CreateASTNode` — so those columns agree *trivially* on both lines.
Giving this adapter real source positions would add a drift axis, since positions derived from
a changed deparse would move too.

### Three drift axes

**Do not believe "only `peek` differs."** That framing was wrong and is retired here. There are
three independent ways the `duckdb` language behaves differently across the lines, and only the
first is cosmetic.

| Axis | What moves | Visible to | Is it a bug? |
|---|---|---|---|
| 1. Deparser / `peek` | `SQLStatement::ToString()` rendering | `parse_ast` / `read_ast` | No — upstream cosmetics |
| 2. Parse-tree drift | the upstream expression *class*, so `type` + `name` + `semantic_type` change | `parse_ast` / `read_ast` | Behavioural change; queries and tests can break |
| 3. Bind-time rejection | SQL that parses but no longer binds | **nothing in this extension** | Yes, for anyone executing that SQL |

#### Axis 2: parse-tree drift is real (counterexample)

```sql
parse_ast('SELECT u.name FROM users u WHERE u.active = true', 'duckdb')  -- node_id 10
```

| | `type` | `name` | `semantic_type` |
|---|---|---|---|
| v1.5.6 | `cast_expression` | *(empty)* | `COMPUTATION_EXPRESSION` |
| DuckDB v2.0 | `literal` | `true` | `LITERAL_ATOMIC` |

v2.0 stops wrapping a boolean literal in an implicit cast. That is three of the columns listed
above as structure-carrying, changing with **no build break**.

A 43-statement corpus showing no structural drift is therefore **not** evidence of no drift — it
contained no boolean literal as a comparison operand. A corpus that finds nothing tells you
nothing unless you can say what it would have caught.

#### Axis 3: bind-time rejection is invisible to this extension

The deprecated lambda arrow `x -> ...` is a **warning** on v1.5.6 and a **hard
`Binder Error: Deprecated lambda arrow (->) detected`** on DuckDB v2.0. `lambda x:` works on
both. `parse_ast` / `read_ast` parse but never bind, so **a parse-only corpus can happily
contain SQL that cannot execute on v2.0.** See also
`tracker/bugs/039-community-extensions-ships-pre-lambda-fix.md` for the downstream cost of this
one.

#### Axis 1: known deparser differences

There is no single direction: v2.0 renders more explicitly in #2 and #5, more tersely in #1 and
#4, and in #3 it stops normalising at all (see the detail below). Read the table, not a rule.

| # | v1.5.6 | DuckDB v2.0 | Note |
|---|---|---|---|
| 1 | `... );;` (doubled trailing semicolon) | `... );` | |
| 2 | `FROM t INNER JOIN v ON (...) LEFT JOIN t2 USING (a)` | `FROM ((t INNER JOIN v ON (...)) LEFT JOIN t2 USING (a))` | |
| 3 | `CAST(x AS "DATE")` — canonical UPPERCASE, quoted, **whatever the source wrote** | `CAST(x AS date)` / `CAST(x AS DATE)` — **the source's own casing**, unquoted | See below: `replace(peek, '"', '')` cannot fix the casing |
| 4 | `main.list_value(1, 2, 3)[1]` | `list_value(1, 2, 3)[1]` | Only **implicitly generated** function references; an explicitly written `main.upper('x')` is unchanged on both lines |
| 5 | `SET  profiling_output TO '...'` (deparsed `PRAGMA`) | `SET  "profiling_output" TO '...'` | The doubled space after `SET` is present on **both** lines |

(#5 is already worked around in-tree: the Test 7 PRAGMA block of
`test/sql/duckdb_advanced_features.test` normalises with `replace(peek, '"', '')`.)

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

### Node *types* can drift without a build break

An earlier version of this doc argued the opposite, from the fact that sitting_duck's node type
strings are its own hardcoded literals (`"select_statement"`, `"create_macro"`, …) selected by
`switch` on DuckDB enums (`StatementType`, `CatalogType`) in `duckdb_adapter.cpp`. **That
reasoning is fallacious.** Owning the literals means an upstream enumerator *rename* breaks the
build — a genuine and useful property. It says nothing about **which** literal gets selected,
because that depends on the upstream expression class, and that is exactly what changed in the
boolean-literal case above: a different `switch` arm, a different hardcoded string, no
compiler complaint.

### Check `git log` before investigating from scratch

The boolean-literal divergence has been known in-tree since **2026-09-06**. Commit `b8c06a8`
(PR #113) documents it in its message ("v2.0 parses `true` as a `literal` (LITERAL_ATOMIC)
where v1.5 emitted a `cast_expression`") and absorbs it with
`WHERE type NOT IN ('cast_expression', 'literal')` at `test/sql/duckdb_parser_test.test:50`.
The same commit records the lambda-arrow binder error and the `PRAGMA`→`SET` quoting.

Lesson: `git log --grep` for the symptom before re-deriving a cross-version break. All three
axes above were already in one commit message.

## Verifying a DuckDB bump

### The ladder

**compiles → loads → binds → returns the right answer.** The canary runs with
`skip_tests: true`, so a green canary proves only the first rung. A green canary dated *before*
an upstream change is stale, not safe.

### Enumerating breaks cheaply

Ninja stops at the first error, so CI reveals **one** break when there may be many — the PR
#192 failure stopped at target 37 of 785. A ~20–40 minute CI round trip per error is not a
workable loop. Instead:

```bash
# Adds objects WITHOUT moving HEAD — safe in a shared checkout / worktree layout.
git -C duckdb fetch origin v2.0-cyanoptera
git -C duckdb archive FETCH_HEAD src/include third_party | tar -x -C <scratch>
# Then, per translation unit, against BOTH header trees:
g++ -fsyntax-only -std=c++17 -I<scratch>/src/include ... src/<tu>.cpp
```

Minutes instead of hours, and it enumerates *all* the breaks rather than the first one.

### Closing the "compiles vs right answer" gap for the `duckdb` adapter

`scripts/compare_duckdb_lines.sh` diffs the DuckDB-SQL AST produced on two DuckDB lines and
separates the two kinds of drift: **structural** drift (node ids, types, names, semantic types,
parentage, depth, sibling order, counts, line spans) is a real regression and exits non-zero;
**`peek`** drift is upstream re-rendering and is reported loudly but exits zero. That split is
the whole point — run it after any `duckdb` submodule bump.

It covers axes 1 and 2. **Axis 3 is outside its reach**, and outside this extension's: a script
driven through `parse_ast` / `read_ast` never binds, so SQL that parses on both lines and only
*executes* on one looks identical to it. That gap is the "binds" rung of the ladder above, and
nothing in this repo currently tests it for the `duckdb` language.

### Always run the control sweep

Run the identical sweep against the **old** headers. Doing so reduced 6 apparent cyanoptera
breaks to **1**: the other 5 failed on v1.5.6 too, being dead files not referenced by
`CMakeLists.txt` whose generated headers the harness lacked.

### Running tests from an agent or CI: pass `-no-agent`

The DuckDB v2.0 CLI enters **agent mode** when `AI_AGENT` or `CLAUDECODE` is set *and* stdout
is not a tty. In that mode it prints a banner to stderr, renders result tables as markdown, and
formats errors as JSON on stderr. Exit codes are unaffected and errors are **not** suppressed
(verified for both bind-time and execution-time exceptions). Any script that parses CLI output
must pass `-no-agent` to restore classic behaviour.

**`-no-agent` is a v2.0-only flag — it does not exist on the v1.5.6 CLI.** A script that must
run against both lines cannot pass it unconditionally; gate it on the binary, or unset
`AI_AGENT` / `CLAUDECODE` instead, which suppresses agent mode without naming a flag either
line lacks.

## Known non-compile gaps

- **`ast_select` planning cost on DuckDB v2.0** — `tracker/bugs/040-duckdb-v2-parse-perf-regression.md`;
  upstream duckdb/duckdb#26036. This is why the canary is build-only.
- **Cross-line drift in the `duckdb` language** (deparser rendering, parse-tree drift,
  bind-time rejection) — `tracker/bugs/041-duckdb-deparser-drift-peek-expectations.md`.
