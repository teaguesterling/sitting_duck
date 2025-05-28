-- Test the new peer review features
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test ast_get_source with a simple example
WITH test_code AS (
    SELECT 'def hello():
    print("Hello, World!")
    return 42

def goodbye():
    print("Goodbye!")' as code
)
SELECT ast_get_source(code, 1, 3, 1) as function_with_context
FROM test_code;

-- Test ast_get_functions_with_source
SELECT * FROM ast_get_functions_with_source('test/data/python/simple.py', 'python', 1);

-- Test ast_get_locations
SELECT * FROM ast(nodes).get_locations()
FROM read_ast_objects('test/data/python/simple.py', 'python')
LIMIT 5;

-- Test getting source for specific functions
WITH parsed AS (
    SELECT 
        nodes,
        read_text('test/data/python/simple.py') as source_text
    FROM read_ast_objects('test/data/python/simple.py', 'python')
),
functions AS (
    SELECT 
        node,
        source_text
    FROM parsed,
         json_each(parsed.nodes) as t(node)
    WHERE node->>'type' = 'function_definition'
      AND node->>'name' = 'process_data'
)
SELECT 
    node->>'name' as function_name,
    ast_node_get_source(node, source_text, 2) as source_with_context
FROM functions;