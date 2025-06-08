-- Visual representations of tree navigation functions
-- This file demonstrates how the helper functions work with ASCII art and examples

-- First, let's create a simple example AST structure
CREATE OR REPLACE TEMPORARY TABLE example_ast AS 
SELECT * FROM (VALUES
    -- node_id, type, name, parent_id, depth, sibling_index, children_count, descendant_count
    (0, 'translation_unit', NULL, NULL, 0, 0, 3, 15),      -- Root with 3 children, 15 total descendants
    (1, 'function_definition', 'main', 0, 1, 0, 2, 7),     -- Function with 2 children, 7 descendants
    (2, 'function_declarator', 'main', 1, 2, 0, 2, 3),     -- Declarator with 2 children
    (3, 'identifier', 'main', 2, 3, 0, 0, 0),              -- Leaf node
    (4, 'parameter_list', NULL, 2, 3, 1, 1, 1),            -- Parameter list with 1 child
    (5, 'parameter_declaration', NULL, 4, 4, 0, 0, 0),     -- Leaf node
    (6, 'compound_statement', NULL, 1, 2, 1, 2, 3),        -- Function body with 2 children
    (7, 'expression_statement', NULL, 6, 3, 0, 1, 1),      -- Statement with 1 child
    (8, 'call_expression', 'printf', 7, 4, 0, 0, 0),       -- Leaf node
    (9, 'return_statement', NULL, 6, 3, 1, 1, 1),          -- Return with 1 child
    (10, 'number_literal', '0', 9, 4, 0, 0, 0),            -- Leaf node
    (11, 'variable_declaration', 'x', 0, 1, 1, 1, 1),      -- Global var with 1 child
    (12, 'init_declarator', 'x', 11, 2, 0, 0, 0),          -- Leaf node
    (13, 'function_definition', 'helper', 0, 1, 2, 2, 2),  -- Another function
    (14, 'function_declarator', 'helper', 13, 2, 0, 0, 0), -- Leaf node
    (15, 'compound_statement', NULL, 13, 2, 1, 0, 0)       -- Empty body
) AS t(node_id, type, name, parent_id, depth, sibling_index, children_count, descendant_count);

-- ASCII representation of the tree structure:
SELECT '
=== EXAMPLE AST STRUCTURE ===

translation_unit (id=0, desc=15)
├── function_definition "main" (id=1, desc=7)
│   ├── function_declarator "main" (id=2, desc=3)
│   │   ├── identifier "main" (id=3, desc=0)
│   │   └── parameter_list (id=4, desc=1)
│   │       └── parameter_declaration (id=5, desc=0)
│   └── compound_statement (id=6, desc=3)
│       ├── expression_statement (id=7, desc=1)
│       │   └── call_expression "printf" (id=8, desc=0)
│       └── return_statement (id=9, desc=1)
│           └── number_literal "0" (id=10, desc=0)
├── variable_declaration "x" (id=11, desc=1)
│   └── init_declarator "x" (id=12, desc=0)
└── function_definition "helper" (id=13, desc=2)
    ├── function_declarator "helper" (id=14, desc=0)
    └── compound_statement (id=15, desc=0)

Key: (id=node_id, desc=descendant_count)
' AS tree_visualization;

-- Now let's demonstrate each helper function visually

-- 1. GET_CHILDREN - Returns immediate children only
SELECT '
=== GET_CHILDREN(ast, node_id=1) ===
Input: function_definition "main" (id=1)
Output: Its 2 immediate children

[1] function_definition "main"     <-- Input node
    ├── [2] function_declarator    <-- Returned
    └── [6] compound_statement     <-- Returned
' AS get_children_visual;

SELECT node_id, type, name, depth 
FROM get_children(example_ast, 1)
ORDER BY sibling_index;

-- 2. GET_DESCENDANTS - Returns node + all descendants
SELECT '
=== GET_DESCENDANTS(ast, node_id=1) ===
Input: function_definition "main" (id=1)
Output: Node itself + all 7 descendants

[1] function_definition "main"     <-- Included (self)
    ├── [2] function_declarator    <-- Included
    │   ├── [3] identifier         <-- Included  
    │   └── [4] parameter_list     <-- Included
    │       └── [5] parameter_decl <-- Included
    └── [6] compound_statement     <-- Included
        ├── [7] expression_stmt    <-- Included
        │   └── [8] call_expr      <-- Included
        └── [9] return_statement   <-- Included
            └── [10] number_lit    <-- Included

Uses: node_id BETWEEN 1 AND 1+7 (descendant_count)
' AS get_descendants_visual;

SELECT node_id, type, name, depth 
FROM get_descendants(example_ast, 1)
ORDER BY node_id;

-- 3. GET_SUBTREE - Returns descendants excluding the root
SELECT '
=== GET_SUBTREE(ast, node_id=1) ===
Input: function_definition "main" (id=1)
Output: All descendants WITHOUT the input node

[1] function_definition "main"     <-- NOT included
    ├── [2] function_declarator    <-- Included
    │   ├── [3] identifier         <-- Included  
    │   └── [4] parameter_list     <-- Included
    │       └── [5] parameter_decl <-- Included
    └── [6] compound_statement     <-- Included
        ├── [7] expression_stmt    <-- Included
        │   └── [8] call_expr      <-- Included
        └── [9] return_statement   <-- Included
            └── [10] number_lit    <-- Included
' AS get_subtree_visual;

SELECT node_id, type, name, depth 
FROM get_subtree(example_ast, 1)
ORDER BY node_id;

-- 4. GET_SIBLINGS - Returns nodes with same parent
SELECT '
=== GET_SIBLINGS(ast, node_id=11) ===
Input: variable_declaration "x" (id=11)
Output: All children of translation_unit (id=0)

translation_unit (id=0)
├── [1] function_definition "main"    <-- Sibling
├── [11] variable_declaration "x"     <-- Input (included)
└── [13] function_definition "helper" <-- Sibling
' AS get_siblings_visual;

SELECT node_id, type, name, sibling_index
FROM get_siblings(example_ast, 11)
ORDER BY sibling_index;

-- 5. GET_ANCESTORS - Returns all ancestors up to root
SELECT '
=== GET_ANCESTORS(ast, node_id=8) ===
Input: call_expression "printf" (id=8)
Output: Path from root to parent (excluding input node)

[0] translation_unit              <-- Returned
└── [1] function_definition       <-- Returned
    └── [6] compound_statement    <-- Returned
        └── [7] expression_stmt   <-- Returned (parent)
            └── [8] call_expr     <-- Input (NOT included)
' AS get_ancestors_visual;

SELECT node_id, type, name, depth
FROM get_ancestors(example_ast, 8)
ORDER BY depth;

-- 6. GET_PATH_TO_NODE - Returns full path including the node
SELECT '
=== GET_PATH_TO_NODE(ast, node_id=8) ===
Input: call_expression "printf" (id=8)
Output: Complete path from root to node

[0] translation_unit              <-- Returned
└── [1] function_definition       <-- Returned
    └── [6] compound_statement    <-- Returned
        └── [7] expression_stmt   <-- Returned
            └── [8] call_expr     <-- Returned (included!)
' AS get_path_visual;

SELECT node_id, type, name, depth
FROM get_path_to_node(example_ast, 8)
ORDER BY depth;

-- 7. EFFICIENCY DEMONSTRATION
SELECT '
=== EFFICIENCY: Depth-First Traversal ===

The tree is stored in depth-first order:
ID:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15

This allows O(1) subtree extraction using:
- Start: node_id
- End: node_id + descendant_count

Example: Get descendants of node 1 (descendant_count=7)
Range: [1, 1+7] = [1, 8]
Result: nodes 1,2,3,4,5,6,7,8 ✓

No tree traversal needed!
' AS efficiency_visual;

-- 8. PRACTICAL EXAMPLE: Finding function complexity
SELECT '
=== PRACTICAL USE: Function Complexity ===

To calculate complexity of "main" function:
1. Find function node (id=1)
2. Use get_descendants to get all nodes in function
3. Count control flow nodes

' AS practical_example;

WITH function_nodes AS (
    SELECT node_id, name, descendant_count
    FROM example_ast
    WHERE type = 'function_definition'
),
complexity AS (
    SELECT 
        f.name as function_name,
        COUNT(CASE WHEN d.type IN ('expression_statement', 'return_statement') THEN 1 END) as statement_count,
        COUNT(CASE WHEN d.type = 'call_expression' THEN 1 END) as call_count,
        COUNT(*) as total_nodes
    FROM function_nodes f
    CROSS JOIN LATERAL get_descendants(example_ast, f.node_id) d
    GROUP BY f.name
)
SELECT * FROM complexity;

-- 9. TREE NAVIGATION PATTERNS
SELECT '
=== COMMON NAVIGATION PATTERNS ===

1. Find all methods in a class:
   - Find class_specifier node
   - Use get_descendants to get all nodes
   - Filter for function_definition nodes

2. Extract function signature:
   - Find function_declarator node  
   - Use get_children to find identifier and parameter_list
   - Use get_children on parameter_list for parameters

3. Find variable usage:
   - Find variable_declaration
   - Use get_siblings to search in same scope
   - Or use get_ancestors to find enclosing function

4. Extract nested structures:
   - Use find_nearest_ancestor_of_type
   - Recursively apply to find all nesting levels
' AS navigation_patterns;