LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Create test data
CREATE OR REPLACE TEMP TABLE test_data AS
SELECT 
    file_path,
    nodes as nodes_json,
    -- Simple struct conversion
    (SELECT ARRAY_AGG(STRUCT_PACK(
        node_id := CAST(json_extract(node, '$.id') AS INTEGER),
        type := json_extract_string(node, '$.type'),
        name := json_extract_string(node, '$.name'),
        depth := CAST(json_extract(node, '$.depth') AS INTEGER),
        file_path := json_extract_string(node, '$.file_path'),
        start_line := CAST(json_extract(node, '$.start.line') AS INTEGER),
        start_column := CAST(json_extract(node, '$.start.column') AS INTEGER),
        end_line := CAST(json_extract(node, '$.end.line') AS INTEGER),
        end_column := CAST(json_extract(node, '$.end.column') AS INTEGER)
    ) ORDER BY CAST(json_extract(node, '$.id') AS INTEGER))
    FROM json_each(nodes::JSON) AS t(node)
    ) as nodes_struct
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Test: Filter by type comparison
SELECT 'JSON approach' as method, COUNT(*) as function_count
FROM test_data,
     json_each(nodes_json::JSON) as je
WHERE json_extract_string(je.value, '$.type') = 'function_definition';

SELECT 'Struct approach' as method, COUNT(*) as function_count  
FROM test_data,
     UNNEST(nodes_struct)
WHERE unnest.type = 'function_definition';

-- Test: Show struct contents work
SELECT unnest.type, unnest.name, unnest.depth
FROM test_data,
     UNNEST(nodes_struct) as unnest
WHERE unnest.type = 'function_definition'
LIMIT 3;
