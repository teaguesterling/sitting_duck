-- Final polished function extractor for unified_ast_backend.cpp and similar C++ files
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO extract_functions_complete(file_path) AS TABLE
    WITH all_function_nodes AS (
        SELECT 
            REGEXP_EXTRACT(peek, '::([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1) as function_name,
            REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)::', 1) as class_name,
            REGEXP_EXTRACT(peek, '^([^:]+)', 1) as return_type,
            type as node_type,
            start_line,
            end_line,
            end_line - start_line + 1 as line_count,
            descendant_count,
            semantic_type,
            peek
        FROM read_ast(file_path)
        WHERE type IN ('function_definition', 'function_declarator')
          AND peek LIKE '%::%'  -- Focus on class methods
    ),
    -- Consolidate definition and declarator pairs
    function_summary AS (
        SELECT 
            function_name,
            class_name,
            TRIM(return_type) as return_type,
            MIN(start_line) as start_line,
            MAX(end_line) as end_line,
            MAX(line_count) as line_count,
            MAX(descendant_count) as complexity,
            STRING_AGG(DISTINCT node_type, ', ') as node_types,
            -- Get the definition's peek (longer and more informative)
            (SELECT peek FROM all_function_nodes af2 
             WHERE af2.function_name = af1.function_name 
               AND af2.class_name = af1.class_name 
               AND af2.node_type = 'function_definition' 
             LIMIT 1) as full_signature
        FROM all_function_nodes af1
        WHERE function_name IS NOT NULL
        GROUP BY function_name, class_name, return_type
    )
    SELECT 
        function_name,
        class_name,
        return_type,
        start_line,
        end_line,
        line_count,
        complexity,
        node_types,
        LEFT(full_signature, 150) as signature_preview
    FROM function_summary
    ORDER BY start_line;

-- Show analysis of UnifiedASTBackend class functions
FROM extract_functions_complete('src/unified_ast_backend.cpp');