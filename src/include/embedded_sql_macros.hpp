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
};

} // namespace duckdb
