-- Test DuckDB JSON method syntax
LOAD json;

-- Create test data
CREATE TABLE test_ast AS 
SELECT '{
    "nodes": [
        {"type": "module", "children": [1, 2]},
        {"type": "function_definition", "name": "main", "children": [3]},
        {"type": "identifier", "content": "print"},
        {"type": "call", "children": [2]}
    ]
}'::JSON as ast;

-- Test various method calls
SELECT '=== Basic extraction ===' as test;
SELECT ast.json_extract('$.nodes[0].type') as root_type FROM test_ast;

SELECT '=== Array operations ===' as test;
SELECT ast.json_array_length('$.nodes') as node_count FROM test_ast;

SELECT '=== Multiple method chains? ===' as test;
-- Can we chain methods?
SELECT ast.json_extract('$.nodes').json_array_length() as count FROM test_ast;

SELECT '=== Extract and transform ===' as test;
SELECT ast.json_extract('$.nodes[?(@.type=="function_definition")]') as functions FROM test_ast;

-- Test with our actual function
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';
CREATE TABLE real_ast AS
SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python');

SELECT '=== Real AST data ===' as test;
SELECT nodes.json_array_length() as node_count FROM real_ast;
SELECT nodes.json_extract('$[0].type') as root_type FROM real_ast;