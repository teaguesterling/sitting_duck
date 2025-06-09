-- Test if we can work around the threading issue with SQL
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Instead of relying on shared iteration state, 
-- let's try to get all data at once and use SQL for row numbering
WITH all_nodes AS (
    SELECT 
        *,
        ROW_NUMBER() OVER (PARTITION BY file_path ORDER BY node_id) - 1 as corrected_node_id
    FROM read_ast(['src/unified_ast_backend.cpp', 'src/ast_type.cpp'])
)
SELECT 
    file_path,
    COUNT(*) as node_count,
    MIN(corrected_node_id) as min_id,
    MAX(corrected_node_id) as max_id
FROM all_nodes
GROUP BY file_path;