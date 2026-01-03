# Parquet-Based AST Indexing

The DuckDB AST extension now supports high-performance parquet-based indexing for large-scale code analysis. This feature enables sub-second queries on millions of AST nodes.

## Overview

Parquet indexes provide:
- **25M nodes → 40MB file** with ZSTD compression level 22
- **Sub-second queries** on large codebases
- **Selective regeneration** for incremental updates
- **Cross-file analysis** without re-parsing

## Creating an Index

### Basic Index Creation

```sql
-- Create index for C++ files
COPY (
    SELECT * FROM read_ast('src/**/*.cpp', peek_mode := 'none')
) TO '.index-cpp.parquet' (FORMAT PARQUET, CODEC 'ZSTD', COMPRESSION_LEVEL 22);

-- Create index for Python files
COPY (
    SELECT * FROM read_ast('**/*.py', peek_mode := 'none')
) TO '.index-py.parquet' (FORMAT PARQUET, CODEC 'ZSTD', COMPRESSION_LEVEL 22);
```

### Using the CLI

```bash
# Create an index
./ast-nav-indexed index 'src/**/*.cpp' cpp

# List all indexes
./ast-nav-indexed list-indexes

# Show index statistics
./ast-nav-indexed stats .index-cpp.parquet
```

## Key Operations

### 1. Get Function Summary for a File

```sql
-- Direct query using macro
SELECT * FROM ast_file_functions('src/main.cpp', '.index-cpp.parquet');

-- Two-step process for more control
-- Step 1: File overview
SELECT 
    COUNT(*) as total_nodes,
    COUNT(DISTINCT type) as unique_node_types,
    COUNT(*) FILTER (WHERE type = 'function_declarator') as functions
FROM read_parquet('.index-cpp.parquet')
WHERE file_path = 'src/main.cpp';

-- Step 2: Function details
SELECT 
    function_name,
    start_line,
    end_line,
    line_count,
    complexity
FROM ast_file_functions('src/main.cpp', '.index-cpp.parquet');
```

### 2. Find and Analyze Functions

```sql
-- Find function with detailed analysis
SELECT * FROM ast_find_function_detail('ParseToASTResult', '.index-cpp.parquet');

-- Returns:
-- - file_path, function_name, definition_type
-- - start_line, end_line, line_count
-- - complexity, param_count, local_vars, calls_made
-- - complexity_per_line ratio
```

### 3. Extract Source Code

```sql
-- Get function source location
SELECT * FROM ast_get_function_source('main', NULL, '.index-cpp.parquet');

-- Extract specific lines
SELECT ast_get_file_source('src/main.cpp', 100, 150);
```

## Selective Index Updates

### Check for Updates

```sql
-- See which files are new or modified
SELECT * FROM ast_update_index('src/**/*.cpp', 'cpp');
```

### Generate Update SQL

```sql
-- Get SQL to merge existing index with new files
SELECT ast_generate_update_sql('src/**/*.cpp', 'cpp');
```

### Manual Update Process

```sql
-- Create temporary index with merged data
COPY (
    -- Keep existing data for unchanged files
    SELECT * FROM read_parquet('.index-cpp.parquet')
    WHERE file_path NOT IN (
        SELECT DISTINCT file_path 
        FROM read_ast('src/modified/*.cpp', peek_mode := 'none')
    )
    UNION ALL
    -- Add new/modified files
    SELECT * FROM read_ast('src/modified/*.cpp', peek_mode := 'none')
) TO '.index-cpp.parquet.tmp' (FORMAT PARQUET, CODEC 'ZSTD', COMPRESSION_LEVEL 22);

-- Then rename: mv .index-cpp.parquet.tmp .index-cpp.parquet
```

## Advanced Queries

### Find Complex Functions

```sql
-- Functions with complexity > 100
SELECT * FROM ast_index_complex_functions('.index-cpp.parquet', 100);
```

### Cross-File Search

```sql
-- Quick search across all indexes
SELECT * FROM ast_quick_find('parse', 'function');

-- Find all calls to a function
WITH calls AS (
    SELECT 
        file_path,
        start_line,
        name
    FROM read_parquet('.index-cpp.parquet')
    WHERE type = 'call_expression'
      AND name = 'ParseToASTResult'
)
SELECT 
    file_path,
    COUNT(*) as call_count,
    LIST(start_line ORDER BY start_line) as lines
FROM calls
GROUP BY file_path;
```

### Code Metrics

```sql
-- File-level complexity metrics
WITH file_metrics AS (
    SELECT 
        file_path,
        COUNT(*) as total_nodes,
        COUNT(*) FILTER (WHERE type = 'function_declarator') as functions,
        COUNT(*) FILTER (WHERE type = 'if_statement') as conditionals,
        COUNT(*) FILTER (WHERE type = 'for_statement' OR type = 'while_statement') as loops,
        MAX(depth) as max_nesting
    FROM read_parquet('.index-cpp.parquet')
    GROUP BY file_path
)
SELECT 
    file_path,
    functions,
    conditionals + loops as cyclomatic_base,
    max_nesting,
    ROUND(total_nodes::DOUBLE / functions, 2) as avg_nodes_per_function
FROM file_metrics
WHERE functions > 0
ORDER BY cyclomatic_base DESC;
```

## Performance Tips

1. **Use `peek_mode := 'none'`** when creating indexes for maximum speed
2. **ZSTD level 22** provides optimal compression (60-80% size reduction)
3. **Partition large codebases** into multiple indexes by language/directory
4. **Use glob patterns** to query specific index files: `.index-*.parquet`
5. **Leverage DuckDB's parallel processing** for complex analytical queries

## Index File Management

```bash
# List index files with sizes
ls -lh .index-*.parquet

# Backup an index
cp .index-cpp.parquet .index-cpp.parquet.backup

# Remove old indexes
rm .index-*.parquet.tmp
```

## Integration with ast-nav

The `ast-nav-indexed` tool provides a streamlined CLI for all indexing operations:

```bash
# Create index
./ast-nav-indexed index 'src/**/*.cpp' cpp

# Query functions in a file
./ast-nav-indexed file-functions 'src/main.cpp'

# Find function details
./ast-nav-indexed find-function 'ParseToASTResult'

# Extract function source
./ast-nav-indexed function-source 'main'

# Search across indexes
./ast-nav-indexed search 'parse' function
```

## Troubleshooting

### Index Creation Fails
- Check file permissions
- Ensure sufficient disk space (estimate: 2-3 bytes per AST node)
- Verify glob patterns match files

### Slow Queries
- Ensure you're using the index, not re-parsing files
- Check if index needs updating (stale data)
- Consider splitting very large indexes

### Memory Issues
- Use streaming mode (`read_ast`) for initial creation
- Process large updates in batches
- Adjust DuckDB memory limits if needed