# 045 — Native `COPY ... TO (FORMAT ast)` Code Generator & Writer

**Status:** **LANDED 2026-10-08** — see "IMPLEMENTED 2026-10-08" at the end of
this file for what was built, which parts of the spec below were deliberately
*not* built, and why. The spec below is preserved as originally written.  
**Goal:** Enable direct serialization of AST queries and tables to source code files on disk via DuckDB's native `COPY ... TO` mechanism.

---

## Overview & Motivation

Currently, `ast_unparse()`, `ast_unparse_from()`, and `ast_unparse_code()` return a relational table of `(file_path VARCHAR, source VARCHAR)` records. To save reconstructed code to disk, users must write external application code or shell scripts.

By implementing a native DuckDB `CopyFunction` for `FORMAT ast`, DuckDB can directly serialize AST query streams into reconstructed, formatted source files on disk.

---

## Example Usage

### 1. Single File Export
```sql
COPY (
    SELECT * FROM my_transformed_ast
) TO 'src/generated/models.py' (
    FORMAT ast,
    LANGUAGE 'python',
    PRESET 'black',
    INDENT 4
);
```

### 2. Multi-File Refactoring Pipeline with Automatic Partitioning
```sql
-- Read codebase, apply AST patches, and write refactored source tree
COPY (
    SELECT * FROM ast_patch(
        read_ast('src/**/*.ts'),
        '.call#deprecatedMethod',
        'replacementMethod()'
    )
) TO 'dist/refactored/' (
    FORMAT ast,
    LANGUAGE 'typescript',
    PRESET 'prettier',
    PARTITION_BY (file_path),
    OVERWRITE true
);
```

---

## Supported Options in `COPY (FORMAT ast, ...)`

| Option | Type | Default | Description |
|---|---|---|---|
| `LANGUAGE` | `VARCHAR` | Auto-detect | Explicit language grammar (e.g. `'python'`, `'cpp'`, `'go'`). Inferred from file extension if omitted. |
| `PRESET` | `VARCHAR` | `'default'` | Tool style preset (e.g. `'pep8'`, `'black'`, `'prettier'`, `'gofmt'`, `'llvm'`, `'google'`). |
| `INDENT` | `BIGINT / VARCHAR` | `4` | Indentation width (`2`, `4`) or custom string (`'\t'`). |
| `LINE_ENDINGS` | `VARCHAR` | `'LF'` | Output line endings (`'LF'` or `'CRLF'`). |
| `OVERWRITE` | `BOOLEAN` | `true` | Overwrite existing target files. |
| `STRIP_COMMENTS` | `BOOLEAN` | `false` | Exclude comment AST nodes during code generation. |

---

## Architecture & Implementation Details

1. **`CopyFunction` Registration:**
   - Register `CopyFunction` with name `"ast"` in `sitting_duck_extension.cpp`.
   - Implement `CopyFunction::copy_to_bind`, `copy_to_initialize_global`, `copy_to_sink`, `copy_to_combine`, and `copy_to_finalize`.

2. **Streaming C++ AST Sink:**
   - Consume streaming `DataChunk`s of AST nodes from the query engine.
   - Group rows by `file_path` and document order (`node_id`).
   - Run C++ `UnparseASTStream` using registered `UnparseRule` entries.
   - Stream formatted bytes directly to DuckDB's `FileSystem` / output file handles with zero intermediate table allocation.

---

# IMPLEMENTED 2026-10-08 — and what changed from the spec above

**Status: landed.** `COPY (<query>) TO '<path>' (FORMAT ast)` writes byte-exact
source. The spec above is preserved as written; this section is what was built
and why it differs.

## The shape

```sql
COPY (FROM read_ast('src/main.c', source := 'full')) TO 'out/main.c' (FORMAT ast);
COPY (FROM read_ast('src/main.c', source := 'full')) TO 'out/main.c' (FORMAT ast, LANGUAGE 'c');
```

`LANGUAGE` is optional, inferred from the `language` column, an override and
disambiguator when given — the form settled by Teague 2026-10-07 and recorded in
`docs/planning/v2-architecture.md` ("write_ast laws") and in `047`'s 4b section.

## BYTE-EXACT OR REFUSE — the central decision

`047` gated this feature on 4a with the argument that *writing lossy output to
disk is worse than returning it in a result set, because a file looks
authoritative*. The hedge that argument invites is "byte-exact when it can be,
normalised otherwise, and it says which". **That was rejected.** A file that
says it is normalised in a message the user has already scrolled past is still a
file full of the wrong bytes. So:

* `FORMAT ast` writes a verbatim reproduction of the parsed file, or it writes
  **nothing**. There is no normalising mode and no fallback.
* Below `source := 'full'` the byte columns are NULL or absent, and the sink
  refuses — it does not quietly drop to the rules-based unparser.
* `PRESET` / `INDENT` / `LINE_ENDINGS` / `STRIP_COMMENTS` from the option table
  above are **rejected**, each with a message explaining that byte-exact output
  has no layout left to choose and naming the explicit lossy recipe:
  `COPY (SELECT encode(source) FROM ast_unparse(...)) TO 'out' (FORMAT blob)`.
  Lossy output therefore requires the user to spell out the normalising macro.
* `PARTITION_BY` / `PER_THREAD_OUTPUT` / `FILE_SIZE_BYTES` / `FILENAME_PATTERN`
  / `FILE_EXTENSION` are rejected: one parse, one file. The multi-file
  refactoring pipeline in the spec above is **not** implemented, and cannot be
  until a multi-file writer has an answer to "which file does this row belong
  to" that does not guess (#89). One `COPY` per file today.
* `OVERWRITE` is unnecessary (a single-file `COPY` replaces its destination) and
  `USE_TMP_FILE` is rejected because the sink always sets it — see below.

## ONE SPLICE, NOT TWO — how the writer reuses the existing logic

The byte-exact splice, its nine validation guards and its path-matching rules
already existed in SQL as `ast_unparse_exact_splice` (`047` 4b part 1, landed
earlier the same day). Reimplementing them in C++ would have put a second copy
of the byte-exact law behind a file that looks authoritative: the two would
drift, and the file would still look right. Three options were considered:

| option | verdict |
|---|---|
| C++ sink that materializes the rows and splices them itself | **rejected** — a second implementation of the law and of nine error messages |
| C++ sink that runs the SQL macro via a nested query at `copy_to_finalize` | **rejected** — needs query execution from inside an executing pipeline (scheduler re-entrancy), to no benefit over the option below |
| **statement rewrite at bind time** | **chosen** |

`CopyFunction::plan` (consulted by `Binder::BindCopyTo` before anything else)
rewrites

```sql
COPY (<query>) TO '<dest>' (FORMAT ast [, LANGUAGE ...] [, FILE_PATH ...])
```

into

```sql
COPY (
  WITH __sd_ast_copy_nodes AS MATERIALIZED (<query>),
       __sd_ast_copy_bytes AS (SELECT * FROM (SELECT fp, ast_source_bytes(fp) AS cblob
                                              FROM (SELECT DISTINCT file_path AS fp
                                                    FROM __sd_ast_copy_nodes))
                               WHERE cblob IS NOT NULL),
       __sd_ast_copy_out AS MATERIALIZED (
         SELECT * FROM ast_unparse_exact_splice('__sd_ast_copy_nodes', '__sd_ast_copy_bytes',
                                                language := …, file_path := …)),
       __sd_ast_copy_guard AS (SELECT CASE … error(…) … END AS ok FROM …)
  SELECT encode(o.source) FROM __sd_ast_copy_guard g LEFT JOIN __sd_ast_copy_out o ON (g.ok)
) TO '<dest>' (FORMAT blob, USE_TMP_FILE true)
```

and re-binds it. Consequences worth recording:

* **No writer code.** `FORMAT blob` is DuckDB's own byte-verbatim copy function
  (`duckdb/src/function/copy_blob.cpp`): a BLOB column's bytes and nothing else
  — no header, no quoting, **no row terminator**, so there is no trailing
  newline to append by accident.
* **No splice code, and no second set of error messages.** Every guard the
  macro has fires through `COPY`, verbatim, and is asserted doing so.
* **The user's query node is MOVED into the CTE**, never re-serialised through
  `ToString()`; a round trip through the parser would be a second, lossy
  interpretation of the user's SQL. The template is parsed, its placeholder
  CTE's `query` is replaced with the user's node, and the result is grafted onto
  the *original* `CopyStatement`, which the caller owns and keeps alive.
* **No recursion.** The rewritten statement's format is `blob`, which has no
  `plan`. Re-binding is also idempotent, which matters for `PREPARE`/`EXECUTE`
  (verified) — a second bind of an already-rewritten statement produces the same
  plan.
* **`AS MATERIALIZED` is load-bearing twice.** DuckDB inlines CTEs by default,
  and the node relation is referenced three times (bytes, splice, row-count
  guard) — without it a `COPY` would parse its input file three times.
* **The guard is a LEFT JOIN, not a WHERE.** A refusal has to fire when the
  query produced *no* rows, and a `WHERE (SELECT …)` over an empty relation may
  never evaluate its subquery. Joining *from* the single-row guard makes the
  aggregate's evaluation structural.

## `ast_source_bytes(path) → BLOB` — the one new primitive

DuckDB table functions take only constant-foldable arguments, so
`read_blob(file_path)` over a node table is not expressible. That is exactly why
`ast_unparse_exact_from(ast_table, files, …)` has to be handed a `files` literal
covering a path every row already names. The sink cannot be handed one: it sees
a query, not a path, and the path lives in the data. `ast_source_bytes` closes
that substrate gap per row. It returns **NULL** for a path that does not exist
(including `'<inline>'`), so the splice's own "not readable as bytes" and
"in-memory `parse_ast()` output" messages survive with their instructions
intact; a path that exists but cannot be read still throws. File access goes
through the client context's `FileSystem`, so `enable_external_access` and
`allowed_directories` apply as they do to `read_blob`.

Follow-up this enables (**not** done here — it would change an existing macro's
signature): `ast_unparse_exact_from`'s `files` argument could become optional.

## The join-key question, answered

`ast_unparse.sql`'s PATH MATCHING block exists because three relations had to
agree on one spelling of a path — the node table's `file_path`, the blob
relation's `fp`, and a `file_path :=` argument — while DuckDB's globber returns
`./`-prefixed and, on Windows, backslash-separated paths. **That hazard is
structurally absent from the sink:** its blob relation's `fp` is derived from the
node table's own `file_path` column in the same statement, so the two sides of
the join are the same string by construction. Only an explicit `FILE_PATH`
override reintroduces a second spelling, and the macro already normalises it.

## Destination integrity

Every guard fires *after* the destination would have been opened, so the sink
forces `USE_TMP_FILE true` for a file destination. Measured over all fourteen
refusals: the destination does not exist afterwards, no `tmp_*` file is
stranded, and a pre-existing destination still holds its original bytes. The
same mechanism makes **in-place rewriting** (`TO` the file that was parsed) safe
— the splice has read every byte it needs before the rename — and that is
asserted.

Two destinations are special, and both were nearly missed:

* **Remote (`s3://`, `https://`, …) is REFUSED at bind time.** DuckDB's `COPY`
  binder discards `use_tmp_file` *unconditionally* for a remote path
  (`bind_copy.cpp`: `if (is_remote_file) { use_tmp_file = false; }`), so the
  forced `true` would be silently dropped and a refusal would leave a 0-byte
  object. A 0-byte S3 object looks exactly as authoritative as a 0-byte file,
  which is the thing this feature exists to prevent — so rather than document a
  guarantee that quietly does not hold there, the sink refuses and names the
  explicit `FORMAT blob` alternative.
* **`/dev/stdout` is written WITHOUT a temporary file**, matching the same
  special case DuckDB's binder makes: it is a stream with nothing to truncate,
  and `/dev/tmp_stdout` is nonsense. Verified: it pipes the reconstructed source
  out verbatim.

`PREPARE`/`EXECUTE` were verified with a parameterised input path and two
different arguments, which also exercises the node-column check binding the
user's query (with its `$1`) a second time.

## 4b part 2 is NOT needed for this

Nothing is generated: every byte written comes from the original file,
attributed either to a leaf or to a gap. There is no synthesized node to lay
out, so no template vocabulary is required. Templates become necessary when the
sink must write a tree for which no file can supply bytes — `ast_patch` output
re-written in place, say — and that is a different feature, not half of this one.

## Incidental finding — `descendant_count` wraps at 65 536 (#212, FIXED 2026-10-09)

`descendant_count` is computed as a `uint32` and then stored through
`AstNode::legacy_descendant_count`, a `uint16`
(`src/include/ast_type.hpp`: `legacy_descendant_count =
static_cast<uint16_t>(descendant_count)`), and that field is what the row
emitters write into the `UINTEGER` column. **Measured:** a 100 001-node tree
reports `root.descendant_count = 34 464 = 100 000 − 65 536`.

The splice's completeness guard is `row count = root.descendant_count + 1`, so
**any file whose AST exceeds 65 536 nodes is refused** — loudly, with the "not a
complete tree" message, which is the right behaviour for the wrong reason.
Byte-exactness is never silently lost; a correct file is rejected. Pre-existing,
and it affects `ast_unparse_exact*` identically (`047`'s 176-file sweep did not
reach a file that large). Pinned in `test/sql/ast_copy_format.test` §2 so that a
fix fails where the premise is written down.

**Fixed.** The four legacy mirrors were widened to `uint32`, which is what every
emitted column already declared on the SQL side; nothing but the C++ field width
was ever the limit. Two sibling truncations surfaced in the same pass and were
measured on the pre-fix binary before being fixed:

| field | mirror | fixture | before → after |
|---|---|---|---|
| `descendant_count` | `uint16` | 100 001-node tree | 34 464 → **100 000** |
| `children_count` | `uint16` | 70 000 sibling statements | 4 464 → **70 000** |
| `depth` / `node_depth` | `uint8` | 3 000-term expression | 255 → **3 003** |

`start_column`/`end_column` had the same `uint16` mirror and no user-visible
symptom: `read_ast` emits columns from the `uint32` `source_*` fields, and
`parse_ast_list`'s FULL branch is unreachable from SQL. Widened anyway.

The serious half was never the refusal — it was that `node_id BETWEEN x AND
x + descendant_count`, the O(1) subtree idiom `CLAUDE.md` recommends, returned
**34 465 of 100 001 rows with no error**. `ast_unparse_exact*` and this writer
were the only consumers that failed loudly.

`§2`'s pin is now the positive assertion (`root_dc = 100 000`) plus a byte-exact
round trip of that same 100 001-node file; `ast_unparse_exact.test` §5b still
refuses a genuine filtered subset, which is the control that the guard was not
merely loosened. Regression test: `test/sql/bugs/issue_212_count_field_widths.test`.

## Cross-line compatibility (issue #213)

The first version compiled on the pinned v1.5.6 and **not** on the
`v2.0-cyanoptera` canary — five breaks, three of them families the repo had
already solved. Recorded in `docs/development/duckdb-version-compatibility.md`:

- **family A** (`string` → `Identifier`) on `BoundStatement::names` and on
  `CopyInfo::options`' keys — absorbed with `IdentString()`. Writing a key back
  needs nothing, because `Identifier`'s constructor from a string *literal* is
  implicit by design.
- **family F** (`Parser` has no zero-argument constructor; `ParserOptions()` is
  private) — absorbed with `DefaultParserOptions()`.
- **family H, new**: `CommonTableExpressionInfo::query` (a `SelectStatement`)
  became `query_node` (a `QueryNode`) — absorbed with `SetCTEQuery()`.

`IdentString` and `DefaultParserOptions` were **moved** out of
`duckdb_adapter.cpp`'s anonymous namespace into the new
`src/include/duckdb_parser_compat.hpp` rather than copied, so there is one
probe per upstream change. Verified with a two-line `-fsyntax-only` sweep plus
a v1.5.6 **control** sweep at both `-std=c++11` and `-std=c++17`: all five
breaks were cyanoptera-only, none a harness artifact, and
`ast_source_bytes_function.cpp` needed no shim at all.

## Files

| file | what |
|---|---|
| `src/ast_copy_function.cpp` | the `plan` rewrite, option validation, the node-column check |
| `src/include/duckdb_parser_compat.hpp` | the shared parser shims (families A, F, H) |
| `src/ast_source_bytes_function.cpp` | `ast_source_bytes(path)` |
| `test/sql/ast_copy_format.test` | 140 assertions |
| `API_REFERENCE.md` | `COPY … (FORMAT ast)` and `ast_source_bytes` |

Not touched: `src/sql_macros/ast_unparse.sql` (the splice and its guards are
used exactly as they were), and the `ast_unparse*` macros' behaviour.
