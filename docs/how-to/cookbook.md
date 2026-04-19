# Cookbook

Quick-reference table for common code analysis tasks. Each links to a focused how-to guide with full examples and variations.

## Task Index

| What you want to do | Guide | Key pattern |
|---------------------|-------|-------------|
| Find all functions | [Common Queries](common-queries.md) | `WHERE semantic_type = 'DEFINITION_FUNCTION'` |
| Find all classes | [Common Queries](common-queries.md) | `WHERE semantic_type = 'DEFINITION_CLASS'` |
| Find function calls | [Common Queries](common-queries.md) | `WHERE semantic_type = 'COMPUTATION_CALL'` |
| Find imports | [Common Queries](common-queries.md) | `WHERE semantic_type = 'EXTERNAL_IMPORT'` |
| Find by name | [Common Queries](common-queries.md) | `WHERE name LIKE '%pattern%'` |
| Build a call graph | [Call Graphs](../tutorials/call-graphs.md) | `JOIN ... ON scope.function = node_id` |
| Find unused functions | [Find Dead Code](find-dead-code.md) | `ast_dead_code('table')` or `.func:not(:is-called)` |
| Measure complexity | [Complexity Analysis](complexity-analysis.md) | `ast_function_metrics('table')` |
| Find long functions | [Complexity Analysis](complexity-analysis.md) | `WHERE end_line - start_line > 50` |
| Analyze nesting depth | [Complexity Analysis](complexity-analysis.md) | `ast_nesting_analysis('table')` |
| Find security issues | [Security Audit](security-audit.md) | `ast_security_audit('table')` |
| Find hardcoded secrets | [Security Audit](security-audit.md) | `string_contains_any_i(peek, [...])` |
| Parse once, query many | [Parse Once, Query Many](parse-once-query-many.md) | `CREATE TABLE ... AS SELECT * FROM read_ast(...)` |
| Export to Parquet | [Parse Once, Query Many](parse-once-query-many.md) | `COPY (...) TO 'file.parquet'` |
| Process multiple files | [Multi-File Processing](multi-file-processing.md) | `read_ast(['glob1', 'glob2'])` |
| Compare across languages | [Cross-Language Analysis](cross-language-analysis.md) | `GROUP BY language` |
| Use CSS selectors | [Selector Examples](selector-examples.md) | `ast_select('src/*.py', '.func')` |
| Extract context | [Context Extraction](extract-context.md) | `context := 'native'` |
| Search by structure | [Structural Search](structural-search.md) | `:has()`, `:match()`, combinators |

## Quick recipes

### Language distribution

```sql
SELECT language, COUNT(DISTINCT file_path) AS files, COUNT(*) AS nodes
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY files DESC;
```

### Definition inventory

```sql
SELECT
    language,
    COUNT(CASE WHEN semantic_type = 'DEFINITION_FUNCTION' THEN 1 END) AS functions,
    COUNT(CASE WHEN semantic_type = 'DEFINITION_CLASS' THEN 1 END) AS classes,
    COUNT(CASE WHEN semantic_type = 'DEFINITION_VARIABLE' THEN 1 END) AS variables
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY functions DESC;
```

### Most-called functions

```sql
SELECT name, COUNT(*) AS call_count
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 'COMPUTATION_CALL'
  AND name IS NOT NULL
GROUP BY name
ORDER BY call_count DESC
LIMIT 20;
```

### TODO/FIXME comments

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 'METADATA_COMMENT'
  AND (peek LIKE '%TODO%' OR peek LIKE '%FIXME%' OR peek LIKE '%HACK%')
ORDER BY file_path, start_line;
```

### Check parsing errors

```sql
SELECT file_path, type, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE type = 'ERROR'
LIMIT 10;
```

## See also

- [Common Queries](common-queries.md) — essential query patterns
- [Analysis Macros](../reference/analysis-macros.md) — built-in macro reference
- [Functions Reference](../reference/functions.md) — `read_ast`, `ast_select` signatures
- [Semantic Types](../reference/semantic-types.md) — full type table
