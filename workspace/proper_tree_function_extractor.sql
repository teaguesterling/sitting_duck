-- Proper tree-based function extractor without using peek
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Helper to get descendants within N nodes (using depth-first property)
CREATE OR REPLACE MACRO get_n_descendants(file_path, parent_node_id, parent_descendant_count) AS TABLE
    SELECT * FROM read_ast(file_path)
    WHERE node_id > parent_node_id 
      AND node_id <= parent_node_id + parent_descendant_count;

-- Extract function information using tree structure
CREATE OR REPLACE MACRO extract_functions_tree_based(file_path) AS TABLE
    WITH function_nodes AS (
        SELECT 
            node_id,
            type,
            start_line,
            end_line,
            children_count,
            descendant_count,
            semantic_type
        FROM read_ast(file_path)
        WHERE type IN ('function_definition', 'function_declarator')
          AND semantic_type = 112
    ),
    -- Extract function names from declarator children
    declarator_info AS (
        SELECT 
            fn.node_id,
            fn.type,
            fn.start_line,
            fn.end_line,
            fn.descendant_count,
            -- Get the qualified_identifier or identifier child
            MAX(CASE 
                WHEN c.type = 'qualified_identifier' THEN c.name
                WHEN c.type = 'identifier' THEN c.name
            END) as full_name,
            -- Get parameter list info
            MAX(CASE 
                WHEN c.type = 'parameter_list' THEN c.children_count
            END) as param_count
        FROM function_nodes fn
        JOIN read_ast(file_path) c ON c.parent_id = fn.node_id
        WHERE fn.type = 'function_declarator'
        GROUP BY fn.node_id, fn.type, fn.start_line, fn.end_line, fn.descendant_count
    ),
    -- Extract return type from function_definition structure
    definition_info AS (
        SELECT 
            fn.node_id,
            fn.type,
            fn.start_line,
            fn.end_line,
            fn.descendant_count,
            -- Find the function_declarator child
            MAX(CASE 
                WHEN c.type = 'function_declarator' THEN c.node_id
            END) as declarator_id,
            -- Find return type (usually first child before declarator)
            MAX(CASE 
                WHEN c.type IN ('primitive_type', 'type_identifier', 'template_type') 
                 AND c.sibling_index = 0 THEN c.name
            END) as return_type
        FROM function_nodes fn
        JOIN read_ast(file_path) c ON c.parent_id = fn.node_id
        WHERE fn.type = 'function_definition'
        GROUP BY fn.node_id, fn.type, fn.start_line, fn.end_line, fn.descendant_count
    ),
    -- Combine information
    combined AS (
        SELECT 
            COALESCE(di.full_name, '') as full_qualified_name,
            -- Extract function name from qualified name
            CASE 
                WHEN di.full_name LIKE '%::%' THEN 
                    REGEXP_EXTRACT(di.full_name, '::([^:]+)$', 1)
                ELSE di.full_name
            END as function_name,
            -- Extract class name from qualified name
            CASE 
                WHEN di.full_name LIKE '%::%' THEN 
                    REGEXP_EXTRACT(di.full_name, '^(.+)::[^:]+$', 1)
            END as class_name,
            COALESCE(def.return_type, '') as return_type,
            COALESCE(di.start_line, def.start_line) as start_line,
            COALESCE(def.end_line, di.end_line) as end_line,
            COALESCE(def.descendant_count, di.descendant_count) as complexity,
            COALESCE(di.param_count, 0) as param_count,
            CASE 
                WHEN di.full_name LIKE '%::%' THEN 'method'
                ELSE 'function'
            END as function_type
        FROM declarator_info di
        FULL OUTER JOIN definition_info def 
            ON di.node_id = def.declarator_id
    )
    SELECT 
        function_name,
        class_name,
        function_type,
        return_type,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        complexity,
        param_count
    FROM combined
    WHERE function_name IS NOT NULL AND function_name != ''
    ORDER BY start_line;

-- Test the tree-based extractor
FROM extract_functions_tree_based('src/unified_ast_backend.cpp');