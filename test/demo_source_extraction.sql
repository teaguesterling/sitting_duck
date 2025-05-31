-- Demonstration of extracting source code for AST nodes
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Since we added file_path to each node, we can now extract source code
-- Note: Due to DuckDB limitations with dynamic file paths in read_text,
-- we need to pass the file content separately

WITH ast_and_content AS (
    SELECT 
        nodes,
        file_path,
        (SELECT content FROM read_text('test/data/python/simple.py')) as file_content
    FROM read_ast_objects('test/data/python/simple.py', 'python')
),
function_nodes AS (
    -- Extract just the function definition nodes
    SELECT 
        (SELECT json_group_array(value) 
         FROM (
            SELECT value 
            FROM json_each(nodes::JSON) 
            WHERE json_extract_string(value, '$.type') = 'function_definition'
         )
        ) as functions,
        file_content
    FROM ast_and_content
),
functions_with_source AS (
    -- Get source code for each function with 2 lines of context
    SELECT ast_nodes_get_source(functions, file_content, 2) as results
    FROM function_nodes
)
-- Display the results
SELECT 
    json_extract_string(func.value, '$.name') as function_name,
    json_extract_string(func.value, '$.file_path') as file,
    CAST(json_extract(func.value, '$.start_line') AS INTEGER) as start_line,
    CAST(json_extract(func.value, '$.end_line') AS INTEGER) as end_line,
    json_extract_string(func.value, '$.source') as source_code
FROM functions_with_source, 
     json_each(results::JSON) as func
ORDER BY start_line;