LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO test_semantic_filter(file_path) AS TABLE
    WITH semantic_stats AS (
        SELECT 
            COUNT(*) FILTER (WHERE (semantic_type & 3840) = 512) as declarations,
            COUNT(*) FILTER (WHERE (semantic_type & 3840) = 768) as statements,
            COUNT(*) FILTER (WHERE (semantic_type & 3840) = 1024) as expressions,
            COUNT(*) FILTER (WHERE (semantic_type & 240) = 48) as functions,
            COUNT(*) FILTER (WHERE (semantic_type & 240) = 16) as classes
        FROM read_ast(file_path)
    )
    SELECT 
        file_path,
        declarations,
        statements,
        expressions,
        functions,
        classes
    FROM semantic_stats;

FROM test_semantic_filter('src/unified_ast_backend.cpp');