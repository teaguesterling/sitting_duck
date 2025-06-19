-- Tree navigation helper functions for DuckDB AST extension
-- These leverage the depth-first traversal order and descendant_count for efficient subtree extraction

-- Get immediate children of a node
CREATE OR REPLACE TEMPORARY MACRO get_children(ast_table, node_id) AS TABLE
    SELECT * FROM ast_table
    WHERE parent_id = node_id
    ORDER BY sibling_index;

-- Get all descendants of a node (including the node itself)
-- Uses descendant_count for efficient extraction in depth-first order
CREATE OR REPLACE TEMPORARY MACRO get_descendants(ast_table, node_id) AS TABLE
    WITH target_node AS (
        SELECT node_id as target_id, descendant_count, depth as target_depth
        FROM ast_table
        WHERE node_id = node_id
        LIMIT 1
    )
    SELECT ast.* 
    FROM ast_table ast, target_node
    WHERE ast.node_id >= target_node.target_id
      AND ast.node_id <= target_node.target_id + target_node.descendant_count
    ORDER BY ast.node_id;

-- Get n descendants starting from a node (useful for bounded subtree extraction)
CREATE OR REPLACE TEMPORARY MACRO get_n_descendants(ast_table, node_id, n) AS TABLE
    WITH target_node AS (
        SELECT node_id as target_id, descendant_count, depth as target_depth
        FROM ast_table
        WHERE node_id = node_id
        LIMIT 1
    )
    SELECT ast.* 
    FROM ast_table ast, target_node
    WHERE ast.node_id >= target_node.target_id
      AND ast.node_id <= target_node.target_id + LEAST(n - 1, target_node.descendant_count)
    ORDER BY ast.node_id;

-- Get the subtree rooted at a node (excluding the root node itself)
CREATE OR REPLACE TEMPORARY MACRO get_subtree(ast_table, node_id) AS TABLE
    WITH target_node AS (
        SELECT node_id as target_id, descendant_count, depth as target_depth
        FROM ast_table
        WHERE node_id = node_id
        LIMIT 1
    )
    SELECT ast.* 
    FROM ast_table ast, target_node
    WHERE ast.node_id > target_node.target_id
      AND ast.node_id <= target_node.target_id + target_node.descendant_count
    ORDER BY ast.node_id;

-- Get siblings of a node (nodes with same parent)
CREATE OR REPLACE TEMPORARY MACRO get_siblings(ast_table, node_id) AS TABLE
    WITH target_node AS (
        SELECT parent_id as target_parent
        FROM ast_table
        WHERE node_id = node_id
        LIMIT 1
    )
    SELECT ast.* 
    FROM ast_table ast, target_node
    WHERE ast.parent_id = target_node.target_parent
    ORDER BY ast.sibling_index;

-- Get the parent of a node
CREATE OR REPLACE TEMPORARY MACRO get_parent(ast_table, node_id) AS TABLE
    WITH target_node AS (
        SELECT parent_id as target_parent
        FROM ast_table
        WHERE node_id = node_id
        LIMIT 1
    )
    SELECT ast.* 
    FROM ast_table ast, target_node
    WHERE ast.node_id = target_node.target_parent;

-- Get ancestors of a node (all nodes from root to parent)
CREATE OR REPLACE TEMPORARY MACRO get_ancestors(ast_table, node_id) AS TABLE
    WITH RECURSIVE ancestors AS (
        -- Start with the target node's parent
        SELECT parent_id as ancestor_id
        FROM ast_table
        WHERE node_id = node_id AND parent_id IS NOT NULL
        
        UNION
        
        -- Recursively get parents
        SELECT ast.parent_id
        FROM ast_table ast
        JOIN ancestors a ON ast.node_id = a.ancestor_id
        WHERE ast.parent_id IS NOT NULL
    )
    SELECT ast.*
    FROM ast_table ast
    JOIN ancestors a ON ast.node_id = a.ancestor_id
    ORDER BY ast.depth;

-- Get the path from root to a node (including the node)
CREATE OR REPLACE TEMPORARY MACRO get_path_to_node(ast_table, node_id) AS TABLE
    WITH RECURSIVE path AS (
        -- Start with the target node
        SELECT node_id as path_node_id, parent_id, depth
        FROM ast_table
        WHERE node_id = node_id
        
        UNION
        
        -- Recursively get parents
        SELECT ast.node_id, ast.parent_id, ast.depth
        FROM ast_table ast
        JOIN path p ON ast.node_id = p.parent_id
    )
    SELECT ast.*
    FROM ast_table ast
    JOIN path p ON ast.node_id = p.path_node_id
    ORDER BY ast.depth;

-- Find the nearest ancestor of a specific type
CREATE OR REPLACE TEMPORARY MACRO find_nearest_ancestor_of_type(ast_table, node_id, target_type) AS TABLE
    WITH RECURSIVE ancestors AS (
        -- Start with the target node's parent
        SELECT parent_id as ancestor_id
        FROM ast_table
        WHERE node_id = node_id AND parent_id IS NOT NULL
        
        UNION
        
        -- Recursively get parents
        SELECT ast.parent_id
        FROM ast_table ast
        JOIN ancestors a ON ast.node_id = a.ancestor_id
        WHERE ast.parent_id IS NOT NULL
    )
    SELECT ast.*
    FROM ast_table ast
    JOIN ancestors a ON ast.node_id = a.ancestor_id
    WHERE ast.type = target_type
    ORDER BY ast.depth DESC
    LIMIT 1;

-- Extract a bounded context around a node (n levels up and down)
CREATE OR REPLACE TEMPORARY MACRO get_context(ast_table, node_id, levels_up, levels_down) AS TABLE
    WITH target_node AS (
        SELECT node_id as target_id, depth as target_depth, parent_id, descendant_count
        FROM ast_table
        WHERE node_id = node_id
        LIMIT 1
    ),
    -- Get ancestors up to levels_up
    ancestors AS (
        SELECT ast.*
        FROM ast_table ast, target_node
        WHERE ast.depth >= target_node.target_depth - levels_up
          AND ast.depth < target_node.target_depth
          AND ast.node_id IN (
              SELECT node_id FROM get_path_to_node(ast_table, target_node.target_id)
          )
    ),
    -- Get descendants up to levels_down  
    descendants AS (
        SELECT ast.*
        FROM ast_table ast, target_node
        WHERE ast.node_id >= target_node.target_id
          AND ast.node_id <= target_node.target_id + target_node.descendant_count
          AND ast.depth <= target_node.target_depth + levels_down
    )
    SELECT * FROM ancestors
    UNION ALL
    SELECT * FROM descendants
    ORDER BY node_id;