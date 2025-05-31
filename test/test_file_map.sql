-- Test creating a file content map
-- Since read_text doesn't support dynamic paths, we need a different approach

-- Option 1: Create a temporary table with file contents
CREATE TEMP TABLE file_cache AS
WITH unique_files AS (
    SELECT DISTINCT json_extract_string(value, '$.file_path') as file_path
    FROM (
        SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
    ), json_each(nodes::JSON)
    WHERE json_extract_string(value, '$.file_path') IS NOT NULL
)
-- For now, manually handle known files (in real use, this would need to be dynamic)
SELECT 
    'test/data/python/simple.py' as file_path,
    (SELECT content FROM read_text('test/data/python/simple.py')) as content;

-- Now we can join with this table
WITH nodes_sample AS (
    SELECT 
        value as node,
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
    ns.name,
    ns.file_path,
    ns.start_line,
    ns.end_line,
    ast_get_source(fc.content, ns.start_line, ns.end_line, 1) as source
FROM nodes_sample ns
JOIN file_cache fc ON ns.file_path = fc.file_path;