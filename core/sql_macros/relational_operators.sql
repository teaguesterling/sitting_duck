-- =============================================================================
-- Relational Operators for AST Querying (Issue #57)
-- =============================================================================
--
-- Macros for structural relationship queries between AST nodes.
-- These use descendant_count range-checks for O(1) subtree membership.
--
-- Unlike the four semantic function categories (ast_get_*, ast_to_*,
-- ast_find_*, ast_extract_*), relational operators are predicates that
-- filter nodes by structural relationships. They form a fifth category:
-- ast_has/ast_inside/ast_precedes/ast_follows answer "is X related to Y?"
-- rather than extracting or transforming AST data.
--
-- Each operator comes in two shapes. The path-taking form takes a file path or
-- glob and calls read_ast() internally. The `_from` form takes the NAME of a
-- table that already holds a parsed AST and reads it with query_table(), so a
-- corpus is parsed once and queried many times -- and so these operators work
-- over an AST produced elsewhere. The path-taking form is a thin wrapper that
-- parses and then delegates, exactly as ast_select delegates to ast_select_from.
-- =============================================================================

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
-- There is no `language` parameter, because a parsed table carries its own.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_has_from('my_ast', 'function_definition', 'return_statement')
-- ----------------------------------------------------------------------------
-- ast_has_from: Nodes of one type that contain a node of another type, in a parsed AST table
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_has_from(
    ast_table,
    ancestor_type,
    descendant_type,
    descendant_name := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM query_table(ast_table)
    )
    SELECT a.*
    FROM ast a
    WHERE a.type = ancestor_type
      AND EXISTS (
          SELECT 1
          FROM ast d
          WHERE d.file_path = a.file_path
            AND d.node_id > a.node_id
            AND d.node_id <= a.node_id + a.descendant_count
            AND d.type = descendant_type
            AND (descendant_name IS NULL OR d.name = descendant_name)
      );

-- Check if ancestor nodes contain a matching descendant at any depth.
-- Uses descendant_count range-check for O(1) subtree membership.
-- Usage: SELECT * FROM ast_has('src/**/*.py', 'function_definition', 'return_statement')
-- Usage: SELECT * FROM ast_has('src/main.py', 'function_definition', 'call', 'execute')
-- ----------------------------------------------------------------------------
-- ast_has: Nodes of one type that contain a node of another type
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_has(
    source,
    ancestor_type,
    descendant_type,
    descendant_name := NULL,
    language := NULL
) AS TABLE
    -- Parse, then delegate to ast_has_from, which holds the predicate.
    WITH __ast_has_src AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT * FROM ast_has_from('__ast_has_src', ancestor_type, descendant_type,
                               descendant_name := descendant_name);

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
-- There is no `language` parameter, because a parsed table carries its own.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_not_has_from('my_ast', 'function_definition', 'return_statement')
-- ----------------------------------------------------------------------------
-- ast_not_has_from: Nodes of one type that do NOT contain a node of another type, in a parsed AST table
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_not_has_from(
    ast_table,
    ancestor_type,
    descendant_type,
    descendant_name := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM query_table(ast_table)
    )
    SELECT a.*
    FROM ast a
    WHERE a.type = ancestor_type
      AND NOT EXISTS (
          SELECT 1
          FROM ast d
          WHERE d.file_path = a.file_path
            AND d.node_id > a.node_id
            AND d.node_id <= a.node_id + a.descendant_count
            AND d.type = descendant_type
            AND (descendant_name IS NULL OR d.name = descendant_name)
      );

-- Negation of ast_has: find ancestor nodes that do NOT contain a descendant type.
-- Usage: SELECT * FROM ast_not_has('src/**/*.py', 'function_definition', 'return_statement')
-- Usage: SELECT * FROM ast_not_has('src/main.py', 'function_definition', 'call', 'execute')
-- ----------------------------------------------------------------------------
-- ast_not_has: Nodes of one type that do NOT contain a node of another type
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_not_has(
    source,
    ancestor_type,
    descendant_type,
    descendant_name := NULL,
    language := NULL
) AS TABLE
    -- Parse, then delegate to ast_not_has_from, which holds the predicate.
    WITH __ast_not_has_src AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT * FROM ast_not_has_from('__ast_not_has_src', ancestor_type, descendant_type,
                                   descendant_name := descendant_name);

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
-- There is no `language` parameter, because a parsed table carries its own.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_inside_from('my_ast', 'call', 'function_definition', 'load_sql')
-- ----------------------------------------------------------------------------
-- ast_inside_from: Nodes of one type that sit inside a node of another type, in a parsed AST table
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_inside_from(
    ast_table,
    descendant_type,
    ancestor_type,
    ancestor_name := NULL,
    descendant_name := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM query_table(ast_table)
    )
    SELECT d.*
    FROM ast d
    WHERE d.type = descendant_type
      AND (descendant_name IS NULL OR d.name = descendant_name)
      AND EXISTS (
          SELECT 1
          FROM ast a
          WHERE a.file_path = d.file_path
            AND a.type = ancestor_type
            AND (ancestor_name IS NULL OR a.name = ancestor_name)
            AND d.node_id > a.node_id
            AND d.node_id <= a.node_id + a.descendant_count
      );

-- Find descendant nodes that are inside an ancestor of a given type.
-- Inverse of ast_has: returns the descendants, not the ancestors.
-- Uses descendant_count range-check for O(1) subtree membership.
-- Usage: SELECT * FROM ast_inside('src/**/*.py', 'call', 'function_definition', 'load_sql')
-- Usage: SELECT * FROM ast_inside('src/main.py', 'return_statement', 'function_definition')
-- ----------------------------------------------------------------------------
-- ast_inside: Nodes of one type that sit inside a node of another type
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_inside(
    source,
    descendant_type,
    ancestor_type,
    ancestor_name := NULL,
    descendant_name := NULL,
    language := NULL
) AS TABLE
    -- Parse, then delegate to ast_inside_from, which holds the predicate.
    WITH __ast_inside_src AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT * FROM ast_inside_from('__ast_inside_src', descendant_type, ancestor_type,
                                  ancestor_name := ancestor_name,
                                  descendant_name := descendant_name);

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
-- There is no `language` parameter, because a parsed table carries its own.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_precedes_from('my_ast', 'comment', 'function_definition')
-- ----------------------------------------------------------------------------
-- ast_precedes_from: Nodes of one type that appear before a node of another type, in a parsed AST table
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_precedes_from(
    ast_table,
    node_type,
    before_type,
    before_name := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM query_table(ast_table)
    )
    SELECT n.*
    FROM ast n
    WHERE n.type = node_type
      AND EXISTS (
          SELECT 1
          FROM ast b
          WHERE b.file_path = n.file_path
            AND b.parent_id = n.parent_id
            AND b.type = before_type
            AND (before_name IS NULL OR b.name = before_name)
            AND n.sibling_index < b.sibling_index
      );

-- Find sibling nodes that precede a node of a given type.
-- Returns nodes that come before (lower sibling_index) a sibling of target_type.
-- Usage: SELECT * FROM ast_precedes('src/**/*.py', 'comment', 'function_definition')
-- ----------------------------------------------------------------------------
-- ast_precedes: Nodes of one type that appear before a node of another type
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_precedes(
    source,
    node_type,
    before_type,
    before_name := NULL,
    language := NULL
) AS TABLE
    -- Parse, then delegate to ast_precedes_from, which holds the predicate.
    WITH __ast_precedes_src AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT * FROM ast_precedes_from('__ast_precedes_src', node_type, before_type,
                                    before_name := before_name);

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
-- There is no `language` parameter, because a parsed table carries its own.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_follows_from('my_ast', 'expression_statement', 'function_definition')
-- ----------------------------------------------------------------------------
-- ast_follows_from: Nodes of one type that appear after a node of another type, in a parsed AST table
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_follows_from(
    ast_table,
    node_type,
    after_type,
    after_name := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM query_table(ast_table)
    )
    SELECT n.*
    FROM ast n
    WHERE n.type = node_type
      AND EXISTS (
          SELECT 1
          FROM ast a
          WHERE a.file_path = n.file_path
            AND a.parent_id = n.parent_id
            AND a.type = after_type
            AND (after_name IS NULL OR a.name = after_name)
            AND n.sibling_index > a.sibling_index
      );

-- Find sibling nodes that follow a node of a given type.
-- Returns nodes that come after (higher sibling_index) a sibling of target_type.
-- Usage: SELECT * FROM ast_follows('src/**/*.py', 'expression_statement', 'function_definition')
-- ----------------------------------------------------------------------------
-- ast_follows: Nodes of one type that appear after a node of another type
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_follows(
    source,
    node_type,
    after_type,
    after_name := NULL,
    language := NULL
) AS TABLE
    -- Parse, then delegate to ast_follows_from, which holds the predicate.
    WITH __ast_follows_src AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT * FROM ast_follows_from('__ast_follows_src', node_type, after_type,
                                   after_name := after_name);
