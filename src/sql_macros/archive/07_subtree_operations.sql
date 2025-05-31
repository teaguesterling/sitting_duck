-- ===================================
-- Subtree Operations Macros
-- ===================================
-- These macros leverage children_count and descendant_count for efficient tree operations

-- ===================================
-- ast_extract_subtree - Extract complete subtree for a given node
-- ===================================
-- Returns all nodes in the subtree rooted at the specified node_id
CREATE OR REPLACE MACRO ast_extract_subtree(nodes, root_node_id) AS (
    [
        node for node in COALESCE(nodes, [])
        if node.node_id >= root_node_id 
           AND node.node_id <= root_node_id + (
               SELECT descendant_count 
               FROM UNNEST(nodes) AS n 
               WHERE n.node_id = root_node_id
               LIMIT 1
           )
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO extract_subtree(nodes, root_node_id) AS (
    ast_extract_subtree(nodes, root_node_id)
);

-- ===================================
-- ast_filter_by_complexity - Filter nodes by subtree size
-- ===================================
-- Returns nodes whose subtree size falls within the specified range
CREATE OR REPLACE MACRO ast_filter_by_complexity(nodes, min_descendants := 0, max_descendants := NULL) AS (
    [
        node for node in COALESCE(nodes, [])
        if node.descendant_count >= min_descendants
           AND (max_descendants IS NULL OR node.descendant_count <= max_descendants)
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO filter_by_complexity(nodes, min_descendants := 0, max_descendants := NULL) AS (
    ast_filter_by_complexity(nodes, min_descendants := min_descendants, max_descendants := max_descendants)
);

-- ===================================
-- ast_find_leaves - Find all leaf nodes (no children)
-- ===================================
-- Returns nodes that have no children
CREATE OR REPLACE MACRO ast_find_leaves(nodes) AS (
    [
        node for node in COALESCE(nodes, [])
        if node.children_count = 0
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO find_leaves(nodes) AS (
    ast_find_leaves(nodes)
);

-- ===================================
-- ast_find_complex_nodes - Find nodes above complexity threshold
-- ===================================
-- Returns nodes with more than the specified number of descendants
CREATE OR REPLACE MACRO ast_find_complex_nodes(nodes, threshold := 10) AS (
    [
        node for node in COALESCE(nodes, [])
        if node.descendant_count > threshold
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO find_complex_nodes(nodes, threshold := 10) AS (
    ast_find_complex_nodes(nodes, threshold := threshold)
);

-- ===================================
-- ast_categorize_complexity - Categorize nodes by subtree complexity
-- ===================================
-- Returns nodes with complexity category added
CREATE OR REPLACE MACRO ast_categorize_complexity(nodes) AS (
    [
        {
            'node_id': node.node_id,
            'type': node.type,
            'name': node.name,
            'file_path': node.file_path,
            'start_line': node.start_line,
            'end_line': node.end_line,
            'depth': node.depth,
            'children_count': node.children_count,
            'descendant_count': node.descendant_count,
            'subtree_size': node.descendant_count + 1,
            'complexity': CASE 
                WHEN node.descendant_count = 0 THEN 'Leaf'
                WHEN node.descendant_count BETWEEN 1 AND 5 THEN 'Simple'
                WHEN node.descendant_count BETWEEN 6 AND 15 THEN 'Moderate'
                WHEN node.descendant_count BETWEEN 16 AND 50 THEN 'Complex'
                ELSE 'Very Complex'
            END
        }
        for node in COALESCE(nodes, [])
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO categorize_complexity(nodes) AS (
    ast_categorize_complexity(nodes)
);

-- ===================================
-- ast_get_immediate_children - Get direct children of a node
-- ===================================
-- Returns only the immediate children of the specified node
CREATE OR REPLACE MACRO ast_get_immediate_children(nodes, parent_node_id) AS (
    [
        node for node in COALESCE(nodes, [])
        if node.parent_id = parent_node_id
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO get_immediate_children(nodes, parent_node_id) AS (
    ast_get_immediate_children(nodes, parent_node_id)
);

-- ===================================
-- ast_subtree_stats - Get statistics for subtrees
-- ===================================
-- Returns summary statistics for subtrees rooted at nodes of specified type
CREATE OR REPLACE MACRO ast_subtree_stats(nodes, node_type := NULL) AS (
    [
        {
            'node_id': node.node_id,
            'type': node.type,
            'name': node.name,
            'location': node.file_path || ':' || node.start_line,
            'subtree_size': node.descendant_count + 1,
            'direct_children': node.children_count,
            'complexity_rank': CASE 
                WHEN node.descendant_count = 0 THEN 1
                WHEN node.descendant_count <= 5 THEN 2
                WHEN node.descendant_count <= 15 THEN 3
                WHEN node.descendant_count <= 50 THEN 4
                ELSE 5
            END
        }
        for node in COALESCE(nodes, [])
        if node_type IS NULL OR node.type = node_type
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO subtree_stats(nodes, node_type := NULL) AS (
    ast_subtree_stats(nodes, node_type := node_type)
);

-- ===================================
-- ast_find_largest_subtrees - Find N largest subtrees
-- ===================================
-- Returns the nodes with the largest subtrees (by descendant count)
-- Note: This macro returns all nodes, but they should be sorted by descendant_count DESC and limited
CREATE OR REPLACE MACRO ast_find_largest_subtrees(nodes, limit_count := 10) AS (
    [
        {
            'rank': ROW_NUMBER() OVER (ORDER BY node.descendant_count DESC),
            'node_id': node.node_id,
            'type': node.type,
            'name': node.name,
            'location': node.file_path || ':' || node.start_line,
            'subtree_size': node.descendant_count + 1,
            'descendant_count': node.descendant_count
        }
        for node in COALESCE(nodes, [])
        if node.descendant_count > 0
    ]
);

-- Chain method version  
CREATE OR REPLACE MACRO find_largest_subtrees(nodes, limit_count := 10) AS (
    ast_find_largest_subtrees(nodes, limit_count := limit_count)
);