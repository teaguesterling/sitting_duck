-- Minimal reproducer for segfault in macro
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test 1: Basic read_ast works
SELECT COUNT(*) as total_nodes FROM read_ast('src/unified_ast_backend.cpp');

-- Test 2: Semantic type filtering works
SELECT COUNT(*) as declarations 
FROM read_ast('src/unified_ast_backend.cpp') 
WHERE (semantic_type & 3840) = 512;

-- Test 3: Multiple aggregations in CTE (potential issue)
WITH base_data AS (
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
),
node_stats AS (
    SELECT 
        COUNT(*) as total_nodes,
        MAX(depth) as max_depth
    FROM base_data
)
SELECT * FROM node_stats;

-- Test 4: String aggregation (potential issue)
WITH type_sample AS (
    SELECT STRING_AGG(type, ', ' ORDER BY type) as example_types
    FROM (
        SELECT DISTINCT type
        FROM read_ast('src/unified_ast_backend.cpp')
        ORDER BY type
        LIMIT 5
    ) t
)
SELECT * FROM type_sample;

-- Test 5: Filter + Count combination (potential issue)
WITH semantic_stats AS (
    SELECT 
        COUNT(*) FILTER (WHERE (semantic_type & 3840) = 512) as declarations,
        COUNT(*) FILTER (WHERE (semantic_type & 3840) = 768) as statements
    FROM read_ast('src/unified_ast_backend.cpp')
)
SELECT * FROM semantic_stats;