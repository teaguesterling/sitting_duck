# Quick Start

Get up and running with Sitting Duck in 5 minutes.

## Your First Query

After [installing](installation.md) Sitting Duck, try these queries:

```sql
-- Load the extension
LOAD sitting_duck;

-- Parse this README and count nodes
SELECT COUNT(*) as node_count FROM read_ast('README.md');
```

## Parse a Source File

```sql
-- See all AST nodes in a Python file
SELECT * FROM read_ast('example.py') LIMIT 20;

-- Find function definitions
SELECT name, start_line, end_line
FROM read_ast('example.py')
WHERE type = 'function_definition';
```

## Visualize Tree Structure

```sql
-- Show indented tree structure
SELECT
    repeat('  ', depth) || type as tree_view,
    name,
    start_line
FROM read_ast('example.py')
ORDER BY node_id
LIMIT 30;
```

## Analyze Multiple Files

```sql
-- Parse all Python files in a directory
SELECT file_path, COUNT(*) as nodes
FROM read_ast('src/**/*.py')
GROUP BY file_path
ORDER BY nodes DESC;

-- Parse files from multiple patterns
SELECT file_path, language, COUNT(*) as nodes
FROM read_ast([
    'src/**/*.py',
    'lib/**/*.js',
    'main.cpp'
], ignore_errors := true)
GROUP BY file_path, language;
```

## Cross-Language Analysis

```sql
-- Compare function counts across languages
SELECT
    language,
    COUNT(*) as function_count
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
WHERE semantic_type = 240  -- DEFINITION_FUNCTION
GROUP BY language
ORDER BY function_count DESC;
```

## Find Complex Functions

```sql
-- Functions with high complexity (many AST nodes)
SELECT
    name,
    file_path,
    descendant_count as complexity,
    start_line
FROM read_ast('**/*.py', ignore_errors := true)
WHERE type = 'function_definition'
  AND descendant_count > 50
ORDER BY complexity DESC
LIMIT 10;
```

## Common Patterns

### Find All Classes

```sql
SELECT name, file_path, start_line
FROM read_ast('**/*.py')
WHERE type = 'class_definition'
ORDER BY file_path, start_line;
```

### Count Imports

```sql
SELECT
    file_path,
    COUNT(*) as import_count
FROM read_ast('**/*.py')
WHERE type IN ('import_statement', 'import_from_statement')
GROUP BY file_path
ORDER BY import_count DESC;
```

### Find Comments

```sql
SELECT
    file_path,
    start_line,
    peek as comment_text
FROM read_ast('**/*.py')
WHERE type = 'comment'
ORDER BY file_path, start_line;
```

## Next Steps

- [Basic Usage](basic-usage.md) - Detailed usage patterns
- [Parsing Files](../guide/parsing-files.md) - File and glob processing
- [Semantic Types](../guide/semantic-types.md) - Cross-language analysis
