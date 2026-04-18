# Multi-File Processing

Process multiple files efficiently with glob patterns and file arrays.

## File Arrays

### Basic Array Usage

Process specific files or multiple patterns in a single query:

```sql
-- Array of specific files
SELECT file_path, language, COUNT(*) as nodes
FROM read_ast([
    'src/main.py',
    'src/utils.py',
    'lib/helpers.js'
], ignore_errors := true)
GROUP BY file_path, language;

-- Array of glob patterns
SELECT file_path, COUNT(*) as nodes
FROM read_ast([
    'src/**/*.py',
    'lib/**/*.js',
    'include/**/*.hpp'
], ignore_errors := true)
GROUP BY file_path;
```

### Mixed Patterns and Files

```sql
-- Combine specific files with patterns
SELECT file_path, language
FROM read_ast([
    'main.py',           -- Specific file
    'src/**/*.py',       -- All Python in src/
    'config.json'        -- Another specific file
], ignore_errors := true)
GROUP BY file_path, language;
```

## Cross-Language Analysis

### Analyze Multiple Languages

```sql
-- Function counts across languages
SELECT
    language,
    COUNT(*) as function_count
FROM read_ast([
    '**/*.py',
    '**/*.js',
    '**/*.java',
    '**/*.go'
], ignore_errors := true)
WHERE semantic_type = 240  -- DEFINITION_FUNCTION
GROUP BY language
ORDER BY function_count DESC;
```

### Compare Patterns Across Languages

```sql
-- Import patterns by language
SELECT
    language,
    type,
    COUNT(*) as count
FROM read_ast([
    '**/*.py',
    '**/*.js',
    '**/*.java'
], ignore_errors := true)
WHERE semantic_type = 48  -- EXTERNAL_IMPORT
GROUP BY language, type
ORDER BY language, count DESC;
```

## Deduplication

File arrays automatically deduplicate:

```sql
-- Even if patterns overlap, each file appears once
SELECT COUNT(DISTINCT file_path) as unique_files
FROM read_ast([
    'src/**/*.py',        -- Matches src/main.py
    'src/main.py',        -- Same file
    '**/*.py'             -- Also matches src/main.py
], ignore_errors := true);
```

## Error Handling

### Ignore Missing Files

```sql
-- Continue processing even if some files don't exist
SELECT file_path, COUNT(*)
FROM read_ast([
    'existing_file.py',
    'missing_file.py',       -- Won't stop processing
    'another_existing.py'
], ignore_errors := true)
GROUP BY file_path;
```

### Validate Inputs

```sql
-- Empty array produces an error
SELECT * FROM read_ast([]);
-- Error: File pattern list cannot be empty

-- NULL values produce an error
SELECT * FROM read_ast(['file.py', NULL]);
-- Error: File pattern list cannot contain NULL values
```

## Performance Optimization

### Batch Processing

```sql
-- Use batch_size for large file sets
SELECT file_path, COUNT(*) as nodes
FROM read_ast([
    'src/**/*.py',
    'lib/**/*.py'
], batch_size := 10, ignore_errors := true)
GROUP BY file_path;
```

### Efficient Patterns

```sql
-- More efficient: specific patterns
SELECT * FROM read_ast([
    'src/**/*.py',
    'lib/**/*.js'
], ignore_errors := true);

-- Less efficient: overly broad patterns
SELECT * FROM read_ast('**/*.*', ignore_errors := true);
```

## File Statistics

### Per-File Node Counts

```sql
SELECT
    file_path,
    language,
    COUNT(*) as total_nodes,
    COUNT(CASE WHEN type LIKE '%function%' THEN 1 END) as functions,
    COUNT(CASE WHEN type LIKE '%class%' THEN 1 END) as classes
FROM read_ast(['**/*.py', '**/*.js'], ignore_errors := true)
GROUP BY file_path, language
ORDER BY total_nodes DESC;
```

### Language Distribution

```sql
SELECT
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as total_nodes,
    ROUND(COUNT(*) * 100.0 / SUM(COUNT(*)) OVER(), 2) as percentage
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;
```

## Combining with Aggregations

### Find Complex Files

```sql
SELECT
    file_path,
    language,
    COUNT(*) as nodes,
    MAX(depth) as max_depth,
    MAX(descendant_count) as max_complexity
FROM read_ast([
    'src/**/*.py',
    'src/**/*.js'
], ignore_errors := true)
GROUP BY file_path, language
HAVING MAX(descendant_count) > 100
ORDER BY max_complexity DESC;
```

### Cross-File Function Analysis

```sql
SELECT
    name,
    file_path,
    descendant_count as complexity,
    language
FROM read_ast(['**/*.py', '**/*.js'], ignore_errors := true)
WHERE semantic_type = 240  -- DEFINITION_FUNCTION
  AND name IS NOT NULL
ORDER BY complexity DESC
LIMIT 20;
```

## Working with Results

### Save to Table

```sql
-- Create a table from multi-file results
CREATE TABLE codebase_ast AS
SELECT *
FROM read_ast([
    'src/**/*.py',
    'lib/**/*.js',
    'include/**/*.hpp'
], ignore_errors := true);

-- Query the table
SELECT language, COUNT(*) FROM codebase_ast GROUP BY language;
```

### Export to Parquet

```sql
-- Export for later analysis
COPY (
    SELECT file_path, type, name, start_line, semantic_type
    FROM read_ast(['**/*.py', '**/*.js'], ignore_errors := true)
) TO 'codebase_analysis.parquet';
```

## Next Steps

- [Semantic Types](semantic-types.md) - Cross-language type system
- [Context Extraction](context-extraction.md) - Detailed semantic analysis
- [Cross-Language Analysis](cross-language.md) - Comparing across languages
