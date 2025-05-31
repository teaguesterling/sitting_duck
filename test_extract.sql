LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test 1: Direct ast_get_source
SELECT '=== Test 1: Direct ast_get_source ===';
SELECT ast_get_source('line1' || chr(10) || 'line2' || chr(10) || 'line3', 1, 3) as full;
SELECT ast_get_source('line1' || chr(10) || 'line2' || chr(10) || 'line3', 2, 2) as middle;

-- Test 2: Manual extraction from file
SELECT '=== Test 2: Manual extraction ===';
WITH fc AS (SELECT content FROM read_text('test/data/python/simple.py'))
SELECT ast_get_source(content, 1, 3) as lines_1_to_3
FROM fc;

-- Test 3: Using ast_extract_source
SELECT '=== Test 3: ast_extract_source ===';
SELECT ast_extract_source('test/data/python/simple.py', 1, 3) as extracted;

-- Test 4: Compare both methods
SELECT '=== Test 4: Comparison ===';
WITH 
method1 AS (
    SELECT ast_get_source(content, 1, 3) as result
    FROM read_text('test/data/python/simple.py')
),
method2 AS (
    SELECT ast_extract_source('test/data/python/simple.py', 1, 3) as result
)
SELECT 
    method1.result as manual_method,
    method2.result as macro_method,
    method1.result = method2.result as are_equal
FROM method1, method2;