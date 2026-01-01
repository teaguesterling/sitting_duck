# Cookbook: Practical Recipes

Real-world SQL recipes for common code analysis tasks.

## Quick Reference

| Task | Key Query Pattern |
|------|-------------------|
| Find all functions | `WHERE is_function_definition(semantic_type)` |
| Find all classes | `WHERE is_class_definition(semantic_type)` |
| Find function calls | `WHERE is_call(semantic_type)` |
| Find imports | `WHERE is_import(semantic_type)` |
| Find control flow | `WHERE is_control_flow(semantic_type)` |
| Find by name | `WHERE name LIKE '%pattern%'` |

---

## Codebase Overview

### Language Distribution

```sql
SELECT
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as nodes
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY files DESC;
```

### Definition Inventory

```sql
SELECT
    language,
    SUM(CASE WHEN is_function_definition(semantic_type) THEN 1 ELSE 0 END) as functions,
    SUM(CASE WHEN is_class_definition(semantic_type) THEN 1 ELSE 0 END) as classes,
    SUM(CASE WHEN is_import(semantic_type) THEN 1 ELSE 0 END) as imports
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY functions DESC;
```

### File Complexity Ranking

```sql
SELECT
    file_path,
    language,
    COUNT(*) as total_nodes,
    MAX(depth) as max_depth,
    COUNT(CASE WHEN is_function_definition(semantic_type) THEN 1 END) as functions
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY file_path, language
ORDER BY total_nodes DESC
LIMIT 20;
```

---

## Finding Functions

### All Function Definitions

```sql
SELECT name, file_path, start_line, end_line
FROM read_ast('src/**/*.py')
WHERE is_function_definition(semantic_type)
  AND name IS NOT NULL
ORDER BY file_path, start_line;
```

### Functions by Name Pattern

```sql
-- Find all validation functions
SELECT name, file_path, start_line
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND name LIKE '%valid%'
ORDER BY name;
```

### Long Functions (Potential Refactoring Candidates)

```sql
SELECT
    name,
    file_path,
    start_line,
    end_line - start_line + 1 as lines,
    descendant_count as complexity
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND end_line - start_line > 50
ORDER BY lines DESC
LIMIT 20;
```

### Most Complex Functions

```sql
SELECT
    name,
    file_path,
    start_line,
    descendant_count as complexity,
    end_line - start_line + 1 as lines
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND descendant_count > 100
ORDER BY complexity DESC
LIMIT 20;
```

### Functions with Many Parameters

```sql
SELECT
    name,
    file_path,
    start_line,
    parameters
FROM read_ast('src/**/*.py', context := 'native')
WHERE is_function_definition(semantic_type)
  AND len(parameters) > 5
ORDER BY len(parameters) DESC;
```

---

## Finding Classes and Types

### All Class Definitions

```sql
SELECT name, file_path, start_line
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_class_definition(semantic_type)
  AND name IS NOT NULL
ORDER BY name;
```

### Classes by Pattern (e.g., Services, Controllers)

```sql
SELECT name, file_path, language
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_class_definition(semantic_type)
  AND (name LIKE '%Service%'
       OR name LIKE '%Controller%'
       OR name LIKE '%Handler%')
ORDER BY name;
```

### Large Classes (God Objects)

```sql
SELECT
    name,
    file_path,
    descendant_count as complexity,
    end_line - start_line + 1 as lines
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_class_definition(semantic_type)
  AND descendant_count > 500
ORDER BY complexity DESC;
```

---

## Import Analysis

### All Imports

```sql
SELECT type, name, file_path, start_line
FROM read_ast('src/**/*.py')
WHERE is_import(semantic_type)
ORDER BY file_path, start_line;
```

### Most Common Imports

```sql
SELECT name, COUNT(*) as usage_count
FROM read_ast('src/**/*.py')
WHERE is_import(semantic_type)
  AND name IS NOT NULL
GROUP BY name
ORDER BY usage_count DESC
LIMIT 20;
```

### Files with Many Imports

```sql
SELECT
    file_path,
    COUNT(*) as import_count
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_import(semantic_type)
GROUP BY file_path
HAVING COUNT(*) > 20
ORDER BY import_count DESC;
```

### External vs Internal Imports (Python)

```sql
SELECT
    file_path,
    SUM(CASE WHEN peek LIKE 'from .%' OR peek LIKE 'import .%' THEN 1 ELSE 0 END) as relative_imports,
    SUM(CASE WHEN peek NOT LIKE 'from .%' AND peek NOT LIKE 'import .%' THEN 1 ELSE 0 END) as external_imports
FROM read_ast('src/**/*.py')
WHERE is_import(semantic_type)
GROUP BY file_path
ORDER BY external_imports DESC;
```

---

## Function Calls

### Find All Calls to a Function

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.py')
WHERE is_call(semantic_type)
  AND name = 'print'
ORDER BY file_path, start_line;
```

### Most Called Functions

```sql
SELECT name, COUNT(*) as call_count
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_call(semantic_type)
  AND name IS NOT NULL
GROUP BY name
ORDER BY call_count DESC
LIMIT 30;
```

### Find Deprecated Function Usage

```sql
SELECT file_path, start_line, name, peek
FROM read_ast('src/**/*.py')
WHERE is_call(semantic_type)
  AND name IN ('eval', 'exec', 'compile', '__import__')
ORDER BY name, file_path;
```

---

## Control Flow Analysis

### Control Flow Density by File

```sql
SELECT
    file_path,
    SUM(CASE WHEN is_conditional(semantic_type) THEN 1 ELSE 0 END) as conditionals,
    SUM(CASE WHEN is_loop(semantic_type) THEN 1 ELSE 0 END) as loops,
    SUM(CASE WHEN is_jump(semantic_type) THEN 1 ELSE 0 END) as jumps
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_control_flow(semantic_type)
GROUP BY file_path
ORDER BY conditionals + loops DESC
LIMIT 20;
```

### Deeply Nested Code

```sql
SELECT
    file_path,
    start_line,
    depth,
    type,
    peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE depth > 10
  AND is_control_flow(semantic_type)
ORDER BY depth DESC
LIMIT 20;
```

### Functions with High Cyclomatic Complexity Proxy

```sql
WITH function_complexity AS (
    SELECT
        f.name,
        f.file_path,
        f.start_line,
        f.end_line,
        f.node_id
    FROM read_ast('src/**/*.py') f
    WHERE is_function_definition(f.semantic_type)
)
SELECT
    fc.name,
    fc.file_path,
    fc.start_line,
    COUNT(*) as control_flow_count
FROM function_complexity fc
JOIN read_ast('src/**/*.py') cf
  ON cf.file_path = fc.file_path
  AND cf.start_line >= fc.start_line
  AND cf.end_line <= fc.end_line
  AND is_control_flow(cf.semantic_type)
GROUP BY fc.name, fc.file_path, fc.start_line
ORDER BY control_flow_count DESC
LIMIT 20;
```

---

## String and Literal Analysis

### Find Hardcoded Strings

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_string_literal(semantic_type)
  AND (peek LIKE '%password%'
       OR peek LIKE '%secret%'
       OR peek LIKE '%api_key%'
       OR peek LIKE '%token%')
ORDER BY file_path;
```

### Find URL Patterns

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_string_literal(semantic_type)
  AND (peek LIKE '%http://%'
       OR peek LIKE '%https://%'
       OR peek LIKE '%localhost%')
ORDER BY file_path;
```

### Find SQL Strings (Potential Injection Risk)

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_string_literal(semantic_type)
  AND (peek LIKE '%SELECT %'
       OR peek LIKE '%INSERT %'
       OR peek LIKE '%UPDATE %'
       OR peek LIKE '%DELETE %')
ORDER BY file_path;
```

---

## API Surface Extraction

### Public Functions (Python)

```sql
SELECT name, file_path, start_line
FROM read_ast('src/**/*.py')
WHERE is_function_definition(semantic_type)
  AND name IS NOT NULL
  AND name NOT LIKE '\_%'  -- Exclude private (underscore prefix)
ORDER BY file_path, name;
```

### Exported Functions (JavaScript/TypeScript)

```sql
SELECT name, file_path, start_line, peek
FROM read_ast('src/**/*.{js,ts}', ignore_errors := true)
WHERE is_export(semantic_type)
  OR (is_function_definition(semantic_type) AND peek LIKE 'export %')
ORDER BY file_path;
```

### Interface Definitions (TypeScript)

```sql
SELECT name, file_path, start_line
FROM read_ast('src/**/*.ts')
WHERE type = 'interface_declaration'
ORDER BY name;
```

### REST Endpoint Patterns

```sql
-- Find decorator-based routes (Python Flask/FastAPI)
SELECT
    a.peek as decorator,
    f.name as function_name,
    f.file_path,
    f.start_line
FROM read_ast('src/**/*.py') a
JOIN read_ast('src/**/*.py') f
  ON f.file_path = a.file_path
  AND f.start_line = a.start_line + 1
WHERE is_annotation(a.semantic_type)
  AND (a.peek LIKE '%@app.route%'
       OR a.peek LIKE '%@router.%'
       OR a.peek LIKE '%@get%'
       OR a.peek LIKE '%@post%')
  AND is_function_definition(f.semantic_type);
```

---

## Code Quality Checks

### Empty Functions

```sql
SELECT name, file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND descendant_count < 5
  AND name IS NOT NULL
ORDER BY file_path;
```

### TODO/FIXME Comments

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_comment(semantic_type)
  AND (peek LIKE '%TODO%'
       OR peek LIKE '%FIXME%'
       OR peek LIKE '%HACK%'
       OR peek LIKE '%XXX%')
ORDER BY file_path, start_line;
```

### Functions Without Docstrings (Python)

```sql
WITH functions AS (
    SELECT name, file_path, start_line, node_id
    FROM read_ast('src/**/*.py')
    WHERE is_function_definition(semantic_type)
      AND name IS NOT NULL
),
docstrings AS (
    SELECT file_path, start_line
    FROM read_ast('src/**/*.py')
    WHERE type = 'expression_statement'
      AND peek LIKE '"""%'
)
SELECT f.name, f.file_path, f.start_line
FROM functions f
LEFT JOIN docstrings d
  ON d.file_path = f.file_path
  AND d.start_line = f.start_line + 1
WHERE d.file_path IS NULL
ORDER BY f.file_path;
```

---

## Cross-Language Patterns

### Compare Function Counts

```sql
SELECT
    language,
    COUNT(*) as function_count,
    ROUND(AVG(descendant_count), 1) as avg_complexity
FROM read_ast([
    'src/**/*.py',
    'src/**/*.js',
    'src/**/*.java',
    'src/**/*.go'
], ignore_errors := true)
WHERE is_function_definition(semantic_type)
GROUP BY language
ORDER BY function_count DESC;
```

### Find Similar Named Functions Across Languages

```sql
SELECT name, language, file_path, start_line
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND name IN (
      SELECT name
      FROM read_ast('src/**/*.*', ignore_errors := true)
      WHERE is_function_definition(semantic_type)
        AND name IS NOT NULL
      GROUP BY name
      HAVING COUNT(DISTINCT language) > 1
  )
ORDER BY name, language;
```

---

## Caching and Performance

### Create a Cached AST Table

```sql
CREATE TABLE codebase_ast AS
SELECT * FROM read_ast('src/**/*.*', ignore_errors := true);

-- Now query the cached table
SELECT language, COUNT(*) FROM codebase_ast GROUP BY language;
```

### Export to Parquet for Later Analysis

```sql
COPY (
    SELECT
        file_path,
        language,
        type,
        name,
        semantic_type,
        semantic_type_to_string(semantic_type) as semantic_name,
        start_line,
        end_line,
        depth,
        descendant_count
    FROM read_ast('src/**/*.*', ignore_errors := true)
    WHERE is_definition(semantic_type)
       OR is_call(semantic_type)
       OR is_import(semantic_type)
) TO 'codebase_analysis.parquet';
```

### Incremental Analysis Pattern

```sql
-- Store last analysis timestamp
CREATE TABLE IF NOT EXISTS analysis_meta (
    last_run TIMESTAMP
);

-- Query only modified files (requires file metadata)
-- This is a pattern - actual implementation needs file mtime access
```

---

## Troubleshooting Queries

### Check What Types Exist

```sql
SELECT DISTINCT type, semantic_type_to_string(semantic_type) as semantic
FROM read_ast('example.py')
ORDER BY semantic, type;
```

### Debug Name Extraction

```sql
SELECT type, name, peek
FROM read_ast('example.py')
WHERE is_function_definition(semantic_type)
   OR is_class_definition(semantic_type);
```

### Check Parsing Errors

```sql
SELECT file_path, type, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE type = 'ERROR'
LIMIT 10;
```

---

## See Also

- [Cross-Language Analysis](cross-language.md) - More cross-language patterns
- [Semantic Types](semantic-types.md) - Full type reference
- [API Reference](../api/core-functions.md) - Function documentation
