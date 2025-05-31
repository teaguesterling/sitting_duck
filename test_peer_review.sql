-- Test peer review features
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test ast_get_source with different parameters
SELECT '--- Test 1: Basic ast_get_source ---';
SELECT ast_get_source('line1
line2
line3
line4
line5', 2, 3);

SELECT '--- Test 2: With context lines ---';
SELECT ast_get_source('line1
line2
line3
line4
line5', 2, 3, context_lines := 1);

-- Debug the macro by running its subparts
SELECT '--- Debug: Split lines ---';
WITH lines AS (
    SELECT 
        ROW_NUMBER() OVER () as line_num,
        line
    FROM (
        SELECT UNNEST(string_split('line1
line2
line3
line4
line5', E'\n')) as line
    )
)
SELECT * FROM lines;

SELECT '--- Debug: Line extraction with context ---';
WITH 
lines AS (
    SELECT 
        ROW_NUMBER() OVER () as line_num,
        line
    FROM (
        SELECT UNNEST(string_split('line1
line2
line3
line4
line5', E'\n')) as line
    )
),
line_range AS (
    SELECT 
        GREATEST(1, 2 - 1) as first_line,
        3 + 1 as last_line
)
SELECT line_num, line 
FROM lines, line_range
WHERE line_num >= line_range.first_line 
  AND line_num <= line_range.last_line;

-- Test other macros
SELECT '--- Test parse_ast function ---';
SELECT parse_ast('def test(): pass', 'python');

-- Test with real AST data
SELECT '--- Test with read_ast_objects ---';
CREATE TABLE test_ast AS 
SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python');

SELECT '--- Test ast_get_locations ---';
SELECT * FROM test_ast;