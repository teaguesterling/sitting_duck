-- Analyze current semantic type distribution to guide type_to_int mapping design
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- See what semantic_type values we actually get
CREATE OR REPLACE MACRO analyze_semantic_distribution(file_path) AS TABLE
    SELECT 
        semantic_type,
        (semantic_type & 3840) >> 8 as kind_bits,
        (semantic_type & 240) >> 4 as super_type_bits,
        (semantic_type & 15) as language_bits,
        COUNT(*) as count,
        STRING_AGG(DISTINCT type, ', ') as example_types
    FROM read_ast(file_path)
    WHERE semantic_type > 0
    GROUP BY semantic_type
    ORDER BY count DESC
    LIMIT 20;

-- Test what our current bit filtering actually captures
CREATE OR REPLACE MACRO test_current_filters(file_path) AS TABLE
    WITH filter_tests AS (
        SELECT 
            type,
            semantic_type,
            CASE WHEN (semantic_type & 3840) = 512 THEN 'DECLARATION' ELSE 'OTHER' END as kind_filter,
            CASE 
                WHEN (semantic_type & 240) = 48 THEN 'FUNCTION'
                WHEN (semantic_type & 240) = 64 THEN 'METHOD' 
                WHEN (semantic_type & 240) = 16 THEN 'CLASS'
                ELSE 'OTHER'
            END as super_type_filter,
            COUNT(*) as count
        FROM read_ast(file_path)
        WHERE semantic_type > 0
        GROUP BY type, semantic_type, kind_filter, super_type_filter
    )
    SELECT 
        kind_filter,
        super_type_filter, 
        COUNT(DISTINCT type) as distinct_types,
        SUM(count) as total_nodes,
        STRING_AGG(DISTINCT type, ', ') as example_types
    FROM filter_tests
    GROUP BY kind_filter, super_type_filter
    ORDER BY total_nodes DESC;