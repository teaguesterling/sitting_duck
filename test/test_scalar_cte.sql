-- Test scalar CTE approach for dynamic file reading
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test 1: Can we use a scalar subquery with dynamic file path?
WITH file_paths AS (
    SELECT 'test/data/python/simple.py' as path
)
SELECT 
    path,
    (SELECT content FROM read_text(path)) as content_attempt
FROM file_paths;

-- Test 2: Try with a literal path to compare
SELECT 
    'test/data/python/simple.py' as path,
    (SELECT content FROM read_text('test/data/python/simple.py')) as content,
    LENGTH((SELECT content FROM read_text('test/data/python/simple.py'))) as content_length;

-- Test 3: Can we use CASE statement to work around this?
WITH nodes_sample AS (
    SELECT 
        json_extract_string(value, '$.file_path') as file_path,
        json_extract_string(value, '$.name') as name,
        CAST(json_extract(value, '$.start.line') AS INTEGER) as start_line,
        CAST(json_extract(value, '$.end.line') AS INTEGER) as end_line
    FROM (
        SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
    ), json_each(nodes::JSON)
    WHERE json_extract_string(value, '$.type') = 'function_definition'
    LIMIT 3
)
SELECT 
    name,
    file_path,
    start_line,
    end_line,
    -- Try to read file content based on path
    CASE 
        WHEN file_path = 'test/data/python/simple.py' 
        THEN (SELECT content FROM read_text('test/data/python/simple.py'))
        ELSE NULL
    END as file_content_case
FROM nodes_sample;

-- Test 4: What about using a prepared statement approach?
-- First, get unique paths
WITH unique_paths AS (
    SELECT DISTINCT json_extract_string(value, '$.file_path') as file_path
    FROM (
        SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
    ), json_each(nodes::JSON)
    WHERE json_extract_string(value, '$.file_path') IS NOT NULL
)
SELECT * FROM unique_paths;