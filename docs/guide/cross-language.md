# Cross-Language Analysis

Compare and analyze code across multiple programming languages.

## Why Cross-Language Analysis?

Sitting Duck's semantic type system enables queries that work identically across all 27 supported languages. Instead of learning language-specific AST types, use universal semantic types.

## Basic Cross-Language Queries

### Count Functions Across Languages

```sql
SELECT
    language,
    COUNT(*) as function_count
FROM read_ast([
    '**/*.py',
    '**/*.js',
    '**/*.java',
    '**/*.go',
    '**/*.rs'
], ignore_errors := true)
WHERE semantic_type = 240  -- DEFINITION_FUNCTION
GROUP BY language
ORDER BY function_count DESC;
```

### Find All Classes

```sql
SELECT
    name,
    language,
    file_path,
    start_line
FROM read_ast([
    '**/*.py',
    '**/*.java',
    '**/*.cpp',
    '**/*.cs'
], ignore_errors := true)
WHERE semantic_type = 248  -- DEFINITION_CLASS
  AND name IS NOT NULL
ORDER BY language, file_path;
```

## Comparing Patterns

### Complexity by Language

```sql
SELECT
    language,
    ROUND(AVG(descendant_count), 2) as avg_complexity,
    MAX(descendant_count) as max_complexity,
    COUNT(*) as function_count
FROM read_ast([
    '**/*.py',
    '**/*.js',
    '**/*.java'
], ignore_errors := true)
WHERE semantic_type = 240  -- Functions
GROUP BY language
ORDER BY avg_complexity DESC;
```

### Import Patterns

```sql
SELECT
    language,
    COUNT(*) as import_count,
    COUNT(DISTINCT file_path) as files_with_imports
FROM read_ast([
    '**/*.py',
    '**/*.js',
    '**/*.java',
    '**/*.go'
], ignore_errors := true)
WHERE semantic_type = 48  -- EXTERNAL_IMPORT
GROUP BY language;
```

### Control Flow Density

```sql
SELECT
    language,
    COUNT(CASE WHEN semantic_type = 144 THEN 1 END) as conditionals,
    COUNT(CASE WHEN semantic_type = 148 THEN 1 END) as loops,
    COUNT(CASE WHEN semantic_type = 152 THEN 1 END) as jumps,
    COUNT(*) as total_nodes
FROM read_ast([
    '**/*.py',
    '**/*.js',
    '**/*.java'
], ignore_errors := true)
GROUP BY language;
```

## Language-Specific vs Universal

### Using Language-Specific Types

When you need precision for a single language:

```sql
-- Python-specific: find decorators
SELECT name, peek, start_line
FROM read_ast('**/*.py')
WHERE type = 'decorator';

-- Java-specific: find annotations
SELECT name, peek, start_line
FROM read_ast('**/*.java')
WHERE type = 'annotation';
```

### Using Universal Semantic Types

When you want cross-language results:

```sql
-- Both decorators and annotations
SELECT
    name,
    language,
    peek
FROM read_ast([
    '**/*.py',
    '**/*.java'
], ignore_errors := true)
WHERE semantic_type = 36;  -- METADATA_ANNOTATION
```

## Codebase Overview

### Language Distribution

```sql
SELECT
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as nodes,
    ROUND(COUNT(*) * 100.0 / SUM(COUNT(*)) OVER(), 1) as percentage
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY nodes DESC;
```

### Definition Inventory

```sql
SELECT
    language,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as classes,
    COUNT(CASE WHEN semantic_type = 244 THEN 1 END) as variables
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
HAVING COUNT(*) > 100
ORDER BY functions DESC;
```

## Finding Similar Code

### Functions with Similar Names

```sql
SELECT
    name,
    language,
    file_path,
    start_line
FROM read_ast([
    '**/*.py',
    '**/*.js',
    '**/*.java'
], ignore_errors := true)
WHERE semantic_type = 240
  AND name LIKE '%validate%'
ORDER BY name, language;
```

### Classes with Common Patterns

```sql
SELECT
    name,
    language,
    descendant_count as complexity
FROM read_ast([
    '**/*.py',
    '**/*.java',
    '**/*.cpp'
], ignore_errors := true)
WHERE semantic_type = 248
  AND name LIKE '%Service%'
ORDER BY complexity DESC;
```

## Complexity Hotspots

### Most Complex Functions

```sql
SELECT
    name,
    language,
    file_path,
    descendant_count as complexity,
    end_line - start_line + 1 as lines
FROM read_ast([
    '**/*.py',
    '**/*.js',
    '**/*.java'
], ignore_errors := true)
WHERE semantic_type = 240
  AND descendant_count > 100
ORDER BY complexity DESC
LIMIT 20;
```

### Deeply Nested Code

```sql
SELECT
    file_path,
    language,
    MAX(depth) as max_depth,
    COUNT(CASE WHEN depth > 10 THEN 1 END) as deep_nodes
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY file_path, language
HAVING MAX(depth) > 10
ORDER BY max_depth DESC;
```

## Exporting Results

### To Parquet

```sql
COPY (
    SELECT
        file_path,
        language,
        type,
        name,
        semantic_type_to_string(semantic_type) as semantic_type,
        start_line,
        end_line,
        descendant_count
    FROM read_ast('**/*.*', ignore_errors := true)
    WHERE is_definition(semantic_type)
) TO 'codebase_definitions.parquet';
```

### To CSV

```sql
COPY (
    SELECT language, COUNT(*) as nodes
    FROM read_ast('**/*.*', ignore_errors := true)
    GROUP BY language
) TO 'language_stats.csv' (HEADER);
```

## Best Practices

1. **Use `ignore_errors := true`** for large cross-language scans
2. **Be specific with patterns** when possible for better performance
3. **Use semantic types** for cross-language queries
4. **Use language-specific types** when you need precision
5. **Cache results** in tables for repeated analysis

```sql
-- Create a reusable table
CREATE TABLE codebase_ast AS
SELECT * FROM read_ast('**/*.*', ignore_errors := true);

-- Query the cached data
SELECT language, COUNT(*) FROM codebase_ast GROUP BY language;
```

## Next Steps

- [Semantic Types](semantic-types.md) - Complete type reference
- [API Reference](../api/core-functions.md) - Function documentation
- [Languages Overview](../languages/overview.md) - Language-specific details
