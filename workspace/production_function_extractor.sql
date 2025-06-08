-- Production-ready universal C++ function extractor
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO extract_cpp_functions_universal(file_path) AS TABLE
    WITH raw_functions AS (
        SELECT 
            type,
            peek,
            start_line,
            end_line,
            end_line - start_line + 1 as line_count,
            descendant_count,
            semantic_type
        FROM read_ast(file_path)
        WHERE type IN ('function_definition', 'function_declarator')
          AND semantic_type = 112  -- DEFINITION_FUNCTION only
    ),
    function_parsed AS (
        SELECT 
            *,
            -- Determine function category
            CASE 
                WHEN peek LIKE '%::%' THEN 'method'
                WHEN peek LIKE 'static %' THEN 'static_function'
                WHEN peek LIKE 'extern %' THEN 'extern_function'  
                WHEN peek LIKE 'DUCKDB_EXTENSION_API%' THEN 'api_function'
                ELSE 'free_function'
            END as function_category,
            
            -- Extract function name with fallback patterns
            COALESCE(
                -- Class methods: Class::method(
                NULLIF(REGEXP_EXTRACT(peek, '::([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1), ''),
                
                -- Static functions: static type func(
                NULLIF(REGEXP_EXTRACT(peek, 'static\s+[^(]*?([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1), ''),
                
                -- API functions: DUCKDB_EXTENSION_API type func(
                NULLIF(REGEXP_EXTRACT(peek, 'DUCKDB_EXTENSION_API\s+[^(]*?([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1), ''),
                
                -- Free function declarators: clean function_name(params)
                CASE WHEN type = 'function_declarator' THEN
                    NULLIF(REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*$', 1), '')
                END,
                
                -- General fallback: any function_name( pattern
                NULLIF(REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1), '')
            ) as function_name,
            
            -- Extract class name for methods
            CASE 
                WHEN peek LIKE '%::%' THEN 
                    REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)::', 1)
            END as class_name,
            
            -- Extract return type (focus on definitions for cleaner results)
            CASE 
                WHEN type = 'function_definition' AND peek LIKE '%::%' THEN
                    TRIM(REGEXP_EXTRACT(peek, '^([^:]+)', 1))
                WHEN type = 'function_definition' AND peek LIKE 'static %' THEN
                    TRIM(REGEXP_EXTRACT(peek, 'static\s+([^(]+?)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(', 1))
                WHEN type = 'function_definition' THEN
                    TRIM(REGEXP_EXTRACT(peek, '^([^(]+?)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(', 1))
            END as return_type
        FROM raw_functions
    ),
    -- Consolidate function definition + declarator pairs
    consolidated AS (
        SELECT 
            function_name,
            class_name,
            function_category,
            -- Prefer return type from definition
            MAX(return_type) as return_type,
            MIN(start_line) as start_line,
            MAX(end_line) as end_line,
            -- Use definition line count if available, else use max
            COALESCE(
                MAX(CASE WHEN type = 'function_definition' THEN line_count END),
                MAX(line_count)
            ) as line_count,
            -- Use definition complexity if available  
            COALESCE(
                MAX(CASE WHEN type = 'function_definition' THEN descendant_count END),
                MAX(descendant_count)
            ) as complexity,
            STRING_AGG(DISTINCT type, ', ') as node_types,
            COUNT(*) as node_count
        FROM function_parsed
        WHERE function_name IS NOT NULL 
          AND function_name != ''
          AND function_name NOT IN ('con', 'db_wrapper')  -- Filter out constructor calls
        GROUP BY function_name, class_name, function_category
    )
    SELECT 
        function_name,
        class_name,
        function_category,
        NULLIF(TRIM(return_type), '') as return_type,
        start_line,
        end_line,
        line_count,
        complexity,
        node_types,
        node_count
    FROM consolidated
    ORDER BY start_line, function_name;

-- Test the production function extractor
FROM extract_cpp_functions_universal('src/duckdb_ast_extension.cpp');