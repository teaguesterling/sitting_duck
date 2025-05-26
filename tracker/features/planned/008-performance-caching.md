# Feature: Session-Level AST Caching

**Status:** Planned
**Priority:** Medium
**Target:** v0.4.0

## Description
Provide helper macros for caching parsed AST data to improve performance when making multiple queries on the same file.

## Problem
AI agents and users often make multiple queries against the same large file, re-parsing each time.

## Proposed Solution
```sql
-- Cache a file's AST
SELECT ast_cache_file('large_file.py', 'python', 'my_cache');

-- Use cached data
SELECT ast(nodes).get_type('function_definition') FROM my_cache;
SELECT ast(nodes).get_names() FROM my_cache;
```

## Implementation
```sql
CREATE OR REPLACE MACRO ast_cache_file(file_path, language, cache_name := 'ast_cache') AS (
    'CREATE OR REPLACE TEMPORARY TABLE ' || cache_name || ' AS ' ||
    'SELECT * FROM read_ast_objects(''' || file_path || ''', ''' || language || ''')'
);

-- Optional: Clear cache
CREATE OR REPLACE MACRO ast_clear_cache(cache_name := 'ast_cache') AS (
    'DROP TABLE IF EXISTS ' || cache_name
);
```

## Benefits
- Significant performance improvement for interactive use
- Reduces parsing overhead
- Especially useful for large files