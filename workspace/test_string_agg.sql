LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO test_string_agg(file_path) AS TABLE
    WITH type_sample AS (
        SELECT STRING_AGG(type, ', ' ORDER BY type) as example_types
        FROM (
            SELECT DISTINCT type
            FROM read_ast(file_path)
            ORDER BY type
            LIMIT 5
        ) t
    )
    SELECT 
        file_path,
        example_types
    FROM type_sample;

FROM test_string_agg('src/unified_ast_backend.cpp');