# Parse Once, Query Many

Parsing is the expensive step. If you're running multiple queries against the same codebase, parse once into a table and reuse it.

## Basic pattern

```sql
-- Parse once
CREATE TABLE codebase AS
SELECT * FROM read_ast('src/**/*.py', ignore_errors := true);

-- Query many times (no re-parsing)
SELECT name FROM codebase WHERE semantic_type = 'DEFINITION_FUNCTION';
SELECT name FROM codebase WHERE semantic_type = 'DEFINITION_CLASS';
SELECT name FROM codebase WHERE semantic_type = 'COMPUTATION_CALL' AND name = 'print';
```

## With ast_select_from

`ast_select` parses files on every call. `ast_select_from` takes a table name and skips parsing:

```sql
CREATE TABLE codebase AS SELECT * FROM read_ast('src/**/*.py');

-- Each query reuses the parsed table
SELECT name FROM ast_select_from('codebase', '.func:has(return_statement)');
SELECT name FROM ast_select_from('codebase', '.class:has(.func#__init__)');
SELECT name FROM ast_select_from('codebase', '.call#print');
```

## Temporary tables for session-scoped work

```sql
CREATE TEMP TABLE session_ast AS
SELECT * FROM read_ast('src/**/*.*', ignore_errors := true);

-- Available for the rest of this DuckDB session
SELECT language, COUNT(*) FROM session_ast GROUP BY language;
```

## Export to Parquet

For analysis across sessions or sharing with others:

```sql
COPY (
    SELECT * FROM read_ast('src/**/*.*', ignore_errors := true)
) TO 'codebase.parquet';

-- Later, in a new session:
CREATE TABLE codebase AS SELECT * FROM 'codebase.parquet';
SELECT name FROM ast_select_from('codebase', '.func');
```

## Selective export

If you only need definitions and calls:

```sql
COPY (
    SELECT
        node_id, parent_id, type, name, semantic_type,
        file_path, language, start_line, end_line,
        depth, descendant_count, scope, qualified_name
    FROM read_ast('src/**/*.*', ignore_errors := true)
    WHERE semantic_type IN (
        'DEFINITION_FUNCTION', 'DEFINITION_CLASS',
        'DEFINITION_VARIABLE', 'COMPUTATION_CALL',
        'EXTERNAL_IMPORT'
    )
) TO 'definitions_and_calls.parquet';
```

## When to parse fresh

- After editing source files (the cached table is stale)
- When you need columns you excluded from a selective export
- When using `context := 'native'` for parameters/modifiers (if the cached table used a different context level)

## See also

- [Functions Reference](../reference/functions.md) — `read_ast`, `ast_select_from` signatures
- [Parameters Reference](../reference/parameters.md) — context levels and output options
- [Multi-File Processing](multi-file-processing.md) — glob patterns and large codebases
