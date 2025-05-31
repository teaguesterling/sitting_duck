-- Calculate descendant counts for DFS-ordered nodes
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Function to calculate descendant count for each node
CREATE OR REPLACE MACRO calculate_descendant_counts(nodes_json VARCHAR)
RETURNS TABLE(node_id INTEGER, descendant_count INTEGER) AS (
    WITH RECURSIVE node_hierarchy AS (
        -- Base: get all nodes with their info
        SELECT 
            CAST(json_extract(node, '$.id') AS INTEGER) as node_id,
            CAST(json_extract(node, '$.depth') AS INTEGER) as depth,
            json_extract(node, '$.children') as children_json
        FROM json_each(nodes_json::JSON) AS t(node)
    ),
    descendant_calc AS (
        -- For each node, count all nodes that come after it with greater depth
        -- until we reach a node at the same or lesser depth
        SELECT 
            n1.node_id,
            COUNT(n2.node_id) as descendant_count
        FROM node_hierarchy n1
        LEFT JOIN node_hierarchy n2 ON (
            n2.node_id > n1.node_id  -- Comes after in DFS order
            AND n2.depth > n1.depth  -- Is deeper (descendant)
            AND NOT EXISTS (
                -- No intervening node at same or lesser depth
                SELECT 1 FROM node_hierarchy n3 
                WHERE n3.node_id > n1.node_id 
                  AND n3.node_id < n2.node_id
                  AND n3.depth <= n1.depth
            )
        )
        GROUP BY n1.node_id
    )
    SELECT node_id, descendant_count FROM descendant_calc
);

-- Test the calculation
WITH test_nodes AS (
    SELECT nodes FROM read_ast_objects('test/data/python/simple.py', 'python')
),
desc_counts AS (
    SELECT * FROM calculate_descendant_counts((SELECT nodes FROM test_nodes))
)
SELECT 
    dc.node_id,
    json_extract_string(je.value, '$.type') as type,
    json_extract_string(je.value, '$.name') as name,
    CAST(json_extract(je.value, '$.depth') AS INTEGER) as depth,
    dc.descendant_count
FROM test_nodes,
     json_each(nodes::JSON) as je,
     desc_counts dc
WHERE CAST(json_extract(je.value, '$.id') AS INTEGER) = dc.node_id
ORDER BY dc.node_id
LIMIT 10;