-- Simple JSON vs Struct performance comparison

-- Create test data
CREATE TEMP TABLE test_data AS
SELECT 
    file_path,
    nodes as nodes_json,
    -- Simple struct conversion (without descendant counts for now)
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

-- Test 1: Filter by type
.timer on
.print "JSON approach - filter by type:"
SELECT COUNT(*) as json_functions
FROM test_data,
     json_each(nodes_json::JSON) as je
WHERE json_extract_string(je.value, '$.type') = 'function_definition';

.print "Struct approach - filter by type:"
SELECT COUNT(*) as struct_functions
FROM test_data,
     UNNEST(nodes_struct) as node
WHERE node.type = 'function_definition';

-- Test 2: Aggregate by type  
.print "JSON approach - aggregate by type:"
SELECT 
    json_extract_string(je.value, '$.type') as type,
    COUNT(*) as count
FROM test_data,
     json_each(nodes_json::JSON) as je
GROUP BY json_extract_string(je.value, '$.type')
ORDER BY count DESC
LIMIT 5;

.print "Struct approach - aggregate by type:"
SELECT 
    node.type,
    COUNT(*) as count
FROM test_data,
     UNNEST(nodes_struct) as node
GROUP BY node.type
ORDER BY count DESC
LIMIT 5;

-- Test 3: Complex filter
.print "JSON approach - complex filter:"
SELECT COUNT(*) as deep_identifiers
FROM test_data,
     json_each(nodes_json::JSON) as je
WHERE CAST(json_extract(je.value, '$.depth') AS INTEGER) > 2
  AND json_extract_string(je.value, '$.type') = 'identifier';

.print "Struct approach - complex filter:"
SELECT COUNT(*) as deep_identifiers
FROM test_data,
     UNNEST(nodes_struct) as node
WHERE node.depth > 2
  AND node.type = 'identifier';

.timer off