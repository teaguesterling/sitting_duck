LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO test_complete_metrics(file_path) AS TABLE
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
    ),
    type_sample AS (
        SELECT STRING_AGG(type, ', ' ORDER BY type) as example_types
        FROM (
            SELECT DISTINCT type
            FROM base_data
            ORDER BY type
            LIMIT 5
        ) t
    ),
    semantic_stats AS (
        SELECT 
            COUNT(*) FILTER (WHERE (semantic_type & 3840) = 512) as declarations,
            COUNT(*) FILTER (WHERE (semantic_type & 3840) = 768) as statements,
            COUNT(*) FILTER (WHERE (semantic_type & 3840) = 1024) as expressions,
            COUNT(*) FILTER (WHERE (semantic_type & 240) = 48) as functions,
            COUNT(*) FILTER (WHERE (semantic_type & 240) = 16) as classes
        FROM base_data
    )
    SELECT 
        file_path,
        total_nodes,
        max_depth,
        ROUND(avg_depth, 2) as avg_depth,
        unique_node_types,
        example_types,
        declarations,
        statements,
        expressions,
        functions,
        classes
    FROM node_stats, type_sample, semantic_stats;

FROM test_complete_metrics('src/unified_ast_backend.cpp');