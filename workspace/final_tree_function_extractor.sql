-- Final tree-based function extractor - no peek needed!
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

CREATE OR REPLACE MACRO extract_functions_from_tree(file_path) AS TABLE
    WITH 
    -- Step 1: Get all function declarators
    func_declarators AS (
        SELECT 
            node_id,
            parent_id,
            start_line,
            end_line,
            children_count,
            descendant_count,
            depth
        FROM read_ast(file_path)
        WHERE type = 'function_declarator'
          AND semantic_type = 112
    ),
    -- Step 2: Get immediate children of declarators
    declarator_info AS (
        SELECT 
            d.node_id,
            d.start_line,
            d.end_line,
            d.parent_id,
            d.descendant_count,
            -- Extract name from identifier or qualified_identifier child
            MAX(CASE 
                WHEN c.type = 'identifier' THEN c.name
                WHEN c.type = 'qualified_identifier' THEN c.name
            END) as full_name,
            -- Count parameters
            SUM(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count
        FROM func_declarators d
        JOIN read_ast(file_path) c ON c.parent_id = d.node_id
        GROUP BY d.node_id, d.start_line, d.end_line, d.parent_id, d.descendant_count
    ),
    -- Step 3: Get parent info for definitions
    definition_info AS (
        SELECT 
            di.node_id,
            di.full_name,
            di.start_line,
            di.end_line,
            di.param_count,
            di.descendant_count,
            p.type as parent_type,
            p.start_line as def_start_line,
            p.end_line as def_end_line,
            p.descendant_count as def_complexity
        FROM declarator_info di
        LEFT JOIN read_ast(file_path) p ON p.node_id = di.parent_id
    ),
    -- Step 4: Parse the function names
    parsed_functions AS (
        SELECT 
            full_name,
            -- Extract function name
            CASE 
                WHEN full_name LIKE '%::%' THEN 
                    REGEXP_EXTRACT(full_name, '::([^:]+)$', 1)
                ELSE full_name
            END as function_name,
            -- Extract class name
            CASE 
                WHEN full_name LIKE '%::%' THEN 
                    REGEXP_EXTRACT(full_name, '^(.+)::[^:]+$', 1)
                ELSE NULL
            END as class_name,
            -- Determine function type
            CASE 
                WHEN full_name LIKE '%::%' THEN 'method'
                ELSE 'function'
            END as function_type,
            -- Use definition boundaries if available, else declarator
            COALESCE(def_start_line, start_line) as start_line,
            COALESCE(def_end_line, end_line) as end_line,
            -- Use definition complexity if available
            COALESCE(def_complexity, descendant_count) as complexity,
            param_count,
            parent_type
        FROM definition_info
    )
    SELECT 
        function_name,
        class_name,
        function_type,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        complexity,
        param_count,
        parent_type
    FROM parsed_functions
    WHERE function_name IS NOT NULL 
      AND function_name != ''
      AND function_name NOT IN ('con', 'db_wrapper')  -- Filter constructor calls
    ORDER BY start_line;

-- Test on unified_ast_backend.cpp
SELECT * FROM extract_functions_from_tree('src/unified_ast_backend.cpp');