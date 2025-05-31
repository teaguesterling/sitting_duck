-- Test CTE approach for extracting source from nodes with embedded file_path
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- First, let's verify we have nodes with file_path
WITH ast_data AS (
    SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
),
sample_nodes AS (
    SELECT (
        SELECT json_group_array(value) 
        FROM (
            SELECT value 
            FROM json_each(nodes::JSON) 
            WHERE json_extract_string(value, '$.type') = 'function_definition'
            LIMIT 3
        )
    ) as functions
    FROM ast_data
)
SELECT 
    json_extract_string(func.value, '$.file_path') as file_path,
    json_extract_string(func.value, '$.name') as name,
    json_extract(func.value, '$.start.line') as start_line
FROM sample_nodes, json_each(functions::JSON) as func;

-- Now test the CTE approach to read files dynamically
WITH 
-- Sample nodes (simulating what we'd pass to the macro)
nodes_json AS (
    SELECT nodes as all_nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
),
function_nodes AS (
    SELECT (
        SELECT json_group_array(value) 
        FROM (
            SELECT value 
            FROM json_each(all_nodes::JSON) 
            WHERE json_extract_string(value, '$.type') = 'function_definition'
        )
    ) as nodes
    FROM nodes_json
),
-- Get unique file paths from the nodes
unique_files AS (
    SELECT DISTINCT json_extract_string(je.value, '$.file_path') as file_path
    FROM function_nodes, json_each(nodes::JSON) AS je
    WHERE json_extract_string(je.value, '$.file_path') IS NOT NULL
),
-- Read content for each unique file
file_contents AS (
    SELECT 
        uf.file_path,
        (SELECT content FROM read_text(uf.file_path)) as content
    FROM unique_files uf
),
-- Join nodes with their file contents and extract source
nodes_with_source AS (
    SELECT 
        je.value as node,
        ast_get_source(
            fc.content,
            CAST(json_extract(je.value, '$.start.line') AS INTEGER),
            CAST(json_extract(je.value, '$.end.line') AS INTEGER),
            1  -- context_lines
        ) as source
    FROM function_nodes, json_each(nodes::JSON) AS je
    JOIN file_contents fc ON json_extract_string(je.value, '$.file_path') = fc.file_path
)
-- Show results
SELECT 
    json_extract_string(node, '$.name') as function_name,
    json_extract_string(node, '$.file_path') as file_path,
    CAST(json_extract(node, '$.start.line') AS INTEGER) as start_line,
    CAST(json_extract(node, '$.end.line') AS INTEGER) as end_line,
    source
FROM nodes_with_source
ORDER BY start_line;