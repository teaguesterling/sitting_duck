-- Test fixed chain methods implementation

-- First, call the registration function
SELECT duckdb_ast_register_short_names();

-- Test 1: Basic chain method syntax with count_nodes
SELECT ast(nodes).get_type('function_definition').count_nodes()
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test 2: Chain with filtering and counting
SELECT ast(nodes).get_type('function_definition').filter_pattern('%hello%').count_nodes()
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test 3: Get first function
SELECT ast(nodes).get_type('function_definition').first_node()
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test 4: Multiple methods in one query
SELECT 
    ast(nodes).get_type('function_definition').count_nodes() as func_count,
    ast(nodes).get_type('class_definition').count_nodes() as class_count,
    ast(nodes).filter_has_name().len() as named_nodes
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test 5: Navigation and counting
SELECT ast(nodes).get_type('class_definition').first_node().nav_children(nodes, 5).size()
FROM read_ast_objects('test/data/python/simple.py', 'python');