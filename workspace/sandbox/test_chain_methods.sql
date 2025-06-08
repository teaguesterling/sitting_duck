-- Test chain methods implementation

-- First, call the registration function
SELECT duckdb_ast_register_short_names();

-- Test 1: Basic chain method syntax
SELECT ast(nodes).get_type('function_definition').count()
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test 2: Chain with filtering
SELECT ast(nodes).get_type('function_definition').filter_pattern('%hello%').count()
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test 3: Navigation chain
SELECT ast(nodes).get_type('class_definition').first().nav_children(nodes, 5).count()
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test 4: Multiple chains
SELECT 
    ast(nodes).get_type('function_definition').count() as functions,
    ast(nodes).get_type('class_definition').count() as classes,
    ast(nodes).filter_has_name().count() as named_nodes
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test 5: Summary through chain
SELECT ast(nodes).summary()
FROM read_ast_objects('test/data/python/simple.py', 'python');