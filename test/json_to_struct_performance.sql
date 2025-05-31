-- Performance comparison: JSON vs Struct approach
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- JSON to Struct conversion macro
CREATE OR REPLACE MACRO json_to_ast_struct(nodes_json VARCHAR) 
RETURNS STRUCT(
    node_id INTEGER,
    type VARCHAR, 
    name VARCHAR,
    depth INTEGER,
    descendant_count INTEGER,
    file_path VARCHAR,
    start_line INTEGER,
    start_column INTEGER,
    end_line INTEGER,
    end_column INTEGER
)[] AS (
    WITH nodes_with_desc AS (
        SELECT 
            node,
            CAST(json_extract(node, '$.id') AS INTEGER) as node_id,
            -- Calculate descendant count: we'll need to implement this properly
            -- For now, use children array length as approximation
            COALESCE(json_array_length(json_extract(node, '$.children')::JSON), 0) as descendant_count
        FROM json_each(nodes_json::JSON) AS t(node)
    )
    SELECT ARRAY_AGG(STRUCT_PACK(
        node_id := CAST(json_extract(node, '$.id') AS INTEGER),
        type := json_extract_string(node, '$.type'),
        name := json_extract_string(node, '$.name'),
        depth := CAST(json_extract(node, '$.depth') AS INTEGER),
        descendant_count := descendant_count,
        file_path := json_extract_string(node, '$.file_path'),
        start_line := CAST(json_extract(node, '$.start.line') AS INTEGER),
        start_column := CAST(json_extract(node, '$.start.column') AS INTEGER),
        end_line := CAST(json_extract(node, '$.end.line') AS INTEGER),
        end_column := CAST(json_extract(node, '$.end.column') AS INTEGER)
    ) ORDER BY node_id)
    FROM nodes_with_desc
);

-- Test data setup
CREATE TEMP TABLE test_data AS
SELECT 
    file_path,
    nodes as nodes_json,
    json_to_ast_struct(nodes) as nodes_struct
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Performance Test 1: Filter by type
-- JSON approach
.timer on
SELECT COUNT(*) as json_functions
FROM test_data,
     json_each(nodes_json::JSON) as je
WHERE json_extract_string(je.value, '$.type') = 'function_definition';

-- Struct approach  
SELECT COUNT(*) as struct_functions
FROM test_data,
     UNNEST(nodes_struct) as node
WHERE node.type = 'function_definition';

-- Performance Test 2: Get function names
-- JSON approach
SELECT json_extract_string(je.value, '$.name') as name
FROM test_data,
     json_each(nodes_json::JSON) as je
WHERE json_extract_string(je.value, '$.type') = 'function_definition'
  AND json_extract_string(je.value, '$.name') IS NOT NULL;

-- Struct approach
SELECT node.name
FROM test_data,
     UNNEST(nodes_struct) as node
WHERE node.type = 'function_definition'
  AND node.name IS NOT NULL;

-- Performance Test 3: Aggregate by type
-- JSON approach
SELECT 
    json_extract_string(je.value, '$.type') as type,
    COUNT(*) as count
FROM test_data,
     json_each(nodes_json::JSON) as je
GROUP BY json_extract_string(je.value, '$.type')
ORDER BY count DESC;

-- Struct approach
SELECT 
    node.type,
    COUNT(*) as count
FROM test_data,
     UNNEST(nodes_struct) as node
GROUP BY node.type
ORDER BY count DESC;

-- Performance Test 4: Complex filtering
-- JSON approach
SELECT COUNT(*) as json_deep_nodes
FROM test_data,
     json_each(nodes_json::JSON) as je
WHERE CAST(json_extract(je.value, '$.depth') AS INTEGER) > 2
  AND json_extract_string(je.value, '$.type') IN ('identifier', 'call', 'string');

-- Struct approach  
SELECT COUNT(*) as struct_deep_nodes
FROM test_data,
     UNNEST(nodes_struct) as node
WHERE node.depth > 2
  AND node.type IN ('identifier', 'call', 'string');

.timer off