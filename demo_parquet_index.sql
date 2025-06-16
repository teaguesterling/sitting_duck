-- Demo: Parquet-based AST Indexing
-- This demonstrates the three key operations requested:
-- 1) Get a summary of all functions in a file
-- 2) Find a function and get detailed analysis
-- 3) Get the actual definition of a file

-- Load the extension and macros
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';
.read ast-nav-parquet.sql

-- =============================================================================
-- STEP 1: Create a parquet index
-- =============================================================================

-- Create an index for C++ files with optimal compression
-- This uses peek_mode := 'none' for fastest performance
COPY (
    SELECT * FROM read_ast('src/**/*.cpp', peek_mode := 'none')
) TO '.index-cpp.parquet' (FORMAT PARQUET, CODEC 'ZSTD', COMPRESSION_LEVEL 22);

-- Verify the index was created
SELECT * FROM ast_index_stats('.index-cpp.parquet');

-- =============================================================================
-- OPERATION 1: Get a summary of all functions in a file
-- =============================================================================

-- Method 1: Direct query on the index
SELECT 
    function_name,
    type,
    start_line,
    end_line,
    line_count,
    complexity
FROM ast_file_functions('src/unified_ast_backend.cpp', '.index-cpp.parquet')
ORDER BY start_line;

-- Method 2: Two-step process - first get file info, then functions
-- Step 1: Get file summary
WITH file_info AS (
    SELECT 
        file_path,
        COUNT(*) as total_nodes,
        COUNT(*) FILTER (WHERE type = 'function_declarator') as function_count
    FROM read_parquet('.index-cpp.parquet')
    WHERE file_path = 'src/unified_ast_backend.cpp'
    GROUP BY file_path
)
SELECT * FROM file_info;

-- Step 2: Get function details
SELECT 
    node_id,
    start_line,
    end_line,
    descendant_count as complexity
FROM read_parquet('.index-cpp.parquet')
WHERE file_path = 'src/unified_ast_backend.cpp'
  AND type = 'function_declarator'
  AND semantic_type = 112;

-- =============================================================================
-- OPERATION 2: Find a function and get detailed analysis
-- =============================================================================

-- Find the ParseToASTResult function with detailed metrics
SELECT * FROM ast_find_function_detail('ParseToASTResult', '.index-cpp.parquet');

-- Get even more detailed analysis by examining the function's subtree
WITH function_info AS (
    SELECT 
        file_path,
        node_id as func_start,
        node_id + descendant_count as func_end,
        descendant_count as total_nodes
    FROM read_parquet('.index-cpp.parquet')
    WHERE type = 'function_declarator'
      AND semantic_type = 112
      AND EXISTS (
          SELECT 1 FROM read_parquet('.index-cpp.parquet') n
          WHERE n.parent_id = node_id
            AND n.file_path = file_path
            AND n.name = 'ParseToASTResult'
      )
    LIMIT 1
)
SELECT 
    fi.file_path,
    'ParseToASTResult' as function_name,
    COUNT(*) FILTER (WHERE n.type = 'if_statement') as if_statements,
    COUNT(*) FILTER (WHERE n.type = 'for_statement') as for_loops,
    COUNT(*) FILTER (WHERE n.type = 'while_statement') as while_loops,
    COUNT(*) FILTER (WHERE n.type = 'call_expression') as function_calls,
    COUNT(*) FILTER (WHERE n.type = 'return_statement') as return_statements,
    COUNT(*) FILTER (WHERE n.type = 'comment') as comments,
    fi.total_nodes
FROM function_info fi
JOIN read_parquet('.index-cpp.parquet') n 
    ON n.file_path = fi.file_path
    AND n.node_id >= fi.func_start
    AND n.node_id <= fi.func_end
GROUP BY fi.file_path, fi.total_nodes;

-- =============================================================================
-- OPERATION 3: Get the actual definition/source of a file
-- =============================================================================

-- Get source location for a function
SELECT * FROM ast_get_function_source('ParseToASTResult', NULL, '.index-cpp.parquet');

-- Get boundaries for extracting a specific section
WITH section_bounds AS (
    SELECT 
        MIN(start_line) as first_line,
        MAX(end_line) as last_line
    FROM read_parquet('.index-cpp.parquet')
    WHERE file_path = 'src/unified_ast_backend.cpp'
      AND type IN ('function_definition', 'function_declarator')
      AND start_line BETWEEN 100 AND 200
)
SELECT 
    'src/unified_ast_backend.cpp' as file_path,
    first_line,
    last_line,
    'sed -n "' || first_line || ',' || last_line || 'p" src/unified_ast_backend.cpp' as extract_command
FROM section_bounds;

-- =============================================================================
-- BONUS: Selective index regeneration
-- =============================================================================

-- Check which files need updating (new or modified)
SELECT * FROM ast_update_index('src/**/*.cpp', 'cpp');

-- Generate the SQL to merge old and new data
SELECT ast_generate_update_sql('src/**/*.cpp', 'cpp');

-- The generated SQL will:
-- 1. Keep all nodes from files NOT in the new pattern
-- 2. Add all nodes from files matching the pattern
-- 3. This effectively updates changed files and adds new ones

-- =============================================================================
-- PERFORMANCE COMPARISON
-- =============================================================================

-- Compare index query vs direct file parsing
.timer on

-- Using index (fast)
SELECT COUNT(*) FROM read_parquet('.index-cpp.parquet')
WHERE type = 'function_declarator';

-- Direct parsing (slower, especially for large codebases)
-- SELECT COUNT(*) FROM read_ast('src/**/*.cpp', peek_mode := 'none')
-- WHERE type = 'function_declarator';

.timer off

-- =============================================================================
-- ADVANCED: Cross-file analysis using index
-- =============================================================================

-- Find all calls to a specific function across the codebase
WITH target_function AS (
    SELECT 'ParseToASTResult' as fname
),
call_sites AS (
    SELECT 
        i.file_path,
        i.start_line,
        i.name as called_function
    FROM read_parquet('.index-cpp.parquet') i
    CROSS JOIN target_function tf
    WHERE i.type = 'call_expression'
      AND i.name = tf.fname
)
SELECT 
    file_path,
    COUNT(*) as call_count,
    LIST(DISTINCT start_line ORDER BY start_line) as line_numbers
FROM call_sites
GROUP BY file_path
ORDER BY call_count DESC;