-- Advanced AST Struct Operations
-- Powerful macros that leverage the struct format's capabilities

-- ===================================
-- ADVANCED ANALYSIS
-- ===================================

-- Find all function calls within a specific function
CREATE OR REPLACE MACRO ast_function_calls_in(ast_struct, function_name) AS (
    WITH target_function AS (
        SELECT node.node_id, node.descendant_count
        FROM ast_nodes(ast_struct)
        WHERE node.type IN ('function_declaration', 'function_definition') 
          AND node.name = function_name
        LIMIT 1
    )
    SELECT call_node.node
    FROM ast_subtree(ast_struct, (SELECT node_id FROM target_function)) call_subtree,
         ast_nodes(call_subtree.node) as call_node
    WHERE call_node.node.type IN ('call_expression', 'function_call')
);

-- Get complexity metrics for functions (based on child count)
CREATE OR REPLACE MACRO ast_function_complexity(ast_struct) AS (
    SELECT 
        node.name as function_name,
        node.descendant_count as total_descendants,
        node.children_count as direct_children,
        node.end_line - node.start_line + 1 as line_count,
        CASE 
            WHEN node.descendant_count > 100 THEN 'high'
            WHEN node.descendant_count > 50 THEN 'medium' 
            ELSE 'low'
        END as complexity_rating
    FROM ast_nodes(ast_struct)
    WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')
      AND node.name IS NOT NULL
    ORDER BY node.descendant_count DESC
);

-- Find all variable references within a scope
CREATE OR REPLACE MACRO ast_variables_in_scope(ast_struct, scope_node_id) AS (
    SELECT DISTINCT var_node.node.name
    FROM ast_subtree(ast_struct, scope_node_id) scope_subtree,
         ast_nodes(scope_subtree.node) as var_node
    WHERE var_node.node.type = 'identifier'
      AND var_node.node.name IS NOT NULL
      AND var_node.node.name != ''
);

-- ===================================
-- CODE STRUCTURE ANALYSIS
-- ===================================

-- Get class hierarchy (classes and their methods)
CREATE OR REPLACE MACRO ast_class_structure(ast_struct) AS (
    SELECT 
        class_node.name as class_name,
        list(method_node.name) as methods,
        count(method_node.node_id) as method_count,
        class_node.start_line as class_start_line
    FROM ast_nodes(ast_struct) class_node
    LEFT JOIN ast_nodes(ast_struct) method_node 
        ON method_node.parent_id = class_node.node_id
        AND method_node.type IN ('method_definition', 'function_definition')
        AND method_node.name IS NOT NULL
    WHERE class_node.type IN ('class_declaration', 'class_definition')
      AND class_node.name IS NOT NULL
    GROUP BY class_node.name, class_node.start_line
    ORDER BY class_node.start_line
);

-- Find nested functions (functions defined inside other functions)
CREATE OR REPLACE MACRO ast_nested_functions(ast_struct) AS (
    SELECT 
        outer_func.name as outer_function,
        inner_func.name as inner_function,
        inner_func.depth - outer_func.depth as nesting_level
    FROM ast_nodes(ast_struct) outer_func
    JOIN ast_nodes(ast_struct) inner_func 
        ON inner_func.depth > outer_func.depth
        AND inner_func.node_id > outer_func.node_id
        AND inner_func.node_id <= outer_func.node_id + outer_func.descendant_count
    WHERE outer_func.type IN ('function_declaration', 'function_definition')
      AND inner_func.type IN ('function_declaration', 'function_definition')
      AND outer_func.name IS NOT NULL
      AND inner_func.name IS NOT NULL
);

-- Get import/dependency analysis
CREATE OR REPLACE MACRO ast_dependencies(ast_struct) AS (
    SELECT 
        node.source_text as import_statement,
        node.start_line as line_number,
        CASE 
            WHEN node.source_text LIKE '%from%' THEN 'from_import'
            WHEN node.source_text LIKE '%import%' THEN 'import' 
            WHEN node.source_text LIKE '%#include%' THEN 'include'
            ELSE 'other'
        END as import_type
    FROM ast_nodes(ast_struct)
    WHERE node.type IN ('import_statement', 'import_from_statement', 'preproc_include')
    ORDER BY node.start_line
);

-- ===================================
-- PERFORMANCE & OPTIMIZATION HELPERS
-- ===================================

-- Fast subtree extraction for large files
CREATE OR REPLACE MACRO ast_fast_subtree(ast_struct, root_type, name_filter := NULL) AS (
    WITH target_roots AS (
        SELECT 
            node.node_id,
            node.descendant_count,
            row_number() OVER (ORDER BY node.node_id) - 1 as start_pos
        FROM ast_nodes(ast_struct)
        WHERE node.type = root_type
          AND (name_filter IS NULL OR node.name = name_filter)
    ),
    indexed_nodes AS (
        SELECT 
            node.*,
            row_number() OVER (ORDER BY node.node_id) - 1 as position
        FROM ast_nodes(ast_struct)
    )
    SELECT 
        r.start_pos as subtree_id,
        n.node
    FROM indexed_nodes n
    JOIN target_roots r ON n.position BETWEEN r.start_pos AND r.start_pos + r.descendant_count
);

-- Get code coverage points (good for testing/analysis)
CREATE OR REPLACE MACRO ast_coverage_points(ast_struct) AS (
    SELECT 
        node.node_id,
        node.type,
        node.start_line,
        node.end_line,
        CASE 
            WHEN node.type IN ('if_statement', 'while_statement', 'for_statement') THEN 'control_flow'
            WHEN node.type IN ('function_declaration', 'function_definition', 'method_definition') THEN 'function'
            WHEN node.type IN ('try_statement', 'catch_clause', 'except_clause') THEN 'exception'
            ELSE 'other'
        END as coverage_category
    FROM ast_nodes(ast_struct)
    WHERE node.type IN (
        'if_statement', 'while_statement', 'for_statement', 'switch_statement',
        'function_declaration', 'function_definition', 'method_definition',
        'try_statement', 'catch_clause', 'except_clause'
    )
    ORDER BY node.start_line
);

-- ===================================
-- AI AGENT HELPERS
-- ===================================

-- Get a summary perfect for AI context windows
CREATE OR REPLACE MACRO ast_ai_summary(ast_struct, max_functions := 10) AS (
    WITH function_summary AS (
        SELECT 
            node.name as name,
            node.start_line as line,
            node.descendant_count as complexity
        FROM ast_nodes(ast_struct)
        WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')
          AND node.name IS NOT NULL
        ORDER BY node.start_line
        LIMIT max_functions
    ),
    class_summary AS (
        SELECT 
            node.name as name,
            node.start_line as line
        FROM ast_nodes(ast_struct)
        WHERE node.type IN ('class_declaration', 'class_definition')
          AND node.name IS NOT NULL
        ORDER BY node.start_line
        LIMIT 5
    )
    SELECT 
        ast_struct.file_path as file,
        ast_struct.language,
        length(ast_struct.nodes) as total_nodes,
        (SELECT max(node.depth) FROM ast_nodes(ast_struct)) as max_depth,
        list(fs) as functions,
        list(cs) as classes
    FROM (SELECT array_agg(function_summary) as fs FROM function_summary),
         (SELECT array_agg(class_summary) as cs FROM class_summary)
);

-- Extract method signatures for API documentation
CREATE OR REPLACE MACRO ast_api_signatures(ast_struct) AS (
    SELECT 
        node.name as method_name,
        node.type as node_type,
        node.start_line,
        node.source_text,
        CASE 
            WHEN node.source_text LIKE '%public%' THEN 'public'
            WHEN node.source_text LIKE '%private%' THEN 'private'
            WHEN node.source_text LIKE '%protected%' THEN 'protected'
            ELSE 'unknown'
        END as visibility
    FROM ast_nodes(ast_struct)
    WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')
      AND node.name IS NOT NULL
    ORDER BY node.start_line
);

-- Find TODO/FIXME comments in source
CREATE OR REPLACE MACRO ast_todos(ast_struct) AS (
    SELECT 
        node.source_text as comment,
        node.start_line as line,
        CASE 
            WHEN upper(node.source_text) LIKE '%TODO%' THEN 'TODO'
            WHEN upper(node.source_text) LIKE '%FIXME%' THEN 'FIXME'  
            WHEN upper(node.source_text) LIKE '%BUG%' THEN 'BUG'
            WHEN upper(node.source_text) LIKE '%HACK%' THEN 'HACK'
            ELSE 'NOTE'
        END as priority
    FROM ast_nodes(ast_struct)
    WHERE node.type LIKE '%comment%'
      AND (upper(node.source_text) LIKE '%TODO%' 
           OR upper(node.source_text) LIKE '%FIXME%'
           OR upper(node.source_text) LIKE '%BUG%'
           OR upper(node.source_text) LIKE '%HACK%')
    ORDER BY node.start_line
);