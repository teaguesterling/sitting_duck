// Auto-generated file - DO NOT EDIT
// Generated from SQL macro files in src/sql_macros/
// Run: python scripts/embed_sql_macros.py

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace duckdb {

// SQL macro definitions embedded at compile time
static const std::vector<std::pair<std::string, std::string>> EMBEDDED_SQL_MACROS = {
    {"semantic_predicates.sql", R"SQLMACRO(
-- Semantic Type Predicate Macros
-- Convenience functions for filtering by specific semantic types
-- These wrap is_semantic_type() for cleaner queries

-- =============================================================================
-- Definition Predicates
-- =============================================================================

-- Check if semantic_type is a function definition
CREATE OR REPLACE MACRO is_function_definition(st) AS
    is_semantic_type(st, 'DEFINITION_FUNCTION');

-- Check if semantic_type is a class definition
CREATE OR REPLACE MACRO is_class_definition(st) AS
    is_semantic_type(st, 'DEFINITION_CLASS');

-- Check if semantic_type is a variable definition
CREATE OR REPLACE MACRO is_variable_definition(st) AS
    is_semantic_type(st, 'DEFINITION_VARIABLE');

-- Check if semantic_type is a module/namespace definition
CREATE OR REPLACE MACRO is_module_definition(st) AS
    is_semantic_type(st, 'DEFINITION_MODULE');

-- Check if semantic_type is a type definition (typedef, type alias)
CREATE OR REPLACE MACRO is_type_definition(st) AS
    is_semantic_type(st, 'DEFINITION_TYPE');

-- =============================================================================
-- Computation Predicates
-- =============================================================================

-- Check if semantic_type is a function/method call
CREATE OR REPLACE MACRO is_function_call(st) AS
    is_semantic_type(st, 'COMPUTATION_CALL');

-- Check if semantic_type is a member/property access
CREATE OR REPLACE MACRO is_member_access(st) AS
    is_semantic_type(st, 'COMPUTATION_ACCESS');

-- =============================================================================
-- Literal Predicates
-- =============================================================================

-- Check if semantic_type is a string literal
CREATE OR REPLACE MACRO is_string_literal(st) AS
    is_semantic_type(st, 'LITERAL_STRING');

-- Check if semantic_type is a number literal
CREATE OR REPLACE MACRO is_number_literal(st) AS
    is_semantic_type(st, 'LITERAL_NUMBER');

-- Check if semantic_type is a boolean literal
CREATE OR REPLACE MACRO is_boolean_literal(st) AS
    is_semantic_type(st, 'LITERAL_BOOLEAN');

-- Check if semantic_type is any literal type
CREATE OR REPLACE MACRO is_literal(st) AS
    is_semantic_type(st, 'LITERAL');

-- =============================================================================
-- Flow Control Predicates
-- =============================================================================

-- Check if semantic_type is a conditional (if/switch/match)
CREATE OR REPLACE MACRO is_conditional(st) AS
    is_semantic_type(st, 'FLOW_CONDITIONAL');

-- Check if semantic_type is a loop (for/while/do)
CREATE OR REPLACE MACRO is_loop(st) AS
    is_semantic_type(st, 'FLOW_LOOP');

-- Check if semantic_type is a jump (return/break/continue/throw)
CREATE OR REPLACE MACRO is_jump(st) AS
    is_semantic_type(st, 'FLOW_JUMP');

-- =============================================================================
-- Organization Predicates
-- =============================================================================

-- Check if semantic_type is a block/scope
CREATE OR REPLACE MACRO is_block(st) AS
    is_semantic_type(st, 'ORGANIZATION_BLOCK');

-- Check if semantic_type is a list/array/container
CREATE OR REPLACE MACRO is_list(st) AS
    is_semantic_type(st, 'ORGANIZATION_LIST');

-- =============================================================================
-- Operator Predicates
-- =============================================================================

-- Check if semantic_type is an assignment
CREATE OR REPLACE MACRO is_assignment(st) AS
    is_semantic_type(st, 'OPERATOR_ASSIGNMENT');

-- Check if semantic_type is a comparison
CREATE OR REPLACE MACRO is_comparison(st) AS
    is_semantic_type(st, 'OPERATOR_COMPARISON');

-- Check if semantic_type is an arithmetic operation
CREATE OR REPLACE MACRO is_arithmetic(st) AS
    is_semantic_type(st, 'OPERATOR_ARITHMETIC');

-- Check if semantic_type is a logical operation (and/or/not)
CREATE OR REPLACE MACRO is_logical(st) AS
    is_semantic_type(st, 'OPERATOR_LOGICAL');

-- =============================================================================
-- External/Import Predicates
-- =============================================================================

-- Check if semantic_type is an import statement
CREATE OR REPLACE MACRO is_import(st) AS
    is_semantic_type(st, 'EXTERNAL_IMPORT');

-- Check if semantic_type is an export statement
CREATE OR REPLACE MACRO is_export(st) AS
    is_semantic_type(st, 'EXTERNAL_EXPORT');

-- Check if semantic_type is a foreign function interface
CREATE OR REPLACE MACRO is_foreign(st) AS
    is_semantic_type(st, 'EXTERNAL_FOREIGN');

-- =============================================================================
-- Metadata Predicates
-- =============================================================================

-- Check if semantic_type is a comment
CREATE OR REPLACE MACRO is_comment(st) AS
    is_semantic_type(st, 'METADATA_COMMENT');

-- Check if semantic_type is an annotation/decorator
CREATE OR REPLACE MACRO is_annotation(st) AS
    is_semantic_type(st, 'METADATA_ANNOTATION');

-- Check if semantic_type is a preprocessor directive
CREATE OR REPLACE MACRO is_directive(st) AS
    is_semantic_type(st, 'METADATA_DIRECTIVE');

-- =============================================================================
-- Type Predicates
-- =============================================================================

-- Check if semantic_type is a primitive type
CREATE OR REPLACE MACRO is_type_primitive(st) AS
    is_semantic_type(st, 'TYPE_PRIMITIVE');

-- Check if semantic_type is a composite type (struct, union, tuple)
CREATE OR REPLACE MACRO is_type_composite(st) AS
    is_semantic_type(st, 'TYPE_COMPOSITE');

-- Check if semantic_type is a reference/pointer type
CREATE OR REPLACE MACRO is_type_reference(st) AS
    is_semantic_type(st, 'TYPE_REFERENCE');

-- Check if semantic_type is a generic/template type
CREATE OR REPLACE MACRO is_type_generic(st) AS
    is_semantic_type(st, 'TYPE_GENERIC');

)SQLMACRO"},
    {"file_utilities.sql", R"SQLMACRO(
-- File Utility Macros
-- Functions for reading and extracting portions of files

-- =============================================================================
-- Line Reading - Table Macros (return rows)
-- =============================================================================

-- Read all lines from a file as rows with line numbers
-- Returns: line_number (BIGINT), line (VARCHAR)
CREATE OR REPLACE MACRO read_lines(file_path) AS TABLE
    SELECT
        ROW_NUMBER() OVER () AS line_number,
        line
    FROM (
        SELECT UNNEST(string_split(content, E'\n')) AS line
        FROM read_text(file_path)
    );

-- Read specific line range from a file as rows
-- Returns: line_number (BIGINT), line (VARCHAR)
CREATE OR REPLACE MACRO read_lines_range(file_path, start_line, end_line) AS TABLE
    WITH numbered AS (
        SELECT
            ROW_NUMBER() OVER () AS line_number,
            line
        FROM (
            SELECT UNNEST(string_split(content, E'\n')) AS line
            FROM read_text(file_path)
        )
    )
    SELECT line_number, line
    FROM numbered
    WHERE line_number >= start_line AND line_number <= end_line;

-- Read lines around a specific line (context window)
-- Useful for showing code context around a specific location
CREATE OR REPLACE MACRO read_lines_context(file_path, center_line, context_lines) AS TABLE
    WITH numbered AS (
        SELECT
            ROW_NUMBER() OVER () AS line_number,
            line
        FROM (
            SELECT UNNEST(string_split(content, E'\n')) AS line
            FROM read_text(file_path)
        )
    )
    SELECT line_number, line
    FROM numbered
    WHERE line_number >= (center_line - context_lines)
      AND line_number <= (center_line + context_lines);

-- =============================================================================
-- Line Reading - Scalar Macros (return single string)
-- =============================================================================

-- Get a specific line range as a single string (newline-joined)
CREATE OR REPLACE MACRO get_lines_text(file_path, start_line, end_line) AS (
    SELECT string_agg(line, E'\n' ORDER BY line_number)
    FROM read_lines_range(file_path, start_line, end_line)
);

-- Get a single line from a file
CREATE OR REPLACE MACRO get_line(file_path, line_num) AS (
    SELECT line
    FROM read_lines_range(file_path, line_num, line_num)
    LIMIT 1
);

-- =============================================================================
-- Source Extraction Helpers (for use with read_ast results)
-- =============================================================================

-- Extract source code for an AST node given file_path, start_line, end_line
-- This is useful when you've already parsed and want to get the source
CREATE OR REPLACE MACRO ast_get_source(file_path, start_line, end_line) AS
    get_lines_text(file_path, start_line, end_line);

-- Get source with line numbers prefixed (useful for display)
CREATE OR REPLACE MACRO ast_get_source_numbered(file_path, start_line, end_line) AS (
    SELECT string_agg(
        printf('%4d: %s', line_number, line),
        E'\n' ORDER BY line_number
    )
    FROM read_lines_range(file_path, start_line, end_line)
);

)SQLMACRO"},
    {"tree_navigation.sql", R"SQLMACRO(
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


)SQLMACRO"},
};

} // namespace duckdb
