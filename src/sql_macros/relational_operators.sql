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
-- All macros take a source file path or glob and call read_ast() internally.
-- =============================================================================

-- Check if ancestor nodes contain a matching descendant at any depth.
-- Uses descendant_count range-check for O(1) subtree membership.
-- Usage: SELECT * FROM ast_has('src/**/*.py', 'function_definition', 'return_statement')
-- Usage: SELECT * FROM ast_has('src/main.py', 'function_definition', 'call', 'execute')
CREATE OR REPLACE MACRO ast_has(
    source,
    ancestor_type,
    descendant_type,
    descendant_name := NULL,
    language := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
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

-- Negation of ast_has: find ancestor nodes that do NOT contain a descendant type.
-- Usage: SELECT * FROM ast_not_has('src/**/*.py', 'function_definition', 'return_statement')
-- Usage: SELECT * FROM ast_not_has('src/main.py', 'function_definition', 'call', 'execute')
CREATE OR REPLACE MACRO ast_not_has(
    source,
    ancestor_type,
    descendant_type,
    descendant_name := NULL,
    language := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
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

-- Find descendant nodes that are inside an ancestor of a given type.
-- Inverse of ast_has: returns the descendants, not the ancestors.
-- Uses descendant_count range-check for O(1) subtree membership.
-- Usage: SELECT * FROM ast_inside('src/**/*.py', 'call', 'function_definition', 'load_sql')
-- Usage: SELECT * FROM ast_inside('src/main.py', 'return_statement', 'function_definition')
CREATE OR REPLACE MACRO ast_inside(
    source,
    descendant_type,
    ancestor_type,
    ancestor_name := NULL,
    descendant_name := NULL,
    language := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
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

-- Find sibling nodes that precede a node of a given type.
-- Returns nodes that come before (lower sibling_index) a sibling of target_type.
-- Usage: SELECT * FROM ast_precedes('src/**/*.py', 'comment', 'function_definition')
CREATE OR REPLACE MACRO ast_precedes(
    source,
    node_type,
    before_type,
    before_name := NULL,
    language := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
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

-- Find sibling nodes that follow a node of a given type.
-- Returns nodes that come after (higher sibling_index) a sibling of target_type.
-- Usage: SELECT * FROM ast_follows('src/**/*.py', 'expression_statement', 'function_definition')
CREATE OR REPLACE MACRO ast_follows(
    source,
    node_type,
    after_type,
    after_name := NULL,
    language := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
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
