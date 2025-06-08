-- Comparison of Table-Style vs Monad-Style Tree Navigation
-- This demonstrates the differences and when to use each approach

-- ============================================
-- SETUP: Load extension and helpers
-- ============================================
LOAD duckdb_ast;
.read src/sql_macros/06_tree_helpers.sql
.read src/sql_macros/07_monad_tree_helpers.sql

-- ============================================
-- APPROACH 1: TABLE STYLE (read_ast)
-- ============================================
SELECT '=== TABLE STYLE APPROACH ===' as approach;

-- Each node is a row in a table
WITH ast_table AS (
    SELECT * FROM read_ast('test/data/cpp/simple.cpp')
),
-- Find a function
func_node AS (
    SELECT node_id, name, descendant_count
    FROM ast_table
    WHERE type = 'function_definition'
    LIMIT 1
)
-- Get its children using table-style helper
SELECT 
    'Children of function:' as description,
    c.type,
    c.name
FROM func_node f
CROSS JOIN LATERAL get_children(ast_table, f.node_id) c;

-- ============================================
-- APPROACH 2: MONAD STYLE (read_ast_objects)
-- ============================================
SELECT '=== MONAD STYLE APPROACH ===' as approach;

-- Single AST object with nested array of nodes
WITH ast_monad AS (
    SELECT ast FROM read_ast_objects('test/data/cpp/simple.cpp')
),
-- Find function nodes using monad operations
func_nodes AS (
    SELECT unnest(monad_get_nodes_of_type(ast, 'function_definition')) as func_node
    FROM ast_monad
    LIMIT 1
)
-- Get children using monad-style helper
SELECT 
    'Children of function:' as description,
    child.type,
    child.name
FROM func_nodes f
CROSS JOIN LATERAL (
    SELECT unnest(monad_get_children((SELECT ast FROM ast_monad), f.func_node.node_id)) as child
) c;

-- ============================================
-- COMPARISON: Same Query, Both Styles
-- ============================================
SELECT '=== EXTRACTING FUNCTION INFO - TABLE STYLE ===' as comparison;

-- Table style: Direct SQL operations
WITH ast_table AS (
    SELECT * FROM read_ast('test/data/cpp/simple.cpp')
),
functions AS (
    SELECT 
        f.node_id,
        f.name as func_name,
        f.start_line,
        f.descendant_count
    FROM ast_table f
    WHERE f.type = 'function_definition'
),
function_details AS (
    SELECT 
        f.func_name,
        f.start_line,
        COUNT(DISTINCT CASE 
            WHEN d.type = 'if_statement' THEN d.node_id 
        END) as if_count,
        COUNT(DISTINCT CASE 
            WHEN d.type = 'call_expression' THEN d.node_id 
        END) as call_count
    FROM functions f
    JOIN LATERAL get_descendants(ast_table, f.node_id) d ON true
    GROUP BY f.func_name, f.start_line
)
SELECT * FROM function_details;

SELECT '=== EXTRACTING FUNCTION INFO - MONAD STYLE ===' as comparison;

-- Monad style: Functional operations on AST object
WITH ast_data AS (
    SELECT ast FROM read_ast_objects('test/data/cpp/simple.cpp')
)
SELECT 
    func.name as func_name,
    func.start_line,
    -- Count control structures in descendants
    (
        SELECT COUNT(*)
        FROM unnest(monad_get_descendants(ast, func.node_id)) as d
        WHERE d.type = 'if_statement'
    ) as if_count,
    -- Count function calls
    (
        SELECT COUNT(*)
        FROM unnest(monad_get_descendants(ast, func.node_id)) as d
        WHERE d.type = 'call_expression'
    ) as call_count
FROM ast_data
CROSS JOIN LATERAL unnest(monad_get_nodes_of_type(ast, 'function_definition')) as func;

-- ============================================
-- WHEN TO USE EACH STYLE
-- ============================================
SELECT '
=== WHEN TO USE EACH STYLE ===

TABLE STYLE (read_ast):
✓ Best for complex SQL queries with JOINs
✓ When you need to correlate multiple nodes
✓ For aggregations and GROUP BY operations
✓ When integrating with existing SQL workflows
✓ Better performance for large-scale analysis

MONAD STYLE (read_ast_objects):
✓ Best for functional/compositional operations
✓ When passing AST between functions
✓ For ast_get_* functions that preserve structure
✓ When you need the full AST as a single value
✓ More elegant for simple extractions

' as usage_guide;

-- ============================================
-- ADVANCED: Mixing Styles
-- ============================================
SELECT '=== MIXING STYLES ===' as advanced;

-- Start with monad, convert to table for complex analysis
WITH ast_monad AS (
    SELECT ast FROM read_ast_objects('test/data/cpp/simple.cpp')
),
-- Extract functions as monad operations
function_nodes AS (
    SELECT unnest(monad_get_nodes_by_semantic_type(ast, 112)) as func
    FROM ast_monad
),
-- Convert to table style for joining
flattened AS (
    SELECT 
        func.node_id,
        func.name,
        func.start_line,
        unnest(monad_get_descendants((SELECT ast FROM ast_monad), func.node_id)) as descendant
    FROM function_nodes
)
-- Now we can do complex SQL
SELECT 
    name as function_name,
    start_line,
    COUNT(DISTINCT CASE 
        WHEN descendant.type LIKE '%statement' THEN descendant.node_id 
    END) as statement_count,
    MAX(descendant.depth) - MIN(descendant.depth) as max_nesting
FROM flattened
GROUP BY name, start_line;

-- ============================================
-- PERFORMANCE CONSIDERATIONS
-- ============================================
SELECT '
=== PERFORMANCE NOTES ===

1. Table style generates all rows upfront
   - Higher memory usage for large files
   - But enables efficient SQL operations

2. Monad style keeps data compact
   - Lower memory footprint
   - But array operations can be slower

3. Descendant extraction is O(1) in both!
   - Range queries work the same way
   - descendant_count optimization applies

4. Choose based on your use case:
   - Analysis → Table style
   - Transformation → Monad style
' as performance_notes;