-- Tree navigation helper functions for AST monad objects
-- These work with the AST struct returned by read_ast_objects (when it existed)

-- Get immediate children of a node from AST monad
-- Returns AST nodes that are children of the specified node_id
CREATE OR REPLACE TEMPORARY MACRO monad_get_children(ast, target_node_id) AS (
    [
        node 
        for node in ast.nodes 
        if node.parent_id = target_node_id
    ]
);

-- Get all descendants of a node (including the node itself) from AST monad
-- Uses descendant_count for efficient extraction
CREATE OR REPLACE TEMPORARY MACRO monad_get_descendants(ast, target_node_id) AS (
    WITH target AS (
        SELECT 
            node_id as tid,
            descendant_count as desc_count
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    )
    [
        node 
        for node in ast.nodes 
        if node.node_id >= target.tid 
           AND node.node_id <= target.tid + target.desc_count
    ]
    FROM target
);

-- Get n descendants starting from a node
CREATE OR REPLACE TEMPORARY MACRO monad_get_n_descendants(ast, target_node_id, n) AS (
    WITH target AS (
        SELECT 
            node_id as tid,
            descendant_count as desc_count
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    )
    [
        node 
        for node in ast.nodes 
        if node.node_id >= target.tid 
           AND node.node_id <= target.tid + LEAST(n - 1, target.desc_count)
    ]
    FROM target
);

-- Get the subtree rooted at a node (excluding the root node itself)
CREATE OR REPLACE TEMPORARY MACRO monad_get_subtree(ast, target_node_id) AS (
    WITH target AS (
        SELECT 
            node_id as tid,
            descendant_count as desc_count
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    )
    [
        node 
        for node in ast.nodes 
        if node.node_id > target.tid 
           AND node.node_id <= target.tid + target.desc_count
    ]
    FROM target
);

-- Get siblings of a node (nodes with same parent)
CREATE OR REPLACE TEMPORARY MACRO monad_get_siblings(ast, target_node_id) AS (
    WITH target AS (
        SELECT parent_id as pid
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    )
    [
        node 
        for node in ast.nodes 
        if node.parent_id = target.pid
    ]
    FROM target
);

-- Get the parent node
CREATE OR REPLACE TEMPORARY MACRO monad_get_parent(ast, target_node_id) AS (
    WITH target AS (
        SELECT parent_id as pid
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    )
    (
        SELECT node
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target.pid
        LIMIT 1
    ).node
    FROM target
);

-- Find the nearest ancestor of a specific type
CREATE OR REPLACE TEMPORARY MACRO monad_find_ancestor_of_type(ast, target_node_id, target_type) AS (
    WITH RECURSIVE ancestors AS (
        -- Start with parent
        SELECT 
            n.parent_id as ancestor_id
        FROM (SELECT unnest(ast.nodes) as node) n
        WHERE n.node_id = target_node_id AND n.parent_id IS NOT NULL
        
        UNION
        
        -- Recursively get parents
        SELECT 
            n.parent_id
        FROM (SELECT unnest(ast.nodes) as node) n
        JOIN ancestors a ON n.node_id = a.ancestor_id
        WHERE n.parent_id IS NOT NULL
    )
    (
        SELECT node
        FROM (SELECT unnest(ast.nodes) as node)
        JOIN ancestors a ON node.node_id = a.ancestor_id
        WHERE node.type = target_type
        ORDER BY node.depth DESC
        LIMIT 1
    ).node
);

-- Extract a specific node by ID from the monad
CREATE OR REPLACE TEMPORARY MACRO monad_get_node(ast, target_node_id) AS (
    (
        SELECT node
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    ).node
);

-- Get nodes of a specific type
CREATE OR REPLACE TEMPORARY MACRO monad_get_nodes_of_type(ast, target_type) AS (
    [
        node 
        for node in ast.nodes 
        if node.type = target_type
    ]
);

-- Get nodes with a specific semantic type
CREATE OR REPLACE TEMPORARY MACRO monad_get_nodes_by_semantic_type(ast, sem_type) AS (
    [
        node 
        for node in ast.nodes 
        if node.semantic_type = sem_type
    ]
);

-- ===================================
-- HYBRID FUNCTIONS
-- ===================================
-- These return results that work well with both styles

-- Count children of a node
CREATE OR REPLACE TEMPORARY MACRO monad_count_children(ast, target_node_id) AS (
    (
        SELECT node.children_count
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    ).children_count
);

-- Count descendants of a node  
CREATE OR REPLACE TEMPORARY MACRO monad_count_descendants(ast, target_node_id) AS (
    (
        SELECT node.descendant_count
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    ).descendant_count
);

-- Get depth of a node
CREATE OR REPLACE TEMPORARY MACRO monad_get_depth(ast, target_node_id) AS (
    (
        SELECT node.depth
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = target_node_id
        LIMIT 1
    ).depth
);

-- Check if node is ancestor of another
CREATE OR REPLACE TEMPORARY MACRO monad_is_ancestor(ast, potential_ancestor_id, node_id) AS (
    WITH node_info AS (
        SELECT 
            node_id,
            -- Path from root stored as ancestor chain
            node_id BETWEEN potential_ancestor_id + 1 
                    AND potential_ancestor_id + (
                        SELECT descendant_count 
                        FROM (SELECT unnest(ast.nodes) as n) 
                        WHERE n.node_id = potential_ancestor_id
                    ) as is_descendant
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.node_id = node_id
    )
    SELECT is_descendant FROM node_info
);

-- ===================================
-- EXAMPLES OF MONAD-STYLE USAGE
-- ===================================

-- Example: Extract all function names from an AST monad
CREATE OR REPLACE TEMPORARY MACRO monad_extract_function_names(ast) AS (
    [
        struct_pack(
            name := child.name,
            parent_name := node.name,
            line := node.start_line
        )
        for node in monad_get_nodes_by_semantic_type(ast, 112)  -- DEFINITION_FUNCTION
        for child in monad_get_children(ast, node.node_id)
        if child.type IN ('identifier', 'qualified_identifier')
    ]
);

-- Example: Get function complexity using monad style
CREATE OR REPLACE TEMPORARY MACRO monad_function_complexity(ast, func_node_id) AS (
    WITH descendants AS (
        SELECT unnest(monad_get_descendants(ast, func_node_id)) as node
    )
    SELECT 
        COUNT(CASE WHEN node.type IN ('if_statement', 'for_statement', 'while_statement') THEN 1 END) as cyclomatic_complexity,
        COUNT(CASE WHEN node.type = 'call_expression' THEN 1 END) as call_count,
        MAX(node.depth) - MIN(node.depth) as max_nesting_depth
    FROM descendants
);