-- Working example of source extraction with embedded file paths
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Step 1: Parse the file and get AST nodes (which now include file_path)
WITH parsed AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
)
SELECT 
    file_path,
    node_count,
    -- Check that file_path is embedded in nodes
    json_extract_string(json_extract(nodes::JSON, '$[0]'), '$.file_path') as first_node_file
FROM parsed;

-- Step 2: Extract function definitions and their source
WITH parsed AS (
    SELECT 
        nodes,
        file_path
    FROM read_ast_objects('test/data/python/simple.py', 'python')
),
file_data AS (
    SELECT 
        parsed.nodes,
        parsed.file_path,
        read_text.content as file_content
    FROM parsed
    CROSS JOIN LATERAL (SELECT content FROM read_text(parsed.file_path)) as read_text
),
function_nodes AS (
    -- Filter to just function definitions
    SELECT 
        (SELECT json_group_array(value) 
         FROM json_each(nodes::JSON) 
         WHERE json_extract_string(value, '$.type') = 'function_definition'
        ) as functions,
        file_content
    FROM file_data
)
-- Extract source with context
SELECT 
    -- Use the chainable macro
    get_source(functions, file_content, 1) as functions_with_source
FROM function_nodes;

-- Step 3: Display the extracted source for each function
WITH parsed AS (
    SELECT nodes, file_path
    FROM read_ast_objects('test/data/python/simple.py', 'python')
),
file_data AS (
    SELECT 
        parsed.nodes,
        (SELECT content FROM read_text(parsed.file_path)) as file_content
    FROM parsed
),
function_nodes AS (
    SELECT 
        (SELECT json_group_array(value) 
         FROM json_each(nodes::JSON) 
         WHERE json_extract_string(value, '$.type') = 'function_definition'
        ) as functions,
        file_content
    FROM file_data
),
extracted AS (
    SELECT get_source(functions, file_content, 1) as results
    FROM function_nodes
)
SELECT 
    json_extract_string(func.value, '$.name') as function_name,
    json_extract(func.value, '$.start_line') as start_line,
    json_extract(func.value, '$.end_line') as end_line,
    json_extract_string(func.value, '$.source') as source_code
FROM extracted,
     json_each(results::JSON) as func;