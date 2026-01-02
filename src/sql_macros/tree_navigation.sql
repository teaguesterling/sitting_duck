-- Tree Navigation Macros
-- Functions for navigating AST parent-child relationships
-- These leverage the DFS pre-order node_id assignment for O(1) subtree queries

-- =============================================================================
-- Direct Relationship Helpers
-- =============================================================================

-- Get immediate children of a node
-- Usage: SELECT * FROM ast_children(my_ast_table, parent_node_id)
-- Note: First argument is a table name (string), second is the parent node_id
CREATE OR REPLACE MACRO ast_children(ast_table, parent_node_id) AS TABLE
    SELECT * FROM query_table(ast_table) WHERE parent_id = parent_node_id;

-- Get all descendants of a node (entire subtree)
-- Uses descendant_count for O(1) range-based lookup (nodes are in DFS pre-order)
-- Usage: SELECT * FROM ast_descendants(my_ast_table, ancestor_node_id)
CREATE OR REPLACE MACRO ast_descendants(ast_table, ancestor_node_id) AS TABLE
    WITH ancestor AS (
        SELECT node_id, descendant_count
        FROM query_table(ast_table)
        WHERE node_id = ancestor_node_id
    )
    SELECT a.*
    FROM query_table(ast_table) a, ancestor anc
    WHERE a.node_id > anc.node_id
      AND a.node_id <= anc.node_id + anc.descendant_count;

-- Get ancestors of a node (path from node to root)
-- Uses recursive CTE following parent_id upward
-- Usage: SELECT * FROM ast_ancestors(my_ast_table, child_node_id)
CREATE OR REPLACE MACRO ast_ancestors(ast_table, child_node_id) AS TABLE
    WITH RECURSIVE ancestors AS (
        SELECT * FROM query_table(ast_table) WHERE node_id = child_node_id
        UNION ALL
        SELECT a.*
        FROM query_table(ast_table) a
        JOIN ancestors anc ON a.node_id = anc.parent_id
        WHERE anc.parent_id IS NOT NULL
    )
    SELECT * FROM ancestors;

-- Get sibling nodes (same parent, excluding self)
-- Usage: SELECT * FROM ast_siblings(my_ast_table, target_node_id)
CREATE OR REPLACE MACRO ast_siblings(ast_table, target_node_id) AS TABLE
    WITH target AS (
        SELECT parent_id FROM query_table(ast_table) WHERE node_id = target_node_id
    )
    SELECT a.*
    FROM query_table(ast_table) a, target t
    WHERE a.parent_id = t.parent_id
      AND a.node_id != target_node_id;

-- =============================================================================
-- Line-Based Navigation
-- =============================================================================

-- Find all nodes that contain a specific line
-- Returns nodes ordered by specificity (smallest span first)
-- Usage: SELECT * FROM ast_containing_line(my_ast_table, line_number)
CREATE OR REPLACE MACRO ast_containing_line(ast_table, line_num) AS TABLE
    SELECT *
    FROM query_table(ast_table)
    WHERE start_line <= line_num AND end_line >= line_num
    ORDER BY (end_line - start_line), start_line;

-- Get all nodes within a line range
-- Usage: SELECT * FROM ast_in_range(my_ast_table, start_line, end_line)
CREATE OR REPLACE MACRO ast_in_range(ast_table, range_start, range_end) AS TABLE
    SELECT *
    FROM query_table(ast_table)
    WHERE start_line >= range_start AND end_line <= range_end;

-- =============================================================================
-- Scope-Aware Helpers
-- =============================================================================

-- Get all nodes inside a function, EXCLUDING nested function bodies
-- This is essential for accurate complexity analysis - avoids double-counting
-- nested function internals as part of the outer function's complexity.
-- Usage: SELECT * FROM ast_function_scope(my_ast_table, function_node_id)
CREATE OR REPLACE MACRO ast_function_scope(ast_table, func_node_id) AS TABLE
    WITH
        -- Get the function node itself
        func AS (
            SELECT node_id, descendant_count
            FROM query_table(ast_table)
            WHERE node_id = func_node_id
        ),
        -- Get all descendants of this function
        descendants AS (
            SELECT a.*
            FROM query_table(ast_table) a, func f
            WHERE a.node_id > f.node_id
              AND a.node_id <= f.node_id + f.descendant_count
        ),
        -- Find nested function definitions (excluding the function itself)
        nested_funcs AS (
            SELECT node_id, descendant_count
            FROM descendants
            WHERE is_function_definition(semantic_type)
        )
    -- Return descendants that are NOT inside any nested function
    SELECT d.*
    FROM descendants d
    WHERE NOT EXISTS (
        SELECT 1 FROM nested_funcs nf
        WHERE d.node_id > nf.node_id
          AND d.node_id <= nf.node_id + nf.descendant_count
    );

-- Get direct members of a class (methods, fields, nested classes)
-- Returns the member definition nodes themselves, not their internal contents.
-- Useful for class structure analysis without descending into method bodies.
-- Usage: SELECT name, type FROM ast_class_members(my_ast_table, class_node_id)
CREATE OR REPLACE MACRO ast_class_members(ast_table, class_node_id) AS TABLE
    WITH
        -- Get the class node
        class AS (
            SELECT node_id, descendant_count
            FROM query_table(ast_table)
            WHERE node_id = class_node_id
        ),
        -- Get all descendants of the class
        descendants AS (
            SELECT a.*
            FROM query_table(ast_table) a, class c
            WHERE a.node_id > c.node_id
              AND a.node_id <= c.node_id + c.descendant_count
        ),
        -- Find all named definition nodes within the class
        -- Excludes syntactic tokens like 'class'/'def' keywords where type=name
        all_defs AS (
            SELECT node_id, descendant_count, name, type, semantic_type,
                   start_line, end_line, depth, parent_id, peek, file_path, language
            FROM descendants
            WHERE is_definition(semantic_type)
              AND name IS NOT NULL AND name != ''
              AND type != name  -- Filter out keyword tokens (e.g., 'class' with name='class')
        )
    -- Return definitions that are NOT nested inside other definitions
    -- (i.e., direct members of the class, not locals inside methods)
    SELECT d.*
    FROM all_defs d
    WHERE NOT EXISTS (
        SELECT 1 FROM all_defs other
        WHERE other.node_id != d.node_id
          AND d.node_id > other.node_id
          AND d.node_id <= other.node_id + other.descendant_count
    );

