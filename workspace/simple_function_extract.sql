-- Simple but reliable function extraction
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Extract function names and details from C++ files
CREATE OR REPLACE MACRO extract_cpp_functions(file_path) AS TABLE
    SELECT 
        -- Extract function name using regex
        REGEXP_EXTRACT(peek, '::([A-Za-z_][A-Za-z0-9_]*)\s*\(', 1) as function_name,
        
        -- Extract class name 
        REGEXP_EXTRACT(peek, '([A-Za-z_][A-Za-z0-9_]*)::', 1) as class_name,
        
        -- Basic info
        type as node_type,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        descendant_count as complexity,
        semantic_type,
        
        -- Show first 100 chars of peek for debugging
        LEFT(peek, 100) as signature_preview
        
    FROM read_ast(file_path)
    WHERE type IN ('function_definition', 'function_declarator')
      AND peek LIKE '%::%'  -- Focus on class methods first
    ORDER BY start_line;