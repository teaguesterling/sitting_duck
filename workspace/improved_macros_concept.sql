-- Enhanced macros using semantic type functions (once implemented)
-- This shows how the semantic_type_to_string function would improve usability

LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Enhanced analysis with readable semantic types
CREATE OR REPLACE MACRO analyze_with_names(file_path) AS TABLE
    SELECT 
        semantic_type,
        -- This will work once we build the new functions:
        -- semantic_type_to_string(semantic_type) as semantic_name,
        -- get_super_kind(semantic_type) as super_kind,
        -- get_kind(semantic_type) as kind,
        
        -- For now, let's decode manually to show the concept:
        CASE ((semantic_type & 192) >> 6)  -- bits 6-7 for super_kind
            WHEN 0 THEN 'DATA_STRUCTURE'
            WHEN 1 THEN 'COMPUTATION' 
            WHEN 2 THEN 'CONTROL_EFFECTS'
            WHEN 3 THEN 'META_EXTERNAL'
            ELSE 'UNKNOWN'
        END as super_kind_manual,
        
        CASE (semantic_type & 48)  -- bits 4-5 for kind within super_kind
            WHEN 0 THEN 'KIND_0'
            WHEN 16 THEN 'KIND_1' 
            WHEN 32 THEN 'KIND_2'
            WHEN 48 THEN 'KIND_3'
            ELSE 'UNKNOWN'
        END as kind_manual,
        
        COUNT(*) as count,
        STRING_AGG(DISTINCT type, ', ') as example_types
    FROM read_ast(file_path)
    WHERE semantic_type > 0
    GROUP BY semantic_type
    ORDER BY count DESC
    LIMIT 15;

-- Enhanced function filtering using semantic patterns  
CREATE OR REPLACE MACRO find_functions_enhanced(file_path) AS TABLE
    SELECT 
        name,
        type,
        semantic_type,
        start_line,
        end_line - start_line + 1 as line_count,
        -- This will work once we build the new functions:
        -- is_semantic_type(semantic_type, 'FUNCTION') as is_function,
        
        -- Manual check for DEFINITION_FUNCTION (0111 0000 = 112)
        CASE WHEN semantic_type = 112 THEN true ELSE false END as is_function_manual,
        
        descendant_count
    FROM read_ast(file_path)
    WHERE semantic_type = 112  -- DEFINITION_FUNCTION
       OR type LIKE '%function%'
    ORDER BY start_line;