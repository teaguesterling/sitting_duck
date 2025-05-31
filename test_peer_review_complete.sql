-- Complete test of peer review features
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test 1: ast_get_source directly with string
SELECT '=== Test 1: ast_get_source ===';
SELECT ast_get_source('line1' || chr(10) || 'line2' || chr(10) || 'line3' || chr(10) || 'line4' || chr(10) || 'line5', 2, 4) as extracted_lines;
SELECT ast_get_source('line1' || chr(10) || 'line2' || chr(10) || 'line3' || chr(10) || 'line4' || chr(10) || 'line5', 2, 4, context_lines := 1) as with_context;

-- Test 2: ast_extract_source from file
SELECT '=== Test 2: ast_extract_source from file ===';
SELECT ast_extract_source('test/data/python/simple.py', 5, 8) as class_definition;
SELECT ast_extract_source('test/data/python/simple.py', 10, 11, context_lines := 1) as add_method_with_context;

-- Test 3: parse_ast function
SELECT '=== Test 3: parse_ast function ===';
SELECT json_extract_string(parse_ast('def hello(): print("hi")', 'python'), '$.children[0].name') as function_name;

-- Test 4: Full workflow - parse and extract source
SELECT '=== Test 4: Full AST workflow ===';
WITH parsed AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
),
functions AS (
    SELECT 
        node->>'name' as function_name,
        (node->'position'->>'start_row')::INTEGER + 1 as start_line,
        (node->'position'->>'end_row')::INTEGER + 1 as end_line
    FROM parsed, 
         json_each(parsed.nodes) as t(node)
    WHERE node->>'type' = 'function_definition'
)
SELECT 
    function_name,
    start_line,
    end_line,
    ast_extract_source('test/data/python/simple.py', start_line, end_line) as source_code
FROM functions;

-- Test 5: ast_get_calls (placeholder until parent tracking is added)
SELECT '=== Test 5: ast_get_calls ===';
WITH parsed AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
)
SELECT * FROM ast_get_calls(parsed.nodes) LIMIT 5;