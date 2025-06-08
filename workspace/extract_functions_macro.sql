-- Comprehensive function extraction macro for C++ files
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO extract_functions(file_path) AS TABLE
    WITH function_nodes AS (
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
    ),
    function_info AS (
        SELECT 
            type,
            peek,
            start_line,
            end_line,
            line_count,
            descendant_count,
            semantic_type,
            
            -- Extract function name (works for both definition and declarator)
            COALESCE(
                REGEXP_EXTRACT(peek, '::([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1),  -- Class method: Class::method(
                REGEXP_EXTRACT(peek, '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1), -- Free function: function(
                REGEXP_EXTRACT(peek, '\s([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1)    -- General case: return_type function(
            ) as function_name,
            
            -- Extract return type
            CASE 
                WHEN peek LIKE '%::%' THEN 
                    TRIM(REGEXP_EXTRACT(peek, '^([^:]+)', 1))  -- Everything before ::
                ELSE
                    TRIM(REGEXP_EXTRACT(peek, '^([^(]+?)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(', 1))  -- return_type before function_name
            END as return_type,
            
            -- Extract class name if it's a method
            REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)::', 1) as class_name,
            
            -- Determine function category
            CASE 
                WHEN peek LIKE '%::%' THEN 'method'
                WHEN peek LIKE '%static %' THEN 'static_function' 
                ELSE 'free_function'
            END as function_category
            
        FROM function_nodes
    ),
    -- Combine definition and declarator info for the same function
    combined_functions AS (
        SELECT 
            function_name,
            return_type,
            class_name,
            function_category,
            MIN(start_line) as start_line,
            MAX(end_line) as end_line,
            MAX(line_count) as line_count,
            MAX(descendant_count) as complexity,
            STRING_AGG(DISTINCT type, ', ') as node_types
        FROM function_info
        WHERE function_name IS NOT NULL
        GROUP BY function_name, return_type, class_name, function_category
    )
    SELECT 
        function_name,
        NULLIF(return_type, '') as return_type,
        NULLIF(class_name, '') as class_name,
        function_category,
        start_line,
        end_line,
        line_count,
        complexity,
        node_types
    FROM combined_functions
    ORDER BY start_line;

-- Test the macro
FROM extract_functions('src/unified_ast_backend.cpp');