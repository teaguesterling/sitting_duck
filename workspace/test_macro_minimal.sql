LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO test_file_metrics(file_path) AS TABLE
    WITH base_data AS (
        SELECT * FROM read_ast(file_path)
    ),
    node_stats AS (
        SELECT 
            COUNT(*) as total_nodes,
            MAX(depth) as max_depth,
            AVG(depth) as avg_depth,
            COUNT(DISTINCT type) as unique_node_types
        FROM base_data
    )
    SELECT 
        file_path,
        total_nodes,
        max_depth,
        ROUND(avg_depth, 2) as avg_depth,
        unique_node_types
    FROM node_stats;

FROM test_file_metrics('src/unified_ast_backend.cpp');