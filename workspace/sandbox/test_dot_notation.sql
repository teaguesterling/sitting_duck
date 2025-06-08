-- Test dot notation with JSON and custom functions
LOAD json;

-- First, let's test basic dot notation with built-in functions
SELECT '=== Built-in JSON functions with dot notation ===' as test;

-- Create test data
CREATE OR REPLACE TABLE test_json AS 
SELECT '{"name": "test", "nodes": [{"type": "function"}, {"type": "class"}]}'::JSON as data;

-- Standard function call
SELECT json_extract(data, '$.name') as standard_call FROM test_json;

-- Dot notation
SELECT data.json_extract('$.name') as dot_notation FROM test_json;

-- Chained operations
SELECT data.json_extract('$.nodes').json_array_length() as chained FROM test_json;

-- Now let's create SQL macros that would work naturally with AST data
SELECT '=== SQL Macros for AST operations ===' as test;

-- Macro to find nodes by type
CREATE OR REPLACE MACRO ast_find_type(json_data, node_type) AS (
    json_extract(json_data, '$[*]')::JSON[]
);

-- Macro to count nodes
CREATE OR REPLACE MACRO ast_count(json_data) AS (
    json_array_length(json_data)
);

-- Macro to get first node
CREATE OR REPLACE MACRO ast_first(json_data) AS (
    json_extract(json_data, '$[0]')
);

-- Test our macros
CREATE OR REPLACE TABLE test_ast AS 
SELECT '[{"type": "module", "children": [1,2]}, {"type": "function", "name": "main"}]'::JSON as nodes;

SELECT nodes.ast_count() as node_count FROM test_ast;
SELECT nodes.ast_first() as first_node FROM test_ast;

-- More complex macro for filtering
CREATE OR REPLACE MACRO ast_filter_type(json_data, node_type) AS (
    list_filter(
        json_extract(json_data, '$[*]')::JSON[],
        x -> json_extract_string(x, '$.type') = node_type
    )
);

-- This would allow natural queries like:
-- SELECT nodes.ast_filter_type('function') FROM read_ast_objects('file.py', 'python');