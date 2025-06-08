-- Demonstration of tree-based AST navigation
-- Shows how to extract information without relying on the peek column

-- Load the extension and macros
LOAD duckdb_ast;
.read src/sql_macros/06_tree_helpers.sql
.read workspace/tree_based_function_extractor_v2.sql

-- First, let's examine a simple function to understand the tree structure
WITH ast_data AS (
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
),
-- Find a function declarator
sample_function AS (
    SELECT node_id, type, name, start_line, children_count, descendant_count
    FROM ast_data
    WHERE semantic_type = 112  -- DEFINITION_FUNCTION
    LIMIT 1
)
SELECT 
    sf.*,
    'Children:' as separator,
    c.type as child_type,
    c.name as child_name,
    c.sibling_index
FROM sample_function sf
CROSS JOIN LATERAL get_children(ast_data, sf.node_id) c;

-- Now let's show how to extract function information properly
SELECT '=== Function extraction without peek ===' as demo;

-- Extract all functions with their details
SELECT 
    function_name,
    class_name,
    function_type,
    start_line,
    end_line,
    line_count,
    param_count,
    complexity,
    qualified_name
FROM extract_functions_tree_based('src/unified_ast_backend.cpp')
ORDER BY start_line
LIMIT 10;

-- Show the power of tree navigation for finding specific patterns
SELECT '=== Finding all member function calls ===' as demo;

WITH ast_data AS (
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
),
-- Find all call expressions
call_expressions AS (
    SELECT node_id, start_line
    FROM ast_data
    WHERE type = 'call_expression'
),
-- For each call, look at its children to identify member function calls
member_calls AS (
    SELECT 
        ce.node_id,
        ce.start_line,
        -- Look for field_expression children (object.method pattern)
        MAX(CASE WHEN c.type = 'field_expression' THEN c.name END) as method_name,
        -- Also check for direct identifier calls
        MAX(CASE WHEN c.type = 'identifier' AND c.sibling_index = 0 THEN c.name END) as function_name
    FROM call_expressions ce
    CROSS JOIN LATERAL get_children(ast_data, ce.node_id) c
    GROUP BY ce.node_id, ce.start_line
)
SELECT 
    start_line,
    COALESCE(method_name, function_name) as called_function
FROM member_calls
WHERE COALESCE(method_name, function_name) IS NOT NULL
ORDER BY start_line
LIMIT 20;

-- Demonstrate finding nested structures
SELECT '=== Finding nested classes/structs ===' as demo;

WITH ast_data AS (
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
),
-- Find all class/struct definitions
class_nodes AS (
    SELECT 
        node_id,
        type,
        name,
        start_line,
        depth
    FROM ast_data
    WHERE type IN ('class_specifier', 'struct_specifier')
      AND name IS NOT NULL
),
-- For each class, check if it has a parent class
nested_classes AS (
    SELECT 
        c.name as class_name,
        c.type as class_type,
        c.start_line,
        c.depth,
        p.name as parent_class
    FROM class_nodes c
    LEFT JOIN LATERAL find_nearest_ancestor_of_type(ast_data, c.node_id, 'class_specifier') p ON true
)
SELECT * FROM nested_classes
ORDER BY depth, start_line;

-- Show how to extract method implementations for a specific class
SELECT '=== Methods of UnifiedASTBackend class ===' as demo;

WITH ast_data AS (
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
),
-- Find the UnifiedASTBackend class
target_class AS (
    SELECT node_id, descendant_count
    FROM ast_data
    WHERE type = 'class_specifier' 
      AND name = 'UnifiedASTBackend'
    LIMIT 1
),
-- Get all function definitions within this class
class_methods AS (
    SELECT 
        d.name as method_name,
        d.start_line,
        d.end_line,
        d.end_line - d.start_line + 1 as line_count
    FROM target_class tc
    CROSS JOIN LATERAL get_descendants(ast_data, tc.node_id) d
    WHERE d.semantic_type = 112  -- DEFINITION_FUNCTION
      AND d.name IS NOT NULL
)
SELECT * FROM class_methods
ORDER BY start_line;

-- Demonstrate context extraction
SELECT '=== Context around a specific identifier ===' as demo;

WITH ast_data AS (
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
),
-- Find an interesting identifier
target_identifier AS (
    SELECT node_id, name, start_line
    FROM ast_data
    WHERE type = 'identifier' 
      AND name = 'GetFlatTableColumnNames'
    LIMIT 1
),
-- Get context (2 levels up, 1 level down)
context_nodes AS (
    SELECT 
        c.type,
        c.name,
        c.depth,
        c.start_line,
        ti.start_line as target_line,
        CASE 
            WHEN c.start_line = ti.start_line THEN '>>> TARGET <<<'
            WHEN c.depth < (SELECT depth FROM ast_data WHERE node_id = ti.node_id) THEN 'ANCESTOR'
            ELSE 'DESCENDANT'
        END as relation
    FROM target_identifier ti
    CROSS JOIN LATERAL get_context(ast_data, ti.node_id, 2, 1) c
)
SELECT * FROM context_nodes
ORDER BY start_line;