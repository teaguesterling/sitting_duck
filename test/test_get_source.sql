-- Test the new get_source chainable macro
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- First, let's get AST nodes with file paths
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
)
SELECT 
    -- Get all function definitions
    json_extract(nodes, '$[?(@.type == "function_definition")]') AS function_nodes
FROM ast_data;

-- Now use the chainable get_source to extract source code
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
),
functions AS (
    SELECT 
        json_extract(nodes::JSON, '$[?(@.type == "function_definition")]')::JSON AS function_nodes
    FROM ast_data
)
SELECT 
    -- Extract source code for all functions with 2 lines of context
    get_source(function_nodes, 2) AS functions_with_source
FROM functions;

-- Alternative: Get specific nodes and their source
WITH ast_data AS (
    SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
)
SELECT 
    -- Filter to get function definitions
    nodes::JSON
        -> filter(node -> node->>'type' = 'function_definition')
        -> get_source(1)  -- 1 line of context
    AS function_sources
FROM ast_data;