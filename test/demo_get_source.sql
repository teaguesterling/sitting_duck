-- Demonstration of the new get_source chainable macro
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Method 1: Using ast_nodes_get_source directly
WITH ast_data AS (
    SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
),
function_nodes AS (
    -- Extract just function definition nodes
    SELECT (
        SELECT json_group_array(value) 
        FROM json_each(nodes::JSON) 
        WHERE json_extract_string(value, '$.type') = 'function_definition'
    ) as functions
    FROM ast_data
)
SELECT 
    ast_nodes_get_source(functions, 2) as functions_with_source
FROM function_nodes;

-- Method 2: Using the chain method get_source
WITH ast_data AS (
    SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
)
SELECT
    nodes::JSON
        -> filter(node -> json_extract_string(node, '$.type') = 'function_definition')
        -> get_source(2)
    AS functions_with_source
FROM ast_data;

-- Method 3: Getting source for specific node types
WITH ast_data AS (
    SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
)
SELECT
    -- Get all class definitions with their source
    nodes::JSON
        -> filter(node -> json_extract_string(node, '$.type') = 'class_definition')
        -> get_source(0) as class_sources,
    -- Get all function definitions with their source
    nodes::JSON
        -> filter(node -> json_extract_string(node, '$.type') = 'function_definition')
        -> get_source(1) as function_sources
FROM ast_data;