-- Natural AST queries using dot notation and SQL macros
LOAD json;
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Load our SQL macros (in practice, these would be loaded by the extension)
-- [Contents of ast_sql_macros.sql would go here]

-- Example 1: Get all function names from a Python file
SELECT nodes.ast_function_names() as functions
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Example 2: Get a complete summary of the code structure  
SELECT nodes.ast_summary() as code_summary
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Example 3: Find all nodes at depth 2 (direct children of functions/classes)
SELECT nodes.ast_at_depth(2) as depth_2_nodes
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Example 4: Count nodes by type
SELECT nodes.ast_type_counts() as type_distribution
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Example 5: Chain operations - get function names and their line numbers
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
)
SELECT 
    nodes.ast_function_names() as functions,
    nodes.ast_lines_of_type('function_definition') as function_lines
FROM ast_data;

-- Example 6: Search for specific text in the code
SELECT nodes.ast_contains_text('print') as print_nodes
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Example 7: Complex query - analyze multiple files
WITH code_analysis AS (
    SELECT 
        file_path,
        nodes.ast_function_names() as functions,
        nodes.ast_class_names() as classes,
        json_array_length(nodes) as total_nodes,
        nodes.ast_type_counts() as node_types
    FROM (
        SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
        UNION ALL
        SELECT * FROM read_ast_objects('test/data/python/calculator.py', 'python')
    )
)
SELECT * FROM code_analysis;

-- Example 8: Natural function chaining for analysis
SELECT 
    -- Get all functions
    nodes.ast_find_type('function_definition').json_array_length() as function_count,
    -- Get all classes  
    nodes.ast_find_type('class_definition').json_array_length() as class_count,
    -- Check complexity
    CASE 
        WHEN nodes.json_array_length() > 1000 THEN 'Complex'
        WHEN nodes.json_array_length() > 100 THEN 'Medium'
        ELSE 'Simple'
    END as complexity
FROM read_ast_objects('test/data/python/simple.py', 'python');