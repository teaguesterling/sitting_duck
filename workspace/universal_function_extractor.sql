-- Universal C++ function name extractor that works for all function types
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO extract_all_cpp_functions(file_path) AS TABLE
    WITH function_analysis AS (
        SELECT 
            type,
            peek,
            start_line,
            end_line,
            end_line - start_line + 1 as line_count,
            descendant_count,
            semantic_type,
            
            -- Determine function type first
            CASE 
                WHEN peek LIKE '%::%' THEN 'class_method'
                WHEN peek LIKE 'static %' THEN 'static_function'
                WHEN peek LIKE 'extern %' THEN 'extern_function'
                WHEN peek LIKE 'DUCKDB_EXTENSION_API%' THEN 'api_function'
                ELSE 'free_function'
            END as function_category,
            
            -- Extract function name using appropriate pattern for each type
            CASE 
                -- Class methods: Class::method(
                WHEN peek LIKE '%::%' THEN 
                    REGEXP_EXTRACT(peek, '::([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1)
                    
                -- Static functions: static return_type function_name(
                WHEN peek LIKE 'static %' THEN
                    REGEXP_EXTRACT(peek, 'static\s+(?:[A-Za-z_][A-Za-z0-9_<>*&\s]*\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1)
                    
                -- API functions: DUCKDB_EXTENSION_API return_type function_name(
                WHEN peek LIKE 'DUCKDB_EXTENSION_API%' THEN
                    REGEXP_EXTRACT(peek, 'DUCKDB_EXTENSION_API\s+(?:[A-Za-z_][A-Za-z0-9_<>*&\s]*\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1)
                    
                -- Free functions (declarators): return_type function_name( or just function_name(
                WHEN type = 'function_declarator' THEN
                    COALESCE(
                        REGEXP_EXTRACT(peek, '(?:^|\s)([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*$', 1),
                        REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1)
                    )
                    
                -- Free functions (definitions): more complex pattern matching
                ELSE
                    COALESCE(
                        -- Try: return_type function_name( pattern
                        REGEXP_EXTRACT(peek, '(?:^|\n)\s*(?:[A-Za-z_][A-Za-z0-9_<>*&:\s]*\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*\{', 1),
                        -- Fallback: any function_name( pattern
                        REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1)
                    )
            END as function_name,
            
            -- Extract class name for methods
            CASE 
                WHEN peek LIKE '%::%' THEN 
                    REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)::', 1)
                ELSE NULL
            END as class_name,
            
            -- Extract return type (simplified)
            CASE 
                WHEN peek LIKE '%::%' AND type = 'function_definition' THEN
                    TRIM(REGEXP_EXTRACT(peek, '^([^:]+)', 1))
                WHEN peek LIKE 'static %' THEN
                    TRIM(REGEXP_EXTRACT(peek, 'static\s+([^(]+?)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(', 1))
                WHEN type = 'function_definition' THEN
                    TRIM(REGEXP_EXTRACT(peek, '^([^(]+?)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(', 1))
                ELSE NULL
            END as return_type
            
        FROM read_ast(file_path)
        WHERE type IN ('function_definition', 'function_declarator')
          AND semantic_type = 112  -- DEFINITION_FUNCTION
    ),
    -- Group definition and declarator pairs
    consolidated_functions AS (
        SELECT 
            function_name,
            class_name,
            function_category,
            return_type,
            MIN(start_line) as start_line,
            MAX(end_line) as end_line,
            MAX(line_count) as line_count,
            MAX(descendant_count) as complexity,
            STRING_AGG(DISTINCT type, ', ') as node_types
        FROM function_analysis
        WHERE function_name IS NOT NULL AND function_name != ''
        GROUP BY function_name, class_name, function_category, return_type
    )
    SELECT 
        function_name,
        NULLIF(class_name, '') as class_name,
        function_category,
        NULLIF(TRIM(return_type), '') as return_type,
        start_line,
        end_line,
        line_count,
        complexity,
        node_types
    FROM consolidated_functions
    ORDER BY start_line;

-- Test on the extension file with mixed function types
FROM extract_all_cpp_functions('src/duckdb_ast_extension.cpp');