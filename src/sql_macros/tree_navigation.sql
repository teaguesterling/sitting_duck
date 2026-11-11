-- Tree Navigation Macros
-- Functions for navigating AST parent-child relationships
-- These leverage the DFS pre-order node_id assignment for O(1) subtree queries

-- =============================================================================
-- Direct Relationship Helpers
-- =============================================================================

-- Get immediate children of a node
-- Usage: SELECT * FROM ast_children(my_ast_table, parent_node_id)
-- Note: First argument is a table name (string), second is the parent node_id
-- ----------------------------------------------------------------------------
-- ast_children: Direct children of a node in a parsed AST table
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_children(ast_table, parent_node_id) AS TABLE
    SELECT * FROM query_table(ast_table) WHERE parent_id = parent_node_id;

-- Get arguments of a function call node
-- Returns the actual argument nodes (excludes punctuation like parentheses and commas)
-- Usage: SELECT * FROM ast_call_arguments(my_ast_table, call_node_id)
-- ----------------------------------------------------------------------------
-- ast_call_arguments: Argument nodes of a call, from its argument_list child
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_call_arguments(ast_table, call_node_id) AS TABLE
    WITH
        -- Find the argument_list child of the call node
        arg_list AS (
            SELECT node_id as arg_list_id
            FROM query_table(ast_table)
            WHERE parent_id = call_node_id
              AND type IN ('argument_list', 'arguments', 'actual_parameters')
            LIMIT 1
        ),
        -- Get children of argument_list, excluding punctuation
        args AS (
            SELECT
                ROW_NUMBER() OVER (ORDER BY a.node_id) - 1 AS arg_position,
                a.node_id AS arg_node_id,
                a.name AS arg_name,
                a.type AS arg_type,
                a.peek AS arg_peek,
                a.semantic_type,
                a.start_line,
                a.end_line
            FROM query_table(ast_table) a, arg_list al
            WHERE a.parent_id = al.arg_list_id
              AND a.type NOT IN ('(', ')', ',', 'comment')
        )
    SELECT * FROM args;

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
-- There is no `language` parameter, because a parsed table carries its own --
-- which leaves the table as this form's ONLY parameter.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_definitions_from('my_ast')
--
-- Reads semantic_type, flags and name. A table whose semantic_type is NULL
-- returns no rows rather than raising: every comparison against NULL is NULL,
-- so the filter matches nothing. That is the NULL-definite convention issue #81
-- set for the selector attribute filters (see the NULL semantics note in
-- css_selectors.sql), and these macros follow it instead of validating columns.
--
-- IMPORTANT, and the reason to call this instead of writing the query yourself:
-- a definition query over a parsed AST table needs the FULL filter chain --
-- is_definition(semantic_type) AND is_construct(flags) AND name != ''. Using
-- is_definition() alone also matches keyword tokens (def, class, CREATE, ...),
-- because a keyword shares the semantic type of the construct it introduces,
-- so every definition comes back twice. This macro applies the whole chain.
-- The same warning is on ast_definitions below, for the path-taking form.
-- ----------------------------------------------------------------------------
-- ast_definitions_from: Every definition (function, class, variable, module) in a parsed AST table
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_definitions_from(ast_table) AS TABLE
    WITH ast AS (
        SELECT * FROM query_table(ast_table)
    )
    SELECT
        a.name,
        CASE
            WHEN is_function_definition(a.semantic_type) THEN 'function'
            WHEN is_class_definition(a.semantic_type) THEN 'class'
            WHEN is_variable_definition(a.semantic_type) THEN 'variable'
            WHEN is_module_definition(a.semantic_type) THEN 'module'
            WHEN is_type_definition(a.semantic_type) THEN 'type'
            ELSE 'other'
        END AS definition_type,
        a.language,
        a.file_path,
        a.start_line,
        a.end_line,
        a.node_id,
        a.type,
        a.semantic_type
    FROM ast a
    WHERE is_definition(a.semantic_type)
      AND is_construct(a.flags)
      AND a.name IS NOT NULL AND a.name != ''
    ORDER BY a.file_path, a.start_line;

-- Get all definitions (functions, classes, variables, etc.) with unified categories
-- Usage: SELECT * FROM ast_definitions('src/**/*.py')
-- Usage: SELECT * FROM ast_definitions('src/main.py', language := 'python')
--
-- IMPORTANT: When querying definitions from raw read_ast() output, always use the
-- full filter chain: is_definition(semantic_type) AND is_construct(flags) AND name != ''
-- Using is_definition() alone will include keyword tokens (def, class, CREATE, etc.)
-- as duplicates, since keywords share the semantic type of their parent construct.
-- ----------------------------------------------------------------------------
-- ast_definitions: Every definition (function, class, variable, module) in the source
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_definitions(source, language := NULL) AS TABLE
    -- Parse, then delegate to ast_definitions_from, which holds the projection,
    -- the filter chain and the ORDER BY.
    WITH __ast_definitions_src AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT * FROM ast_definitions_from('__ast_definitions_src');

-- Get all descendants of a node (entire subtree)
-- Uses descendant_count for O(1) range-based lookup (nodes are in DFS pre-order)
-- Usage: SELECT * FROM ast_descendants(my_ast_table, ancestor_node_id)
-- ----------------------------------------------------------------------------
-- ast_descendants: Every node beneath an ancestor, via its descendant_count range
-- ----------------------------------------------------------------------------
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
-- ----------------------------------------------------------------------------
-- ast_ancestors: The ancestor chain from a node up to the root
-- ----------------------------------------------------------------------------
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
-- ----------------------------------------------------------------------------
-- ast_siblings: Nodes sharing a parent with the target node
-- ----------------------------------------------------------------------------
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

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
-- There is no `language` parameter, because a parsed table carries its own.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_containing_line_from('my_ast', 42)
--
-- Reads start_line and end_line. A table whose line columns are NULL returns no
-- rows rather than raising -- the same NULL-definite convention as
-- ast_definitions_from above.
-- ----------------------------------------------------------------------------
-- ast_containing_line_from: Nodes of a parsed AST table whose source span covers a given line number
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_containing_line_from(ast_table, line_num) AS TABLE
    WITH ast AS (
        SELECT * FROM query_table(ast_table)
    )
    SELECT a.*
    FROM ast a
    WHERE a.start_line <= line_num AND a.end_line >= line_num
    ORDER BY (a.end_line - a.start_line), a.start_line;

-- Find all nodes that contain a specific line
-- Returns nodes ordered by specificity (smallest span first)
-- Usage: SELECT * FROM ast_containing_line('src/main.py', 42)
-- ----------------------------------------------------------------------------
-- ast_containing_line: Nodes whose source span covers a given line number
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_containing_line(source, line_num, language := NULL) AS TABLE
    -- Parse, then delegate to ast_containing_line_from, which holds the
    -- predicate and the specificity ORDER BY.
    WITH __ast_containing_line_src AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT * FROM ast_containing_line_from('__ast_containing_line_src', line_num);

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
-- There is no `language` parameter, because a parsed table carries its own.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_in_range_from('my_ast', 10, 20)
--
-- Reads start_line and end_line. A table whose line columns are NULL returns no
-- rows rather than raising -- the same NULL-definite convention as
-- ast_definitions_from above.
-- ----------------------------------------------------------------------------
-- ast_in_range_from: Nodes of a parsed AST table whose source span falls inside a line range
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_in_range_from(ast_table, range_start, range_end) AS TABLE
    WITH ast AS (
        SELECT * FROM query_table(ast_table)
    )
    SELECT a.*
    FROM ast a
    WHERE a.start_line >= range_start AND a.end_line <= range_end;

-- Get all nodes within a line range
-- Usage: SELECT * FROM ast_in_range('src/main.py', 10, 20)
-- ----------------------------------------------------------------------------
-- ast_in_range: Nodes whose source span falls inside a line range
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_in_range(source, range_start, range_end, language := NULL) AS TABLE
    -- Parse, then delegate to ast_in_range_from, which holds the predicate.
    WITH __ast_in_range_src AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT * FROM ast_in_range_from('__ast_in_range_src', range_start, range_end);

-- =============================================================================
-- Scope-Aware Helpers
-- =============================================================================

-- Get all nodes inside a function, EXCLUDING nested function bodies
-- This is essential for accurate complexity analysis - avoids double-counting
-- nested function internals as part of the outer function's complexity.
-- Usage: SELECT * FROM ast_function_scope(my_ast_table, function_node_id)
-- ----------------------------------------------------------------------------
-- ast_function_scope: Every node inside one function definition
-- ----------------------------------------------------------------------------
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
-- ----------------------------------------------------------------------------
-- ast_class_members: Members declared directly inside one class
-- ----------------------------------------------------------------------------
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
        -- Excludes syntactic tokens like 'class'/'def' keywords via flag check
        all_defs AS (
            SELECT node_id, descendant_count, name, type, semantic_type,
                   start_line, end_line, depth, parent_id, peek, file_path, language
            FROM descendants
            WHERE is_definition(semantic_type)
              AND is_construct(flags)
              AND name IS NOT NULL AND name != ''
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

-- Resolve nearest definition ancestor for each definition node
-- Walks up parent_id, skipping organizational/structural nodes (e.g., blocks),
-- until it finds an ancestor that is itself a definition.
-- Returns (node_id, def_name, kind, parent_def_name, parent_def_kind, parent_def_node_id)
-- Usage: SELECT * FROM ast_definition_parent('my_ast_table')
-- ----------------------------------------------------------------------------
-- ast_definition_parent: The nearest enclosing definition for every node in an AST table
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_definition_parent(ast_table) AS TABLE
    WITH RECURSIVE def_parent AS (
        -- Start: each definition node with its immediate parent
        SELECT
            d.node_id,
            d.name AS def_name,
            d.semantic_type AS def_type,
            d.parent_id AS current_parent_id,
            0 AS hops
        FROM query_table(ast_table) d
        WHERE is_definition(d.semantic_type)
          AND is_construct(d.flags)
          AND d.name IS NOT NULL AND d.name != ''
          AND d.depth >= 1

        UNION ALL

        -- Walk up: if current parent isn't a definition, go to its parent
        SELECT
            dp.node_id,
            dp.def_name,
            dp.def_type,
            p.parent_id AS current_parent_id,
            dp.hops + 1
        FROM def_parent dp
        JOIN query_table(ast_table) p ON dp.current_parent_id = p.node_id
        WHERE NOT is_definition(p.semantic_type) AND dp.hops < 10
    )
    SELECT
        dp.node_id,
        dp.def_name,
        semantic_type_to_string(dp.def_type) AS kind,
        p.name AS parent_def_name,
        semantic_type_to_string(p.semantic_type) AS parent_def_kind,
        p.node_id AS parent_def_node_id
    FROM def_parent dp
    JOIN query_table(ast_table) p ON dp.current_parent_id = p.node_id
    WHERE is_definition(p.semantic_type);
