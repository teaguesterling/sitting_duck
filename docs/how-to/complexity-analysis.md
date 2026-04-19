# Complexity Analysis

Measure function complexity, nesting depth, and code size to find refactoring candidates.

## Function metrics: ast_function_metrics

```sql
CREATE TABLE codebase AS SELECT * FROM read_ast('src/**/*.py');

SELECT name, file_path, lines, cyclomatic, return_count, loop_count
FROM ast_function_metrics('codebase')
WHERE cyclomatic > 5
ORDER BY cyclomatic DESC
LIMIT 20;
```

The macro returns one row per function with:
- `lines` — line count (end_line - start_line + 1)
- `cyclomatic` — proxy: count of branches + loops + exception handlers
- `return_count`, `conditional_count`, `loop_count` — individual control-flow counts

## Long functions

```sql
SELECT name, file_path, start_line,
       end_line - start_line + 1 AS lines,
       descendant_count AS ast_nodes
FROM codebase
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND end_line - start_line > 50
ORDER BY lines DESC
LIMIT 20;
```

`descendant_count` is a rough complexity proxy — more AST nodes means more logic.

## Nesting depth

```sql
SELECT name, max_depth, deep_nodes
FROM ast_nesting_analysis('codebase')
WHERE deep_nodes > 0
ORDER BY max_depth DESC;
```

Or manually find deeply nested control flow:

```sql
SELECT file_path, start_line, depth, type, peek
FROM codebase
WHERE depth > 10
  AND semantic_type IN (
      'FLOW_CONDITIONAL', 'FLOW_LOOP',
      'ERROR_TRY', 'ERROR_CATCH'
  )
ORDER BY depth DESC
LIMIT 20;
```

## Large classes

```sql
SELECT name, file_path,
       descendant_count AS complexity,
       end_line - start_line + 1 AS lines
FROM codebase
WHERE semantic_type = 'DEFINITION_CLASS'
  AND descendant_count > 500
ORDER BY complexity DESC;
```

## Functions with many parameters

```sql
SELECT name, file_path, start_line, parameters
FROM read_ast('src/**/*.py', context := 'native')
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND len(parameters) > 5
ORDER BY len(parameters) DESC;
```

## File complexity ranking

```sql
SELECT
    file_path,
    language,
    COUNT(*) AS total_nodes,
    MAX(depth) AS max_depth,
    COUNT(CASE WHEN semantic_type = 'DEFINITION_FUNCTION' THEN 1 END) AS functions
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY file_path, language
ORDER BY total_nodes DESC
LIMIT 20;
```

## Function size distribution

```sql
SELECT
    language,
    COUNT(*) AS total_functions,
    ROUND(100.0 * COUNT(CASE WHEN end_line - start_line <= 5 THEN 1 END) / COUNT(*), 1) AS tiny_pct,
    ROUND(100.0 * COUNT(CASE WHEN end_line - start_line BETWEEN 6 AND 20 THEN 1 END) / COUNT(*), 1) AS small_pct,
    ROUND(100.0 * COUNT(CASE WHEN end_line - start_line BETWEEN 21 AND 50 THEN 1 END) / COUNT(*), 1) AS medium_pct,
    ROUND(100.0 * COUNT(CASE WHEN end_line - start_line > 50 THEN 1 END) / COUNT(*), 1) AS large_pct
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND name IS NOT NULL
GROUP BY language
ORDER BY large_pct DESC;
```

## Cross-language comparison

```sql
SELECT
    language,
    COUNT(*) AS function_count,
    ROUND(AVG(descendant_count), 1) AS avg_complexity,
    ROUND(AVG(end_line - start_line + 1), 1) AS avg_lines
FROM read_ast(['src/**/*.py', 'src/**/*.js', 'src/**/*.go'], ignore_errors := true)
WHERE semantic_type = 'DEFINITION_FUNCTION'
GROUP BY language
ORDER BY avg_complexity DESC;
```

## See also

- [Analysis Macros](../reference/analysis-macros.md) — `ast_function_metrics`, `ast_nesting_analysis` reference
- [Semantic Types](../reference/semantic-types.md) — control flow type categories
- [Common Queries](common-queries.md) — more analysis patterns
