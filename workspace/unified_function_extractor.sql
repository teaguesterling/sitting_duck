-- Final unified function extractor that properly groups function definitions and declarators
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO extract_cpp_functions_unified(file_path) AS TABLE
    WITH function_nodes AS (
        SELECT 
            REGEXP_EXTRACT(peek, '::([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1) as function_name,
            REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)::', 1) as class_name,
            -- Clean up return type extraction
            CASE 
                WHEN type = 'function_definition' THEN
                    TRIM(REGEXP_EXTRACT(peek, '^([^\\n]+?)\\s+[A-Za-z_][A-Za-z0-9_]*::', 1))
                ELSE NULL
            END as return_type,
            type as node_type,
            start_line,
            end_line,
            end_line - start_line + 1 as line_count,
            descendant_count,
            semantic_type,
            LEFT(peek, 200) as signature_sample
        FROM read_ast(file_path)
        WHERE type IN ('function_definition', 'function_declarator')
          AND peek LIKE '%::%'
    ),
    -- Group by function and take the definition data where available
    unified_functions AS (
        SELECT 
            function_name,
            class_name,
            -- Take return type from definition (where it's cleaner)
            MAX(return_type) as return_type,
            MIN(start_line) as start_line,
            MAX(end_line) as end_line,
            -- Take line count from definition (more meaningful)
            MAX(CASE WHEN node_type = 'function_definition' THEN line_count END) as definition_lines,
            -- Take complexity from definition
            MAX(CASE WHEN node_type = 'function_definition' THEN descendant_count END) as complexity,
            semantic_type,
            -- Show what node types we found
            STRING_AGG(DISTINCT node_type, ', ') as found_nodes,
            -- Take signature from definition
            MAX(CASE WHEN node_type = 'function_definition' THEN signature_sample END) as signature
        FROM function_nodes
        WHERE function_name IS NOT NULL
        GROUP BY function_name, class_name, semantic_type
    )
    SELECT 
        function_name,
        class_name,
        return_type,
        start_line,
        end_line,
        COALESCE(definition_lines, end_line - start_line + 1) as line_count,
        complexity,
        found_nodes,
        signature
    FROM unified_functions
    ORDER BY start_line;

-- Analyze UnifiedASTBackend class
FROM extract_cpp_functions_unified('src/unified_ast_backend.cpp');