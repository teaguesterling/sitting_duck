-- Test different C++ function patterns with current extraction method
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

WITH function_samples AS (
    SELECT 
        type,
        peek,
        start_line,
        -- Test current class method extraction
        REGEXP_EXTRACT(peek, '::([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1) as class_method_name,
        
        -- Test free function extraction (no ::)
        REGEXP_EXTRACT(peek, '^[^:]*?([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1) as general_function_name,
        
        -- More specific free function pattern
        REGEXP_EXTRACT(peek, '^\s*(?:static\s+)?(?:void\s+|[A-Za-z_][A-Za-z0-9_<>]*\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1) as free_function_name,
        
        -- Detect pattern type
        CASE 
            WHEN peek LIKE '%::%' THEN 'class_method'
            WHEN peek LIKE 'static %' THEN 'static_function'  
            ELSE 'free_function'
        END as function_type
        
    FROM read_ast('src/duckdb_ast_extension.cpp')
    WHERE type IN ('function_definition', 'function_declarator')
    ORDER BY start_line
)
SELECT 
    function_type,
    LEFT(peek, 80) as peek_sample,
    class_method_name,
    general_function_name, 
    free_function_name,
    -- Show which extraction worked
    CASE 
        WHEN class_method_name IS NOT NULL THEN class_method_name
        WHEN free_function_name IS NOT NULL THEN free_function_name
        WHEN general_function_name IS NOT NULL THEN general_function_name
        ELSE 'FAILED'
    END as extracted_name
FROM function_samples;