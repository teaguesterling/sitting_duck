-- Test the children_count and descendant_count implementation
.load build/release/duckdb_ast.duckdb_extension

-- Load and examine the structure from test data
CREATE TABLE ast_data AS 
SELECT * FROM read_ast_objects('test/data/javascript/count_test.js', 'javascript');

-- Check that we have the new fields
DESCRIBE SELECT * FROM (SELECT UNNEST(nodes) FROM ast_data);

-- Show nodes with their counts
SELECT 
    node_id,
    type,
    name,
    depth,
    children_count,
    descendant_count
FROM ast_data
CROSS JOIN UNNEST(nodes) AS n
ORDER BY node_id
LIMIT 20;

-- Verify leaf nodes have children_count = 0
SELECT 
    COUNT(*) as leaf_nodes,
    COUNT(*) FILTER (WHERE descendant_count = 0) as zero_descendants
FROM ast_data
CROSS JOIN UNNEST(nodes) AS n
WHERE children_count = 0;

-- Find the most complex nodes
SELECT 
    node_id,
    type,
    name,
    descendant_count
FROM ast_data
CROSS JOIN UNNEST(nodes) AS n
WHERE descendant_count > 0
ORDER BY descendant_count DESC
LIMIT 5;

-- Verify parent-child relationships
WITH node_data AS (
    SELECT * FROM ast_data CROSS JOIN UNNEST(nodes) AS n
)
SELECT 
    p.node_id as parent_id,
    p.type as parent_type,
    p.children_count as stated_children,
    COUNT(c.node_id) as actual_children
FROM node_data p
LEFT JOIN node_data c ON c.parent_id = p.node_id
GROUP BY p.node_id, p.type, p.children_count
HAVING p.children_count != COUNT(c.node_id);