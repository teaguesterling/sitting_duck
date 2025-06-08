-- Tree-based function extractor using the new helper macros
-- This version is cleaner and more maintainable

-- First, let's create a version that reads the AST once into a CTE
CREATE OR REPLACE MACRO extract_functions_tree_based(file_path) AS TABLE
WITH ast_data AS (
    SELECT * FROM read_ast(file_path)
),
-- Find all function declarators (semantic_type = 112)
function_nodes AS (
    SELECT 
        node_id,
        parent_id,
        start_line,
        end_line,
        children_count,
        descendant_count
    FROM ast_data
    WHERE semantic_type = 112  -- DEFINITION_FUNCTION
),
-- Extract function names from immediate children
function_names AS (
    SELECT 
        f.node_id,
        f.start_line,
        f.end_line,
        -- Get the name from identifier or qualified_identifier children
        MAX(CASE 
            WHEN c.type IN ('identifier', 'qualified_identifier', 'field_identifier', 'destructor_name')
            THEN c.name
        END) as function_name,
        -- Count parameters (look for parameter_list children)
        COUNT(DISTINCT CASE WHEN c.type = 'parameter_list' THEN c.node_id END) as has_params,
        -- Get the actual parameter count
        MAX(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count
    FROM function_nodes f
    JOIN (SELECT * FROM get_children(ast_data, f.node_id)) c ON true
    GROUP BY f.node_id, f.start_line, f.end_line
),
-- Extract class context by looking at ancestors
class_context AS (
    SELECT 
        fn.node_id,
        -- Find the nearest class/struct definition ancestor
        MAX(CASE 
            WHEN a.type IN ('class_specifier', 'struct_specifier') 
            THEN COALESCE(
                -- Try to get the class name from children
                (SELECT name FROM get_children(ast_data, a.node_id) 
                 WHERE type = 'type_identifier' LIMIT 1),
                -- Fallback to getting it from the name field
                a.name
            )
        END) as class_name
    FROM function_names fn
    LEFT JOIN LATERAL (
        SELECT * FROM get_ancestors(ast_data, fn.node_id)
        WHERE type IN ('class_specifier', 'struct_specifier')
        ORDER BY depth DESC
        LIMIT 1
    ) a ON true
    GROUP BY fn.node_id
),
-- Calculate complexity metrics using descendant counts
complexity_metrics AS (
    SELECT 
        f.node_id,
        -- Count control flow nodes in the function subtree
        COUNT(DISTINCT CASE 
            WHEN d.type IN ('if_statement', 'for_statement', 'while_statement', 
                           'do_statement', 'switch_statement', 'case_statement',
                           'for_range_loop', 'conditional_expression')
            THEN d.node_id 
        END) as complexity,
        -- Count return statements
        COUNT(DISTINCT CASE WHEN d.type = 'return_statement' THEN d.node_id END) as return_count,
        -- Count function calls
        COUNT(DISTINCT CASE WHEN d.type = 'call_expression' THEN d.node_id END) as call_count
    FROM function_nodes f
    JOIN LATERAL get_descendants(ast_data, f.node_id) d ON true
    GROUP BY f.node_id
)
-- Final result combining all information
SELECT 
    fn.function_name,
    cc.class_name,
    CASE 
        WHEN cc.class_name IS NOT NULL THEN 'method'
        ELSE 'function'
    END as function_type,
    fn.start_line,
    fn.end_line,
    fn.end_line - fn.start_line + 1 as line_count,
    fn.param_count,
    cm.complexity,
    cm.return_count,
    cm.call_count,
    -- Create fully qualified name
    CASE 
        WHEN cc.class_name IS NOT NULL 
        THEN cc.class_name || '::' || fn.function_name
        ELSE fn.function_name
    END as qualified_name
FROM function_names fn
LEFT JOIN class_context cc ON fn.node_id = cc.node_id
LEFT JOIN complexity_metrics cm ON fn.node_id = cm.node_id
WHERE fn.function_name IS NOT NULL
ORDER BY fn.start_line;

-- Example usage:
-- SELECT * FROM extract_functions_tree_based('src/unified_ast_backend.cpp');

-- Alternative version that shows the power of the tree helpers
-- This one also extracts the function signature
CREATE OR REPLACE MACRO extract_functions_with_signatures(file_path) AS TABLE
WITH ast_data AS (
    SELECT * FROM read_ast(file_path)
),
function_declarators AS (
    SELECT * FROM ast_data 
    WHERE semantic_type = 112
),
function_details AS (
    SELECT 
        fd.node_id,
        fd.start_line,
        fd.end_line,
        -- Get function name
        (SELECT name FROM get_children(ast_data, fd.node_id) 
         WHERE type IN ('identifier', 'qualified_identifier', 'field_identifier', 'destructor_name')
         LIMIT 1) as function_name,
        -- Get parameter list
        (SELECT children_count FROM get_children(ast_data, fd.node_id)
         WHERE type = 'parameter_list'
         LIMIT 1) as param_count,
        -- Get the entire signature by looking at the parent declaration
        (SELECT peek FROM get_parent(ast_data, fd.node_id) LIMIT 1) as signature_peek,
        -- Check if it's inside a class
        (SELECT name FROM find_nearest_ancestor_of_type(ast_data, fd.node_id, 'class_specifier') 
         LIMIT 1) as class_name
    FROM function_declarators fd
)
SELECT 
    function_name,
    class_name,
    CASE WHEN class_name IS NOT NULL THEN 'method' ELSE 'function' END as function_type,
    start_line,
    end_line,
    end_line - start_line + 1 as line_count,
    COALESCE(param_count, 0) as param_count,
    -- Extract just the first line of the signature
    CASE 
        WHEN signature_peek IS NOT NULL 
        THEN TRIM(SPLIT_PART(signature_peek, CHR(10), 1))
        ELSE NULL
    END as signature_line
FROM function_details
WHERE function_name IS NOT NULL
ORDER BY start_line;