// Auto-generated file - DO NOT EDIT
// Generated from SQL macro files in src/sql_macros/
// Run: python scripts/embed_sql_macros.py
// Note: Large files are split into chunks to handle MSVC's 16KB string literal limit

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
-- Scalar functions for extracting source code from files
--
-- For full-featured line reading (globs, line specs, context windows, lateral joins),
-- use the duckdb_read_lines extension: https://github.com/teaguesterling/duckdb_read_lines

-- =============================================================================
-- Source Extraction - Scalar Macros (return single value)
-- =============================================================================

-- Extract source code for an AST node given file_path, start_line, end_line
-- Returns lines as a single newline-joined string
CREATE OR REPLACE MACRO ast_get_source(file_path, start_line, end_line) AS (
    WITH file_content AS (
        SELECT string_split(content, E'\n') as lines
        FROM read_text(file_path)
    ),
    line_nums AS (
        SELECT lines, generate_series(1, len(lines)) as line_numbers
        FROM file_content
    ),
    all_lines AS (
        SELECT
            UNNEST(line_numbers) AS line_number,
            lines[UNNEST(line_numbers)] AS line
        FROM line_nums
    )
    SELECT string_agg(line, E'\n' ORDER BY line_number)
    FROM all_lines
    WHERE line_number >= start_line AND line_number <= end_line
);

-- Get source with line numbers prefixed (useful for display)
CREATE OR REPLACE MACRO ast_get_source_numbered(file_path, start_line, end_line) AS (
    WITH file_content AS (
        SELECT string_split(content, E'\n') as lines
        FROM read_text(file_path)
    ),
    line_nums AS (
        SELECT lines, generate_series(1, len(lines)) as line_numbers
        FROM file_content
    ),
    all_lines AS (
        SELECT
            UNNEST(line_numbers) AS line_number,
            lines[UNNEST(line_numbers)] AS line
        FROM line_nums
    )
    SELECT string_agg(
        printf('%4d: %s', line_number, line),
        E'\n' ORDER BY line_number
    )
    FROM all_lines
    WHERE line_number >= start_line AND line_number <= end_line
);

-- =============================================================================
-- Source Lookup - Table Macro (find definition by name, return numbered source)
-- =============================================================================

-- Look up a definition by name and return its numbered source code
-- Usage:
--   SELECT * FROM ast_source_of('src/**/*.py', 'process_payment');
--   SELECT * FROM ast_source_of('src/**/*.py', 'MyClass', kind := 'class');
--   SELECT * FROM ast_source_of('src/**/*.py', 'handler', language := 'python', kind := 'function');
CREATE OR REPLACE MACRO ast_source_of(
    file_patterns,
    target_name,
    language := NULL,
    kind := NULL
) AS TABLE
    WITH
        ast AS (
            SELECT *
            FROM read_ast(file_patterns, language)
        ),
        matches AS (
            SELECT
                file_path,
                name,
                CASE
                    WHEN is_function_definition(semantic_type) THEN 'function'
                    WHEN is_class_definition(semantic_type) THEN 'class'
                    WHEN is_variable_definition(semantic_type) THEN 'variable'
                    WHEN is_module_definition(semantic_type) THEN 'module'
                    WHEN is_type_definition(semantic_type) THEN 'type'
                    ELSE 'other'
                END AS definition_kind,
                type,
                start_line,
                end_line
            FROM ast
            WHERE is_definition(semantic_type)
              AND is_construct(flags)
              AND name = target_name
              AND (kind IS NULL
                   OR (kind = 'function' AND is_function_definition(semantic_type))
                   OR (kind = 'class' AND is_class_definition(semantic_type))
                   OR (kind = 'variable' AND is_variable_definition(semantic_type))
                   OR (kind = 'module' AND is_module_definition(semantic_type))
                   OR (kind = 'type' AND is_type_definition(semantic_type))
              )
        ),
        -- Read file contents for all matched files
        file_contents AS (
            SELECT DISTINCT ON (m.file_path)
                m.file_path,
                string_split(fc.content, E'\n') AS lines
            FROM matches m
            JOIN read_text(file_patterns) fc ON fc.filename = m.file_path
        ),
        -- Build numbered source for each match
        source_output AS (
            SELECT
                m.file_path,
                m.name,
                m.definition_kind,
                m.start_line,
                m.end_line,
                (SELECT string_agg(
                    printf('%4d: %s', line_num, fc.lines[line_num]),
                    E'\n' ORDER BY line_num
                )
                FROM generate_series(m.start_line, m.end_line) AS t(line_num)
                ) AS source
            FROM matches m
            JOIN file_contents fc ON fc.file_path = m.file_path
        )
    SELECT
        file_path,
        name,
        definition_kind,
        start_line,
        end_line,
        source
    FROM source_output
    ORDER BY file_path, start_line;

-- =============================================================================
-- Source Extraction - Single Line
-- =============================================================================

-- Get a single line from a file
CREATE OR REPLACE MACRO ast_get_source_line(file_path, line_num) AS (
    WITH file_content AS (
        SELECT string_split(content, E'\n') as lines
        FROM read_text(file_path)
    )
    SELECT lines[line_num]
    FROM file_content
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

-- Get arguments of a function call node
-- Returns the actual argument nodes (excludes punctuation like parentheses and commas)
-- Usage: SELECT * FROM ast_call_arguments(my_ast_table, call_node_id)
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

-- Get all definitions (functions, classes, variables, etc.) with unified categories
-- Usage: SELECT * FROM ast_definitions('src/**/*.py')
-- Usage: SELECT * FROM ast_definitions('src/main.py', language := 'python')
--
-- IMPORTANT: When querying definitions from raw read_ast() output, always use the
-- full filter chain: is_definition(semantic_type) AND is_construct(flags) AND name != ''
-- Using is_definition() alone will include keyword tokens (def, class, CREATE, etc.)
-- as duplicates, since keywords share the semantic type of their parent construct.
CREATE OR REPLACE MACRO ast_definitions(source, language := NULL) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
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
-- Usage: SELECT * FROM ast_containing_line('src/main.py', 42)
CREATE OR REPLACE MACRO ast_containing_line(source, line_num, language := NULL) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT a.*
    FROM ast a
    WHERE a.start_line <= line_num AND a.end_line >= line_num
    ORDER BY (a.end_line - a.start_line), a.start_line;

-- Get all nodes within a line range
-- Usage: SELECT * FROM ast_in_range('src/main.py', 10, 20)
CREATE OR REPLACE MACRO ast_in_range(source, range_start, range_end, language := NULL) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT a.*
    FROM ast a
    WHERE a.start_line >= range_start AND a.end_line <= range_end;

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

-- =============================================================================
-- Analysis Macros
-- =============================================================================

-- Compute metrics for all functions in the AST
-- Returns one row per function with complexity metrics.
-- Uses function scope (excludes nested function internals) for accurate counts.
-- Usage: SELECT * FROM ast_function_metrics('src/**/*.py') WHERE cyclomatic > 10
CREATE OR REPLACE MACRO ast_function_metrics(source, language := NULL) AS TABLE
    WITH
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- All function definitions
        functions AS (
            SELECT
                a.node_id AS func_id,
                a.name,
                a.file_path,
                a.language,
                a.start_line,
                a.end_line,
                a.depth AS func_depth,
                a.descendant_count
            FROM ast a
            WHERE is_function_definition(a.semantic_type)
              AND a.name IS NOT NULL AND a.name != ''
        ),
        -- For each function, identify nested functions within it
        nested_funcs AS (
            SELECT
                nf.node_id AS nested_id,
                nf.descendant_count AS nested_count,
                f.func_id AS parent_func_id
            FROM functions f
            JOIN ast nf
              ON nf.node_id > f.func_id
             AND nf.node_id <= f.func_id + f.descendant_count
             AND is_function_definition(nf.semantic_type)
        ),
        -- Compute metrics for each function
        -- Counts only nodes in function scope (excludes nested function internals)
        function_metrics AS (
            SELECT
                f.func_id,
                f.file_path,
                f.name,
                f.language,
                f.start_line,
                f.end_line,
                f.func_depth,
                -- Count return statements (not the 'return' keyword, just the statement)
                COUNT(CASE
                    WHEN n.type = 'return_statement'
                    THEN 1
                END) AS return_count,
                -- Count conditionals: statements/clauses only, not keywords
                -- (if_statement, elif_clause, switch_statement, match_arm, etc.)
                COUNT(CASE
                    WHEN is_conditional(n.semantic_type)
                     AND (n.type LIKE '%_statement' OR n.type LIKE '%_clause'
                          OR n.type LIKE '%_expression' OR n.type LIKE '%_arm'
                          OR n.type LIKE '%_case' OR n.type LIKE '%_branch')
                    THEN 1
                END) AS conditionals,
                -- Count loops: statements only, not keywords
                -- (for_statement, while_statement, etc.)
                COUNT(CASE
                    WHEN is_loop(n.semantic_type)
                     AND (n.type LIKE '%_statement' OR n.type LIKE '%_expression'
                          OR n.type LIKE '%_loop')
                    THEN 1
                END) AS loops,
                -- Max depth of any node in scope
                MAX(n.depth) AS max_node_depth
            FROM functions f
            LEFT JOIN ast n
              ON n.node_id > f.func_id
             AND n.node_id <= f.func_id + f.descendant_count
            WHERE
                -- Exclude nodes inside nested functions
                NOT EXISTS (
                    SELECT 1 FROM nested_funcs nf
                    WHERE nf.parent_func_id = f.func_id
                      AND n.node_id > nf.nested_id
                      AND n.node_id <= nf.nested_id + nf.nested_count
                )
            GROUP BY f.func_id, f.file_path, f.name, f.language,
                     f.start_line, f.end_line, f.func_depth
        )
    SELECT
        fm.file_path,
        fm.name,
        fm.language,
        fm.start_line,
        fm.end_line,
        fm.end_line - fm.start_line + 1 AS lines,
        fm.return_count,
        fm.conditionals,
        fm.loops,
        fm.conditionals + fm.loops + 1 AS cyclomatic,
        COALESCE(fm.max_node_depth::INTEGER - fm.func_depth::INTEGER, 0) AS max_depth
    FROM function_metrics fm
    ORDER BY fm.file_path, fm.start_line;

-- Find functions that contain a specific node type
-- Returns functions with at least one matching node in their scope.
-- Uses function scope (excludes nested function internals).
-- Usage: SELECT * FROM ast_functions_containing('src/**/*.py', 'try_statement')
-- Usage: SELECT * FROM ast_functions_containing('src/**/*.py', 'call') WHERE match_name = 'eval'
CREATE OR REPLACE MACRO ast_functions_containing(source, target_type, language := NULL) AS TABLE
    WITH

)SQLMACRO"
        R"SQLMACRO(
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- All function definitions
        functions AS (
            SELECT
                a.node_id AS func_id,
                a.name AS func_name,
                a.file_path,
                a.language,
                a.start_line,
                a.end_line,
                a.descendant_count
            FROM ast a
            WHERE is_function_definition(a.semantic_type)
              AND a.name IS NOT NULL AND a.name != ''
        ),
        -- For each function, identify nested functions within it
        nested_funcs AS (
            SELECT
                nf.node_id AS nested_id,
                nf.descendant_count AS nested_count,
                f.func_id AS parent_func_id
            FROM functions f
            JOIN ast nf
              ON nf.node_id > f.func_id
             AND nf.node_id <= f.func_id + f.descendant_count
             AND is_function_definition(nf.semantic_type)
        ),
        -- Find matching nodes in each function's scope
        matches AS (
            SELECT
                f.func_id,
                f.func_name,
                f.file_path,
                f.language,
                f.start_line,
                f.end_line,
                n.node_id AS match_node_id,
                n.name AS match_name,
                n.type AS match_type,
                n.start_line AS match_line,
                n.peek AS match_peek
            FROM functions f
            JOIN ast n
              ON n.node_id > f.func_id
             AND n.node_id <= f.func_id + f.descendant_count
             AND n.type = target_type
            WHERE
                -- Exclude nodes inside nested functions
                NOT EXISTS (
                    SELECT 1 FROM nested_funcs nf
                    WHERE nf.parent_func_id = f.func_id
                      AND n.node_id > nf.nested_id
                      AND n.node_id <= nf.nested_id + nf.nested_count
                )
        )
    SELECT
        m.file_path,
        m.func_name,
        m.language,
        m.start_line AS func_start_line,
        m.end_line AS func_end_line,
        m.match_name,
        m.match_line,
        m.match_peek
    FROM matches m
    ORDER BY m.file_path, func_start_line, m.match_line;

-- Analyze nesting depth per function
-- Returns depth statistics for identifying deeply nested code.
-- Uses function scope (excludes nested function internals).
-- Usage: SELECT * FROM ast_nesting_analysis('src/**/*.py') WHERE max_depth > 5
CREATE OR REPLACE MACRO ast_nesting_analysis(source, language := NULL) AS TABLE
    WITH
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- All function definitions
        functions AS (
            SELECT
                a.node_id AS func_id,
                a.name,
                a.file_path,
                a.language,
                a.start_line,
                a.end_line,
                a.depth AS func_depth,
                a.descendant_count
            FROM ast a
            WHERE is_function_definition(a.semantic_type)
              AND a.name IS NOT NULL AND a.name != ''
        ),
        -- For each function, identify nested functions within it
        nested_funcs AS (
            SELECT
                nf.node_id AS nested_id,
                nf.descendant_count AS nested_count,
                f.func_id AS parent_func_id
            FROM functions f
            JOIN ast nf
              ON nf.node_id > f.func_id
             AND nf.node_id <= f.func_id + f.descendant_count
             AND is_function_definition(nf.semantic_type)
        ),
        -- Compute depth statistics for each function
        depth_stats AS (
            SELECT
                f.func_id,
                f.file_path,
                f.name,
                f.language,
                f.start_line,
                f.end_line,
                f.func_depth,
                -- Max relative depth (cast to INTEGER to avoid UINT32 overflow)
                MAX(n.depth::INTEGER - f.func_depth::INTEGER) AS max_depth,
                -- Average relative depth
                ROUND(AVG(n.depth::INTEGER - f.func_depth::INTEGER), 2) AS avg_depth,
                -- Count of deeply nested nodes (relative depth > 5)
                COUNT(CASE WHEN n.depth::INTEGER - f.func_depth::INTEGER > 5 THEN 1 END) AS deep_nodes,
                -- Total nodes in scope
                COUNT(*) AS total_nodes
            FROM functions f
            LEFT JOIN ast n
              ON n.node_id > f.func_id
             AND n.node_id <= f.func_id + f.descendant_count
            WHERE
                -- Exclude nodes inside nested functions
                NOT EXISTS (
                    SELECT 1 FROM nested_funcs nf
                    WHERE nf.parent_func_id = f.func_id
                      AND n.node_id > nf.nested_id
                      AND n.node_id <= nf.nested_id + nf.nested_count
                )
            GROUP BY f.func_id, f.file_path, f.name, f.language,
                     f.start_line, f.end_line, f.func_depth
        )
    SELECT
        ds.file_path,
        ds.name,
        ds.language,
        ds.start_line,
        ds.end_line,
        ds.max_depth,
        ds.avg_depth,
        ds.deep_nodes,
        ds.total_nodes
    FROM depth_stats ds
    ORDER BY ds.max_depth DESC, ds.file_path, ds.start_line;

-- Automated security concern detection
-- Scans for common security anti-patterns across languages.
-- Usage: SELECT * FROM ast_security_audit('src/**/*.py') WHERE risk_level = 'high'
CREATE OR REPLACE MACRO ast_security_audit(source, language := NULL) AS TABLE
    WITH
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- Define security patterns to detect
        security_patterns AS (
            SELECT * FROM (VALUES
                -- Code Injection (High Risk)
                ('eval', 'Code Injection', 'high', 'Dynamic code evaluation'),
                ('exec', 'Code Injection', 'high', 'Dynamic code execution'),
                ('compile', 'Code Injection', 'high', 'Dynamic code compilation'),
                ('Function', 'Code Injection', 'high', 'Dynamic function creation'),
                ('setInterval', 'Code Injection', 'medium', 'Potential code injection via string'),
                ('setTimeout', 'Code Injection', 'medium', 'Potential code injection via string'),
                -- Command Injection (High Risk)
                ('system', 'Command Injection', 'high', 'Shell command execution'),
                ('popen', 'Command Injection', 'high', 'Shell command execution'),
                ('spawn', 'Command Injection', 'high', 'Process spawning'),
                ('execSync', 'Command Injection', 'high', 'Synchronous command execution'),
                ('execFile', 'Command Injection', 'high', 'File execution'),
                ('ShellExecute', 'Command Injection', 'high', 'Windows shell execution'),
                -- Deserialization (High Risk)
                ('pickle.load', 'Deserialization', 'high', 'Unsafe pickle deserialization'),
                ('pickle.loads', 'Deserialization', 'high', 'Unsafe pickle deserialization'),
                ('yaml.load', 'Deserialization', 'high', 'Unsafe YAML loading'),
                ('unserialize', 'Deserialization', 'high', 'PHP unsafe deserialization'),
                ('Marshal.load', 'Deserialization', 'high', 'Ruby unsafe deserialization'),
                ('readObject', 'Deserialization', 'medium', 'Java deserialization'),
                -- SQL Injection indicators (Medium Risk)
                ('execute', 'SQL Injection', 'medium', 'SQL execution - verify parameterized'),
                ('executemany', 'SQL Injection', 'medium', 'SQL execution - verify parameterized'),
                ('raw', 'SQL Injection', 'medium', 'Raw SQL query'),
                ('rawQuery', 'SQL Injection', 'medium', 'Raw SQL query'),
                -- File Operations (Medium Risk)
                ('readFile', 'Path Traversal', 'medium', 'File read - verify path sanitization'),
                ('writeFile', 'Path Traversal', 'medium', 'File write - verify path sanitization'),
                ('unlink', 'Path Traversal', 'medium', 'File deletion - verify path sanitization'),
                ('rmdir', 'Path Traversal', 'medium', 'Directory deletion - verify path'),
                -- Crypto Concerns (Medium Risk)
                ('md5', 'Weak Crypto', 'medium', 'MD5 is cryptographically weak'),
                ('sha1', 'Weak Crypto', 'medium', 'SHA1 is cryptographically weak'),
                ('DES', 'Weak Crypto', 'medium', 'DES encryption is weak'),
                ('RC4', 'Weak Crypto', 'medium', 'RC4 encryption is weak'),
                -- Debug/Development (Low Risk)
                ('console.log', 'Debug Code', 'low', 'Debug logging in code'),
                ('print', 'Debug Code', 'low', 'Print statement - may leak info'),
                ('debugger', 'Debug Code', 'low', 'Debugger statement'),
                ('assert', 'Debug Code', 'low', 'Assert statement')
            ) AS t(pattern_name, risk_category, risk_level, description)
        ),
        -- Find all function calls in the AST
        calls AS (
            SELECT
                a.node_id,
                a.file_path,
                a.language,
                a.name,
                a.start_line,
                a.peek
            FROM ast a
            WHERE is_function_call(a.semantic_type)
              AND a.name IS NOT NULL AND a.name != ''
        ),
        -- Find the containing function for each call
        containing_funcs AS (
            SELECT
                c.node_id AS call_node_id,
                f.name AS function_name
            FROM calls c
            LEFT JOIN ast f
              ON f.node_id < c.node_id
             AND c.node_id <= f.node_id + f.descendant_count
             AND is_function_definition(f.semantic_type)
             AND f.name IS NOT NULL AND f.name != ''
            QUALIFY ROW_NUMBER() OVER (PARTITION BY c.node_id ORDER BY f.node_id DESC) = 1
        ),
        -- Match calls against security patterns
        -- Checks call name and peek (context) for qualified names like pickle.load
        findings AS (
            SELECT
                c.file_path,
                c.language,
                c.start_line,
                COALESCE(cf.function_name, '<module>') AS function_name,
                p.risk_category,
                p.risk_level,
                p.description AS finding,
                c.name AS matched_pattern,
                c.peek AS context
            FROM calls c
            JOIN security_patterns p
              ON c.name = p.pattern_name
              OR c.name LIKE '%.' || p.pattern_name
              OR c.name LIKE p.pattern_name || '.%'
              OR c.peek LIKE '%' || p.pattern_name || '%'
            LEFT JOIN containing_funcs cf ON cf.call_node_id = c.node_id
        )
    SELECT
        fi.file_path,
        fi.language,
        fi.start_line,
        fi.function_name,
        fi.risk_category,
        fi.risk_level,
        fi.finding,
        fi.matched_pattern,
        fi.context
    FROM findings fi
    ORDER BY
        CASE fi.risk_level WHEN 'high' THEN 1 WHEN 'medium' THEN 2 ELSE 3 END,
        fi.file_path, fi.start_line;

-- Resolve nearest definition ancestor for each definition node
-- Walks up parent_id, skipping organizational/structural nodes (e.g., blocks),
-- until it finds an ancestor that is itself a definition.
-- Returns (node_id, def_name, kind, parent_def_name, parent_def_kind, parent_def_node_id)
-- Usage: SELECT * FROM ast_definition_parent('my_ast_table')
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

-- Find potentially dead (unused) code
-- Identifies functions and classes that are never referenced in the codebase.
-- Note: This is heuristic - cannot detect dynamic usage or cross-file references
-- when analyzing a single file. Best used on entire codebase.
-- Usage: SELECT * FROM ast_dead_code('src/**/*.py')
CREATE OR REPLACE MACRO ast_dead_code(source, language := NULL) AS TABLE
    WITH
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- All function definitions
        function_defs AS (
            SELECT
                a.node_id,
                a.name,
                a.file_path,
                a.language,
                a.start_line,
                a.end_line,
                a.type,
                'function' AS definition_type
            FROM ast a

)SQLMACRO"
        R"SQLMACRO(
            WHERE is_function_definition(a.semantic_type)
              AND a.name IS NOT NULL AND a.name != ''
              -- Exclude special methods (constructors, dunder methods, etc.)
              AND a.name NOT LIKE '\_\_%' ESCAPE '\'
              AND a.name NOT IN ('main', 'setup', 'teardown', 'init', 'constructor')
        ),
        -- All class definitions
        class_defs AS (
            SELECT
                a.node_id,
                a.name,
                a.file_path,
                a.language,
                a.start_line,
                a.end_line,
                a.type,
                'class' AS definition_type
            FROM ast a
            WHERE is_class_definition(a.semantic_type)
              AND a.name IS NOT NULL AND a.name != ''
        ),
        -- All definitions combined
        all_defs AS (
            SELECT * FROM function_defs
            UNION ALL
            SELECT * FROM class_defs
        ),
        -- All function calls (references to functions)
        function_calls AS (
            SELECT DISTINCT a.name
            FROM ast a
            WHERE is_function_call(a.semantic_type)
              AND a.name IS NOT NULL AND a.name != ''
        ),
        -- All identifier references (for classes and other references)
        -- Exclude identifiers that are the name of a definition (parent is definition type)
        identifier_refs AS (
            SELECT DISTINCT a.name
            FROM ast a
            LEFT JOIN ast parent ON parent.node_id = a.parent_id
            WHERE is_identifier(a.semantic_type)
              AND a.name IS NOT NULL AND a.name != ''
              -- Exclude identifiers that are names within definitions
              AND (parent.semantic_type IS NULL
                   OR (NOT is_function_definition(parent.semantic_type)
                       AND NOT is_class_definition(parent.semantic_type)))
        ),
        -- All references combined
        all_refs AS (
            SELECT name FROM function_calls
            UNION
            SELECT name FROM identifier_refs
        ),
        -- Find definitions that are never referenced
        dead_code AS (
            SELECT
                d.file_path,
                d.name,
                d.language,
                d.start_line,
                d.end_line,
                d.type,
                d.definition_type,
                'Never referenced in codebase' AS reason
            FROM all_defs d
            WHERE NOT EXISTS (
                SELECT 1 FROM all_refs r
                WHERE r.name = d.name
            )
        )
    SELECT
        dc.file_path,
        dc.name,
        dc.language,
        dc.start_line,
        dc.end_line,
        dc.type,
        dc.definition_type,
        dc.reason
    FROM dead_code dc
    ORDER BY dc.file_path, dc.start_line;
)SQLMACRO"},
    {"pattern_matching.sql", R"SQLMACRO(
-- =============================================================================
-- Pattern Matching Macros for AST Querying
-- =============================================================================
--
-- Pattern-by-example matching system that allows users to write code patterns
-- with placeholders to find matching AST structures.
--
-- WILDCARD SYNTAX (two forms):
--
--   Simple wildcards (exact position matching):
--     __X__      Named wildcard - captures matched node as 'X'
--     __         Anonymous wildcard - matches any node but doesn't capture
--
--   Extended wildcards (with rules):
--     %__X<*>__%     Named variadic: matches 0 or more siblings
--     %__X<+>__%     Named variadic: matches 1 or more siblings
--     %__<*>__%      Anonymous variadic: matches 0 or more siblings (no capture)
--     %__<+>__%      Anonymous variadic: matches 1 or more siblings (no capture)
--     %__X<type=T>__%  Type constraint: only match nodes of type T
--
-- PARAMETERS:
--   match_syntax := false  - Include punctuation/delimiters in matching
--   match_by := 'type'     - Match on 'type' (tree-sitter) or 'semantic_type' (cross-language)
--   depth_fuzz := 0        - Allow +/- N levels of depth flexibility for cross-language matching
--
-- EXAMPLES:
--   -- Find all func() calls in Python files, capture the argument
--   SELECT * FROM ast_match('src/**/*.py', 'my_func(__X__)');
--
--   -- Find 3-arg calls with literal 2 in middle, capture func and last arg
--   SELECT * FROM ast_match('src/**/*.py', '__F__(__, 2, __X__)');
--
--   -- Explicit language (also used for pattern parsing)
--   SELECT * FROM ast_match('src/**/*.py', '__F__(__X__)', language := 'python', match_by := 'semantic_type');
--
--   -- Variadic: Find functions with ANY body that ends with return
--   SELECT * FROM ast_match('src/**/*.py', 'def __F__(__):
--       %__BODY<*>__%
--       return __Y__');
--
--   -- Access captures (LIST-valued, use [1] for single wildcards)
--   SELECT ast_capture(captures, 'X').name FROM ast_match(...);
--   SELECT captures['X'][1].name FROM ast_match(...);  -- equivalent
--
--   -- Variadic captures return multi-element lists
--   SELECT captures['BODY'] FROM ast_match(...);  -- list of matched nodes
--
-- NOTE: Simple wildcards use UPPERCASE (__NAME__), Python dunders use lowercase (__init__)
--       so they don't conflict. Extended wildcards use %__NAME<rules>__% syntax.
--
-- =============================================================================

-- =============================================================================
-- Helper Macros
-- =============================================================================

-- Check if name is a wildcard pattern:
--   __X__  - named wildcard (captures as 'X')
--   __     - anonymous wildcard (matches but doesn't capture)
CREATE OR REPLACE MACRO is_pattern_wildcard(name) AS
    name IS NOT NULL AND (
        name = '__' OR  -- anonymous wildcard
        regexp_matches(name, '^__[A-Z][A-Z0-9_]*__$')  -- named wildcard
    );

-- Extract capture name from wildcard (NULL for anonymous __)
CREATE OR REPLACE MACRO wildcard_capture_name(name) AS
    CASE
        WHEN name = '__' THEN NULL  -- anonymous, no capture
        WHEN regexp_matches(name, '^__[A-Z][A-Z0-9_]*__$')
            THEN substring(name, 3, length(name) - 4)
        ELSE NULL
    END;

-- Mask off language-specific bits (0-1) for cross-language semantic type comparison
-- Semantic type layout: [ ss kk tt ll ] where ll = language-specific bits
-- 0xFC = 11111100 masks off the last 2 bits
CREATE OR REPLACE MACRO semantic_type_base(sem_type) AS
    (sem_type::INTEGER & 252)::UTINYINT;

-- =============================================================================
-- Extended Wildcard Preprocessor
-- =============================================================================
--
-- TWO SYNTAXES SUPPORTED:
--
-- Legacy syntax: %__NAME<rules>__%
--   %__BODY<*>__%     - Named variadic
--   %__<*>__%         - Anonymous variadic
--
-- HTML syntax: %__<NAME modifiers attrs>__%  (Phase 1)
--   %__<BODY*>__%                    - Named variadic
--   %__<*>__%                        - Anonymous variadic
--   %__<X* type=identifier>__%       - With constraints
--   %__<BODY* max-descendants=10>__% - With numeric constraints
--
-- Modifiers: * (0+ siblings), + (1+ siblings), ~ (negate), ? (optional), ** (recursive)
-- Attributes: type=, not-type=, name=, semantic=, min-descendants=, max-descendants=, etc.

-- =============================================================================
-- HTML Wildcard Parser (regex-based for macro compatibility)
-- =============================================================================
-- NOTE: A parse_ast_objects() scalar function would enable using tree-sitter HTML
-- here instead of regex. See tracker/features/031-extended-wildcard-rules.md

-- Parse a single HTML wildcard tag: <NAME modifiers attrs>
-- Returns struct with all parsed fields
CREATE OR REPLACE MACRO parse_html_wildcard(html_str) AS {
    name: COALESCE(
        NULLIF(regexp_extract(html_str, '<([A-Z_][A-Z0-9_]*)', 1), ''),
        '_'
    ),
    variadic_star: regexp_matches(html_str, '\*') AND NOT regexp_matches(html_str, '\*\*'),
    variadic_plus: regexp_matches(html_str, '\+'),
    negate: regexp_matches(html_str, '~'),
    optional: regexp_matches(html_str, '\?'),
    recursive: regexp_matches(html_str, '\*\*'),
    type_constraint: NULLIF(regexp_extract(html_str, '[ ]type=([A-Za-z_][A-Za-z0-9_]*)', 1), ''),
    not_type_constraint: NULLIF(regexp_extract(html_str, 'not-type=([A-Za-z_][A-Za-z0-9_]*)', 1), ''),
    name_constraint: NULLIF(regexp_extract(html_str, '[ ]name=([A-Za-z_][A-Za-z0-9_]*)', 1), ''),
    name_pattern: NULLIF(regexp_extract(html_str, 'name-pattern=([^ >]+)', 1), ''),
    semantic_constraint: NULLIF(regexp_extract(html_str, 'semantic=([A-Z_]+)', 1), ''),
    min_descendants: TRY_CAST(NULLIF(regexp_extract(html_str, 'min-descendants=([0-9]+)', 1), '') AS INTEGER),
    max_descendants: TRY_CAST(NULLIF(regexp_extract(html_str, 'max-descendants=([0-9]+)', 1), '') AS INTEGER),
    min_children: TRY_CAST(NULLIF(regexp_extract(html_str, 'min-children=([0-9]+)', 1), '') AS INTEGER),
    max_children: TRY_CAST(NULLIF(regexp_extract(html_str, 'max-children=([0-9]+)', 1), '') AS INTEGER)
};

-- =============================================================================
-- Legacy Syntax Support
-- =============================================================================

-- Extract rules from legacy %__NAME<rules>__% syntax
CREATE OR REPLACE MACRO extract_wildcard_rules(pattern_str) AS (
    WITH
    matches AS (
        SELECT regexp_extract_all(pattern_str, '%__([A-Z][A-Z0-9_]*)?<([^>]+)>__%') as all_matches
    ),
    unnested AS (
        SELECT unnest(all_matches) as match_text FROM matches
    )
    SELECT list({
        name: NULLIF(regexp_extract(match_text, '%__([A-Z][A-Z0-9_]*)?<', 1), ''),
        rules_raw: regexp_extract(match_text, '<([^>]+)>', 1),
        is_variadic: regexp_extract(match_text, '<([^>]+)>', 1) LIKE '%*%'
                  OR regexp_extract(match_text, '<([^>]+)>', 1) LIKE '%+%',
        variadic_min: CASE
            WHEN regexp_extract(match_text, '<([^>]+)>', 1) LIKE '%+%' THEN 1
            WHEN regexp_extract(match_text, '<([^>]+)>', 1) LIKE '%*%' THEN 0
            ELSE NULL
        END,
        type_constraint: regexp_extract(match_text, 'type=([a-z_]+)', 1)
    }) as wildcards
    FROM unnested
    WHERE match_text IS NOT NULL AND match_text != ''
);

-- =============================================================================
-- Pattern Cleaning (handles both syntaxes)
-- =============================================================================

-- Clean pattern: convert extended wildcards to simple __NAME__ or __
-- Handles both legacy %__NAME<rules>__% and HTML %__<NAME* attrs>__% syntax
-- Order matters: more specific patterns first, then catch-alls
CREATE OR REPLACE MACRO clean_pattern(pattern_str) AS
    regexp_replace(
        regexp_replace(
            regexp_replace(
                regexp_replace(pattern_str,
                    -- Step 1: Legacy named %__NAME<rules>__% -> __NAME__
                    '%__([A-Z][A-Z0-9_]*)<[^>]+>__%', '__\1__', 'g'),
                -- Step 2: HTML named %__<NAME...>__% -> __NAME__ (must come before anonymous!)
                '%__<([A-Z_][A-Z0-9_]*)[^>]*>__%', '__\1__', 'g'),
            -- Step 3: Legacy anonymous %__<rules>__% -> __
            '%__<[^>]+>__%', '__', 'g'),
        -- Step 4: Catch any remaining (shouldn't be any, but safety)
        '%__<[*+~?][^>]*>__%', '__', 'g');

-- Check if pattern contains any variadic wildcards (either syntax)
-- Legacy: %__X<*>__%, %__X<+>__%, %__X<?>__%, %__X<~>__%
-- HTML: %__<X*>__%, %__<X+>__%, %__<X?>__%, %__<X~>__%
-- NOTE: This intentionally matches <**> too — callers must check recursive separately
CREATE OR REPLACE MACRO pattern_has_variadic(pattern_str) AS
    -- Legacy syntax: <*>, <+>, <?>, <~> inside %__...__%
    regexp_matches(pattern_str, '%__[A-Z]*<[^>]*[*+?~][^>]*>__%')
    -- HTML syntax: *, +, ?, ~ as modifier inside %__<...>__%
    OR regexp_matches(pattern_str, '%__<[^>]*[*+?~][^>]*>__%');

-- Check if pattern contains any recursive wildcards (<**> syntax)
-- Legacy: %__X<**>__% or HTML: %__<X**>__%
CREATE OR REPLACE MACRO pattern_has_recursive(pattern_str) AS
    -- Legacy syntax: <**> inside %__...__%
    regexp_matches(pattern_str, '%__[A-Z]*<\*\*>__%')
    -- HTML syntax: ** as modifier inside %__<...>__%
    OR regexp_matches(pattern_str, '%__<[A-Z_]*\*\*[^*>]*>__%');

-- Get list of variadic wildcard names
CREATE OR REPLACE MACRO get_variadic_names(pattern_str) AS (
    SELECT COALESCE(
        (SELECT list(w.name)
         FROM (SELECT unnest(extract_wildcard_rules(pattern_str)) as w)
         WHERE w.is_variadic),
        []::VARCHAR[]
    )
);

-- =============================================================================
-- Pattern Parsing (Table Macro - for inspection)
-- =============================================================================

-- Note: ast_pattern parses a CLEANED pattern (no %__X<rules>__% syntax)
-- Use clean_pattern() before calling ast_pattern if you have extended wildcards
CREATE OR REPLACE MACRO ast_pattern(pattern_str, language) AS TABLE
    WITH
        pattern_raw AS (
            SELECT * FROM parse_ast(pattern_str, language)
        ),
        root_depth AS (
            SELECT MIN(depth) as min_depth
            FROM pattern_raw
            WHERE type NOT IN ('module', 'expression_statement', 'program', 'source_file',
                              'compilation_unit', 'document', 'source', 'script')
        )
    SELECT
        node_id as pattern_node_id,
        depth - (SELECT min_depth FROM root_depth) as rel_depth,
        sibling_index,
        type as pattern_type,
        name as pattern_name,
        semantic_type as pattern_semantic_type,
        descendant_count,
        is_pattern_wildcard(name) as is_wildcard,
        wildcard_capture_name(name) as capture_name,
        -- Bug #009 fixed: IS_SYNTAX_ONLY is now auto-set for PARSER_DELIMITER/PUNCTUATION
        is_syntax_only(flags) as is_syntax,
        -- Is this node a parsing artifact wrapper around a wildcard?
        -- When wildcards are parsed as code, they produce expression_statement wrappers.
        -- These wrappers should be ignored when determining which siblings to exclude
        -- from variadic capture. Only expression_statement-type wrappers qualify.
        NOT is_pattern_wildcard(name)
        AND type IN ('expression_statement', 'expression')
        AND EXISTS (
            SELECT 1 FROM pattern_raw p2
            WHERE p2.parent_id = pattern_raw.node_id
              AND is_pattern_wildcard(p2.name)
        ) as wraps_wildcard,
        -- Is this node's parent a wildcard? If so, we relax sibling matching
        -- for this node because wildcard parents may have variable child structure
        -- (e.g., async keyword, different parameter counts, optional modifiers)
        EXISTS (
            SELECT 1 FROM pattern_raw p3
            WHERE p3.node_id = pattern_raw.parent_id
              AND is_pattern_wildcard(p3.name)
        ) as parent_is_wildcard,
        -- Note: is_recursive is always false here because the cleaned pattern
        -- has no <**> syntax. Actual detection happens in ast_match's pattern CTE
        -- using the original pattern_str. This field exists for struct completeness.
        false as is_recursive
    FROM pattern_raw
    WHERE depth >= (SELECT min_depth FROM root_depth);

-- =============================================================================
-- Pattern List (Scalar Macro - for use in table macros)
-- Returns all pattern nodes; filtering happens in ast_match based on match_syntax
-- =============================================================================

-- Note: Takes a CLEANED pattern string. Use clean_pattern() first for extended wildcards.
CREATE OR REPLACE MACRO ast_pattern_list(pattern_str, language) AS (
    SELECT list({
        rel_depth: rel_depth,
        sibling_index: sibling_index,
        pattern_type: pattern_type,
        pattern_name: pattern_name,
        pattern_semantic_type: pattern_semantic_type,
        pattern_descendant_count: descendant_count,
        is_wildcard: is_wildcard,
        capture_name: capture_name,
        is_syntax: is_syntax,
        wraps_wildcard: wraps_wildcard,
        parent_is_wildcard: parent_is_wildcard,
        is_recursive: is_recursive
    })
    FROM ast_pattern(pattern_str, language)
);

-- =============================================================================
-- Main Matching Macro
-- =============================================================================

-- Find AST nodes matching a pattern
-- Usage:
--   SELECT * FROM ast_match('src/**/*.py', 'my_func(__X__)');

)SQLMACRO"
        R"SQLMACRO(
--   SELECT * FROM ast_match('src/main.py', 'my_func(__X__)', match_syntax := true);
--   SELECT * FROM ast_match('src/**/*.py', '__F__(__X__)', match_by := 'semantic_type');
--
-- Parameters:
--   source       - File path or glob pattern to parse
--   pattern_str  - Code pattern with __WILDCARDS__ (e.g., 'my_func(__X__)')
--   language     - Language for parsing source and pattern (default: 'python')
--   match_syntax - If true, punctuation must match exactly (default: false)
--   match_by     - 'type' for tree-sitter types, 'semantic_type' for cross-language (default: 'type')
--
CREATE OR REPLACE MACRO ast_match(
    source,
    pattern_str,
    language := 'python',
    match_syntax := false,
    match_by := 'type',
    depth_fuzz := 0  -- Allow +/- this many levels of depth flexibility
) AS TABLE
    WITH
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- Unnest the pattern list into rows
        -- Use cleaned pattern (extended wildcards converted to simple)
        -- Detect variadics by checking if pattern contains %__*<*> or %__*<+>
        pattern AS (
            SELECT
                unnest.rel_depth,
                unnest.sibling_index,
                unnest.pattern_type,
                unnest.pattern_name,
                unnest.pattern_semantic_type,
                unnest.pattern_descendant_count,
                unnest.is_wildcard,
                unnest.capture_name,
                unnest.is_syntax,
                unnest.wraps_wildcard,
                unnest.parent_is_wildcard,
                -- Pattern-level variadic detection: *, +, ?, ~ in any extended wildcard syntax
                pattern_has_variadic(pattern_str) as pattern_has_variadic,
                -- Pattern-level recursive detection: ** in any extended wildcard syntax
                pattern_has_recursive(pattern_str) as pattern_has_recursive,
                -- Per-wildcard recursive detection: <**> or HTML <NAME**>
                -- Named: match capture_name against pattern string
                -- Anonymous: use pattern-level flag + check this wildcard is body-level
                -- (wrapped by expression_statement, not a parameter/name wildcard)
                (CASE
                    WHEN unnest.capture_name IS NOT NULL THEN
                        position('%__' || unnest.capture_name || '<**>__%' IN pattern_str) > 0
                        OR regexp_matches(pattern_str, '%__<' || unnest.capture_name || '\*\*[^>]*>__%')
                    WHEN unnest.is_wildcard AND unnest.capture_name IS NULL
                         AND pattern_has_recursive(pattern_str) THEN
                        -- Anonymous recursive: only if at a body-level position
                        -- (i.e., wrapped by an expression_statement)
                        unnest.wraps_wildcard = false  -- the wildcard itself is not a wrapper
                        AND unnest.rel_depth > 0
                        AND EXISTS (
                            SELECT 1 FROM (SELECT unnest(ast_pattern_list(
                                clean_pattern(pattern_str), language)) as pw)
                            WHERE pw.wraps_wildcard = true
                              AND pw.rel_depth = unnest.rel_depth::INTEGER - 1
                        )
                    ELSE false
                END) as is_recursive,
                -- Per-wildcard variadic detection: this specific wildcard has variadic syntax
                -- Uses position() for exact substring matching (LIKE _ is a wildcard, causing false matches)
                -- Includes <*>, <+>, <?>, <~> — excludes <**> (recursive pipeline)
                unnest.capture_name IS NOT NULL AND (
                    -- Legacy syntax: %__NAME<modifier>__%
                    position('%__' || unnest.capture_name || '<*>__%' IN pattern_str) > 0
                    OR position('%__' || unnest.capture_name || '<+>__%' IN pattern_str) > 0
                    OR position('%__' || unnest.capture_name || '<?>__%' IN pattern_str) > 0
                    OR position('%__' || unnest.capture_name || '<~>__%' IN pattern_str) > 0
                    -- HTML syntax: %__<NAME modifier>__%
                    OR regexp_matches(pattern_str, '%__<' || unnest.capture_name || '[*+?~][^>]*>__%')
                ) AND NOT (
                    -- Exclude recursive wildcards
                    position('%__' || unnest.capture_name || '<**>__%' IN pattern_str) > 0
                    OR regexp_matches(pattern_str, '%__<' || unnest.capture_name || '\*\*[^>]*>__%')
                ) as is_variadic,
                -- Variadic minimum count: 0 for *, <?>, <~>; 1 for +
                CASE
                    WHEN unnest.capture_name IS NOT NULL AND (
                        position('%__' || unnest.capture_name || '<+>__%' IN pattern_str) > 0
                        OR regexp_matches(pattern_str, '%__<' || unnest.capture_name || '\+[^>]*>__%')
                    ) THEN 1
                    WHEN unnest.capture_name IS NOT NULL AND (
                        position('%__' || unnest.capture_name || '<*>__%' IN pattern_str) > 0
                        OR position('%__' || unnest.capture_name || '<?>__%' IN pattern_str) > 0
                        OR position('%__' || unnest.capture_name || '<~>__%' IN pattern_str) > 0
                        OR regexp_matches(pattern_str, '%__<' || unnest.capture_name || '[*?~][^>]*>__%')
                    ) THEN 0
                    ELSE NULL
                END as variadic_min,
                -- Variadic maximum count: 1 for <?>; 0 for <~>; NULL (unlimited) for <*>, <+>
                CASE
                    WHEN unnest.capture_name IS NOT NULL AND (
                        position('%__' || unnest.capture_name || '<?>__%' IN pattern_str) > 0
                        OR regexp_matches(pattern_str, '%__<' || unnest.capture_name || '\?[^>]*>__%')
                    ) THEN 1
                    WHEN unnest.capture_name IS NOT NULL AND (
                        position('%__' || unnest.capture_name || '<~>__%' IN pattern_str) > 0
                        OR regexp_matches(pattern_str, '%__<' || unnest.capture_name || '~[^>]*>__%')
                    ) THEN 0
                    ELSE NULL
                END as variadic_max
            FROM (SELECT unnest(ast_pattern_list(
                clean_pattern(pattern_str),
                language)) as unnest)
            -- Filter out syntax nodes unless match_syntax is true
            WHERE match_syntax OR NOT unnest.is_syntax
        ),

        -- Get pattern root info for candidate filtering
        pattern_root AS (
            SELECT pattern_type, pattern_name, pattern_semantic_type, pattern_descendant_count, is_wildcard
            FROM pattern
            WHERE rel_depth = 0
            LIMIT 1
        ),

        -- Precompute recursive wildcard depth threshold.
        -- When <**> is present, pattern nodes at or below this depth use flexible
        -- depth/sibling matching. The threshold is the rel_depth of the wrapper
        -- (expression_statement) that contains the recursive wildcard.
        recursive_threshold AS (
            SELECT COALESCE(
                MIN(p3.rel_depth),
                999  -- No recursive wildcard: threshold never triggers
            ) as depth
            FROM pattern p3
            WHERE p3.wraps_wildcard
              AND EXISTS (
                  SELECT 1 FROM pattern p4
                  WHERE p4.is_recursive AND p4.rel_depth = p3.rel_depth + 1
              )
        ),

        -- Find candidate roots in target (nodes matching pattern root)
        candidates AS (
            SELECT
                t.node_id as candidate_root,
                t.depth as candidate_depth,
                t.descendant_count as candidate_descendants,
                t.file_path,
                t.start_line,
                t.end_line,
                t.peek
            FROM ast t, pattern_root pr
            WHERE
                -- Match on type or semantic_type based on match_by parameter
                -- For semantic_type mode: use exact type for operator TOKENS (*, +, etc.)
                -- but semantic type for operator EXPRESSIONS (binary_operator, binary_expression)
                CASE
                    WHEN match_by = 'semantic_type'
                         AND is_kind(pr.pattern_semantic_type, 'OPERATOR')
                         AND pr.pattern_descendant_count = 0  -- Token, not expression
                         THEN t.type = pr.pattern_type  -- Exact match for operator tokens
                    WHEN match_by = 'semantic_type'
                         THEN semantic_type_base(t.semantic_type) = semantic_type_base(pr.pattern_semantic_type)
                    ELSE t.type = pr.pattern_type
                END
                -- Name matching (for non-wildcards)
                AND (pr.pattern_name IS NULL OR pr.pattern_name = ''
                     OR pr.is_wildcard
                     OR t.name = pr.pattern_name)
                -- If pattern has descendants, candidate must have descendants too
                -- (prevents matching tokens when we want expressions)
                AND (pr.pattern_descendant_count = 0 OR t.descendant_count > 0)
        ),

        -- For each candidate, verify all non-wildcard pattern nodes have matches
        -- When variadics are present, sibling matching is relaxed (exists anywhere, not exact position)
        matched_candidates AS (
            SELECT c.*
            FROM candidates c
            WHERE NOT EXISTS (
                -- Find any non-wildcard, non-variadic pattern node without a corresponding match
                SELECT 1
                FROM pattern p
                WHERE p.is_wildcard = false
                  AND NOT p.is_variadic  -- Skip variadic wildcards in verification
                  AND NOT p.is_recursive  -- Skip recursive wildcards in verification
                  AND NOT p.wraps_wildcard  -- Skip wrapper nodes (e.g., expression_statement around __X__)
                  AND NOT EXISTS (
                      SELECT 1
                      FROM ast t
                      WHERE t.file_path = c.file_path  -- Must be same file (node_ids are per-file)
                        AND t.node_id >= c.candidate_root
                        AND t.node_id <= c.candidate_root + c.candidate_descendants
                        -- Depth matching with optional fuzz (cast to INTEGER to avoid UINT32 overflow)
                        -- When recursive wildcards present: nodes at/below the wrapper depth
                        -- of the recursive wildcard use >= matching (any depth deeper than expected)
                        AND CASE
                            WHEN p.pattern_has_recursive
                                 AND p.rel_depth >= (SELECT depth FROM recursive_threshold)
                            THEN (t.depth::INTEGER - c.candidate_depth::INTEGER) >= p.rel_depth::INTEGER
                            ELSE ABS((t.depth::INTEGER - c.candidate_depth::INTEGER) - p.rel_depth::INTEGER) <= depth_fuzz
                        END
                        -- Sibling matching: exact when no variadics, relaxed when variadics present
                        -- or when this node's parent is a wildcard (allows optional modifiers like async)
                        AND (p.rel_depth = 0
                             OR p.pattern_has_variadic  -- Skip sibling check if variadic present
                             OR p.parent_is_wildcard  -- Skip sibling check if parent is a wildcard
                             OR (p.pattern_has_recursive
                                 AND p.rel_depth >= (SELECT depth FROM recursive_threshold))
                             OR t.sibling_index = p.sibling_index)
                        -- Match on type or semantic_type based on match_by parameter
                        -- For semantic_type mode: use exact type for operator TOKENS (*, +, etc.)
                        -- but semantic type for operator EXPRESSIONS (binary_operator, etc.)
                        AND CASE
                            WHEN match_by = 'semantic_type'
                                 AND is_kind(p.pattern_semantic_type, 'OPERATOR')
                                 AND p.pattern_descendant_count = 0  -- Token, not expression
                                 THEN t.type = p.pattern_type  -- Exact match for operator tokens
                            WHEN match_by = 'semantic_type'
                                 THEN semantic_type_base(t.semantic_type) = semantic_type_base(p.pattern_semantic_type)
                            ELSE t.type = p.pattern_type
                        END
                        AND (p.pattern_name IS NULL OR p.pattern_name = '' OR t.name = p.pattern_name)
                  )
            )
        ),

        -- =====================================================================
        -- Single (non-variadic) capture pipeline
        -- =====================================================================

        -- Extract captured nodes for each non-variadic wildcard
        captures_single_raw AS (
            SELECT
                mc.candidate_root,
                mc.file_path as candidate_file,
                mc.candidate_depth,
                mc.candidate_descendants,
                p.capture_name,
                p.rel_depth as pattern_rel_depth,
                p.sibling_index as pattern_sibling_index,
                t.sibling_index as target_sibling_index,
                t.node_id as captured_node_id,
                t.type as captured_type,
                t.name as captured_name,
                t.peek as captured_peek,
                t.start_line as captured_start_line,
                t.end_line as captured_end_line
            FROM matched_candidates mc
            CROSS JOIN pattern p
            JOIN ast t ON
                t.file_path = mc.file_path

)SQLMACRO"
        R"SQLMACRO(
                AND t.node_id >= mc.candidate_root
                AND t.node_id <= mc.candidate_root + mc.candidate_descendants
                -- Depth matching: flexible when at/below recursive wildcard wrapper depth
                AND CASE
                    WHEN p.pattern_has_recursive
                         AND p.rel_depth >= (SELECT depth FROM recursive_threshold)
                    THEN (t.depth::INTEGER - mc.candidate_depth::INTEGER) >= p.rel_depth::INTEGER
                    ELSE ABS((t.depth::INTEGER - mc.candidate_depth::INTEGER) - p.rel_depth::INTEGER) <= depth_fuzz
                END
                AND (p.rel_depth = 0
                     OR p.pattern_has_variadic
                     OR p.parent_is_wildcard
                     OR (p.pattern_has_recursive
                         AND p.rel_depth >= (SELECT depth FROM recursive_threshold))
                     OR t.sibling_index = p.sibling_index)
            WHERE p.is_wildcard = true
              AND p.capture_name IS NOT NULL
              AND NOT p.is_variadic
              AND NOT p.is_recursive
              AND NOT p.parent_is_wildcard  -- Skip name-propagation children
        ),

        -- Deduplicate single captures - prefer nodes at expected sibling position
        captures_single_dedup AS (
            SELECT DISTINCT ON (candidate_file, candidate_root, capture_name)
                candidate_file,
                candidate_root,
                capture_name,
                captured_node_id,
                captured_type,
                captured_name,
                captured_peek,
                captured_start_line,
                captured_end_line
            FROM captures_single_raw
            ORDER BY candidate_file, candidate_root, capture_name,
                     ABS(target_sibling_index::INTEGER - pattern_sibling_index::INTEGER),
                     captured_node_id DESC
        ),

        -- Wrap single captures as 1-element lists
        captures_single_listed AS (
            SELECT
                candidate_file,
                candidate_root,
                capture_name,
                [{
                    capture: capture_name,
                    node_id: captured_node_id,
                    type: captured_type,
                    name: captured_name,
                    peek: captured_peek,
                    start_line: captured_start_line,
                    end_line: captured_end_line
                }] as capture_list
            FROM captures_single_dedup
        ),

        -- =====================================================================
        -- Variadic capture pipeline
        -- =====================================================================

        -- Find the parent scope for variadic captures by matching a structural
        -- anchor node to a target, then using that target's parent_id to scope capture.
        -- Variadic wildcards are parsed as identifiers INSIDE expression_statement
        -- wrappers, so the actual sibling level is at rel_depth - 1 (the wrapper's
        -- depth, not the wildcard identifier's depth).
        -- NOTE: If no anchor sibling exists at the wrapper level (variadic is the only
        -- non-syntax child), this produces zero rows — * variadics get empty lists,
        -- + variadics are filtered out. This is a known limitation.
        variadic_scope AS (
            SELECT DISTINCT
                mc.candidate_root,
                mc.file_path as candidate_file,
                p.capture_name,
                p.variadic_min,
                p.variadic_max,
                p.rel_depth as variadic_depth,
                t.parent_id as scope_parent_id
            FROM matched_candidates mc
            CROSS JOIN pattern p
            -- Find a non-wildcard, non-wrapper anchor at the wrapper's depth
            JOIN pattern p2 ON
                p2.rel_depth = p.rel_depth - 1
                AND NOT p2.is_wildcard
                AND NOT p2.wraps_wildcard
            JOIN ast t ON
                t.file_path = mc.file_path
                AND t.node_id >= mc.candidate_root
                AND t.node_id <= mc.candidate_root + mc.candidate_descendants
                AND ABS((t.depth::INTEGER - mc.candidate_depth::INTEGER) - p2.rel_depth::INTEGER) <= depth_fuzz
                AND (CASE
                    WHEN match_by = 'semantic_type'
                         THEN semantic_type_base(t.semantic_type) = semantic_type_base(p2.pattern_semantic_type)
                    ELSE t.type = p2.pattern_type
                END)
                AND (p2.pattern_name IS NULL OR p2.pattern_name = '' OR t.name = p2.pattern_name)
            WHERE p.is_variadic = true
              AND p.capture_name IS NOT NULL
        ),

        -- Collect target nodes that are siblings within the scoped parent
        captures_variadic_raw AS (
            SELECT
                vs.candidate_root,
                vs.candidate_file,
                vs.capture_name,
                vs.variadic_min,
                t.sibling_index as target_sibling_index,
                t.node_id as captured_node_id,
                t.type as captured_type,
                t.name as captured_name,
                t.peek as captured_peek,
                t.start_line as captured_start_line,
                t.end_line as captured_end_line
            FROM variadic_scope vs
            JOIN ast t ON
                t.file_path = vs.candidate_file
                AND t.parent_id = vs.scope_parent_id
            WHERE
              -- Exclude syntax/punctuation nodes (when match_syntax = false)
              (match_syntax OR NOT is_syntax_only(t.flags))
              -- Exclude nodes matched by non-variadic, non-wildcard pattern nodes
              AND NOT EXISTS (
                  SELECT 1 FROM pattern p2
                  WHERE p2.is_wildcard = false
                    AND NOT p2.is_variadic
                    AND NOT p2.wraps_wildcard
                    AND p2.rel_depth = vs.variadic_depth - 1
                    AND (CASE
                        WHEN match_by = 'semantic_type'
                             THEN semantic_type_base(t.semantic_type) = semantic_type_base(p2.pattern_semantic_type)
                        ELSE t.type = p2.pattern_type
                    END)
                    AND (p2.pattern_name IS NULL OR p2.pattern_name = '' OR t.name = p2.pattern_name)
              )
              -- Exclude nodes already claimed by single wildcard captures
              AND NOT EXISTS (
                  SELECT 1 FROM captures_single_dedup sd
                  WHERE sd.candidate_file = vs.candidate_file
                    AND sd.candidate_root = vs.candidate_root
                    AND sd.captured_node_id = t.node_id
              )
        ),

        -- Aggregate variadic nodes into ordered lists
        captures_variadic_listed AS (
            SELECT
                candidate_file,
                candidate_root,
                capture_name,
                list({
                    capture: capture_name,
                    node_id: captured_node_id,
                    type: captured_type,
                    name: captured_name,
                    peek: captured_peek,
                    start_line: captured_start_line,
                    end_line: captured_end_line
                } ORDER BY target_sibling_index) as capture_list
            FROM captures_variadic_raw
            GROUP BY candidate_file, candidate_root, capture_name
        ),

        -- Handle * variadics that match zero nodes (produce empty list)
        captures_variadic_empty AS (
            SELECT
                mc.file_path as candidate_file,
                mc.candidate_root,
                p.capture_name,
                []::STRUCT(capture VARCHAR, node_id BIGINT, "type" VARCHAR, name VARCHAR, peek VARCHAR, start_line BIGINT, end_line BIGINT)[] as capture_list
            FROM matched_candidates mc
            CROSS JOIN (
                SELECT DISTINCT capture_name, variadic_min
                FROM pattern
                WHERE is_variadic = true AND capture_name IS NOT NULL
            ) p
            WHERE p.variadic_min = 0  -- Only * (0+) variadics can be empty
              AND NOT EXISTS (
                  SELECT 1 FROM captures_variadic_listed vl
                  WHERE vl.candidate_file = mc.file_path
                    AND vl.candidate_root = mc.candidate_root
                    AND vl.capture_name = p.capture_name
              )
        ),

        -- =====================================================================
        -- Recursive (<**>) capture pipeline
        -- =====================================================================
        -- Recursive wildcards capture ALL descendants at any depth within the
        -- scope region. Uses descendant_count range-check (not recursive CTE)
        -- since nodes are in DFS pre-order.

        -- Find scope for recursive captures (same approach as variadic_scope)
        recursive_scope AS (
            SELECT DISTINCT
                mc.candidate_root,
                mc.file_path as candidate_file,
                p.capture_name,
                p.rel_depth as recursive_depth,
                t.parent_id as scope_parent_id,
                t_parent.node_id as scope_node_id,
                t_parent.descendant_count as scope_descendant_count
            FROM matched_candidates mc
            CROSS JOIN pattern p
            -- Find a non-wildcard, non-wrapper anchor at the wrapper's depth
            JOIN pattern p2 ON
                p2.rel_depth = p.rel_depth - 1
                AND NOT p2.is_wildcard
                AND NOT p2.wraps_wildcard
            JOIN ast t ON
                t.file_path = mc.file_path
                AND t.node_id >= mc.candidate_root
                AND t.node_id <= mc.candidate_root + mc.candidate_descendants
                AND ABS((t.depth::INTEGER - mc.candidate_depth::INTEGER) - p2.rel_depth::INTEGER) <= depth_fuzz
                AND (CASE
                    WHEN match_by = 'semantic_type'
                         THEN semantic_type_base(t.semantic_type) = semantic_type_base(p2.pattern_semantic_type)
                    ELSE t.type = p2.pattern_type
                END)
                AND (p2.pattern_name IS NULL OR p2.pattern_name = '' OR t.name = p2.pattern_name)
            -- Get the scope parent node for descendant range
            JOIN ast t_parent ON
                t_parent.file_path = mc.file_path
                AND t_parent.node_id = t.parent_id
            WHERE p.is_recursive = true
              AND p.capture_name IS NOT NULL
        ),

        -- Collect ALL descendants within the scope (any depth)
        captures_recursive_raw AS (
            SELECT
                rs.candidate_root,
                rs.candidate_file,
                rs.capture_name,
                t.node_id as captured_node_id,
                t.type as captured_type,
                t.name as captured_name,
                t.peek as captured_peek,
                t.start_line as captured_start_line,
                t.end_line as captured_end_line
            FROM recursive_scope rs
            JOIN ast t ON
                t.file_path = rs.candidate_file
                AND t.node_id > rs.scope_node_id
                AND t.node_id <= rs.scope_node_id + rs.scope_descendant_count
            WHERE
              -- Exclude syntax/punctuation nodes (when match_syntax = false)
              (match_syntax OR NOT is_syntax_only(t.flags))
              -- Exclude nodes already claimed by single wildcard captures
              AND NOT EXISTS (
                  SELECT 1 FROM captures_single_dedup sd
                  WHERE sd.candidate_file = rs.candidate_file
                    AND sd.candidate_root = rs.candidate_root
                    AND sd.captured_node_id = t.node_id
              )
        ),

        -- Aggregate recursive captures into ordered lists (DFS order = node_id order)
        captures_recursive_listed AS (
            SELECT
                candidate_file,
                candidate_root,
                capture_name,
                list({
                    capture: capture_name,
                    node_id: captured_node_id,
                    type: captured_type,
                    name: captured_name,
                    peek: captured_peek,
                    start_line: captured_start_line,
                    end_line: captured_end_line
                } ORDER BY captured_node_id) as capture_list
            FROM captures_recursive_raw
            GROUP BY candidate_file, candidate_root, capture_name
        ),

        -- Handle recursive wildcards that match zero descendants (empty list)
        captures_recursive_empty AS (
            SELECT
                mc.file_path as candidate_file,
                mc.candidate_root,
                p.capture_name,
                []::STRUCT(capture VARCHAR, node_id BIGINT, "type" VARCHAR, name VARCHAR, peek VARCHAR, start_line BIGINT, end_line BIGINT)[] as capture_list
            FROM matched_candidates mc
            CROSS JOIN (
                SELECT DISTINCT capture_name
                FROM pattern
                WHERE is_recursive = true AND capture_name IS NOT NULL
            ) p
            WHERE NOT EXISTS (
                SELECT 1 FROM captures_recursive_listed rl
                WHERE rl.candidate_file = mc.file_path
                  AND rl.candidate_root = mc.candidate_root
                  AND rl.capture_name = p.capture_name
            )
        ),

        -- =====================================================================
        -- Combine and aggregate all captures
        -- =====================================================================

        captures_all AS (
            SELECT candidate_file, candidate_root, capture_name, capture_list
            FROM captures_single_listed

)SQLMACRO"
        R"SQLMACRO(
            UNION ALL
            SELECT candidate_file, candidate_root, capture_name, capture_list
            FROM captures_variadic_listed
            UNION ALL
            SELECT candidate_file, candidate_root, capture_name, capture_list
            FROM captures_variadic_empty
            UNION ALL
            SELECT candidate_file, candidate_root, capture_name, capture_list
            FROM captures_recursive_listed
            UNION ALL
            SELECT candidate_file, candidate_root, capture_name, capture_list
            FROM captures_recursive_empty
        ),

        -- Aggregate captures into MAP(VARCHAR, LIST(STRUCT{...}))
        captures_agg AS (
            SELECT
                candidate_file,
                candidate_root,
                map_from_entries(list((
                    capture_name,
                    capture_list
                ))) as captures
            FROM captures_all
            GROUP BY candidate_file, candidate_root
        ),

        -- Reject matches where named + variadics have 0 captures.
        -- NOTE: Anonymous + variadics (%__<+>__%) are not enforced here because
        -- is_variadic requires capture_name IS NOT NULL. This is a known limitation;
        -- anonymous + variadics currently behave like * (0+).
        variadic_plus_valid AS (
            SELECT mc.file_path, mc.candidate_root
            FROM matched_candidates mc
            WHERE NOT EXISTS (
                -- Find any named + variadic that has no captures for this match
                SELECT 1
                FROM (
                    SELECT DISTINCT capture_name
                    FROM pattern
                    WHERE is_variadic = true AND capture_name IS NOT NULL AND variadic_min = 1
                ) plus_vars
                WHERE NOT EXISTS (
                    SELECT 1 FROM captures_variadic_listed vl
                    WHERE vl.candidate_file = mc.file_path
                      AND vl.candidate_root = mc.candidate_root
                      AND vl.capture_name = plus_vars.capture_name
                )
            )
        ),

        -- Reject matches where variadic captures exceed variadic_max.
        -- <?> has max=1 (optional), <~> has max=0 (negation/absence).
        variadic_max_valid AS (
            SELECT mc.file_path, mc.candidate_root
            FROM matched_candidates mc
            WHERE NOT EXISTS (
                -- Find any variadic with a max constraint that is exceeded
                SELECT 1
                FROM (
                    SELECT DISTINCT capture_name, variadic_max
                    FROM pattern
                    WHERE is_variadic = true AND capture_name IS NOT NULL AND variadic_max IS NOT NULL
                ) max_vars
                JOIN captures_variadic_listed vl
                  ON vl.candidate_file = mc.file_path
                 AND vl.candidate_root = mc.candidate_root
                 AND vl.capture_name = max_vars.capture_name
                WHERE length(vl.capture_list) > max_vars.variadic_max
            )
        ),

        -- =====================================================================
        -- Same-name constraint validation
        -- =====================================================================
        -- When __X__ appears multiple times in the pattern, all positions must
        -- capture nodes with the same peek (source text) value.
        -- This CTE finds (candidate, capture_name) pairs that FAIL the check.
        -- Uses MIN/MAX on peek across all raw captures (simpler than per-position dedup).
        -- Per-position best matches for same-name capture checking.
        -- Reads from captures_single_raw (pre-dedup) intentionally: we need per-position
        -- matches, whereas captures_single_dedup collapses to one capture per name.
        -- ROW_NUMBER picks the best match per (candidate, capture_name, position) using
        -- the same ranking logic as captures_single_dedup (sibling proximity, then node_id).
        same_name_ranked AS (
            SELECT
                candidate_file, candidate_root, capture_name,
                pattern_rel_depth, pattern_sibling_index,
                captured_peek,
                ROW_NUMBER() OVER (
                    PARTITION BY candidate_file, candidate_root, capture_name,
                                 pattern_rel_depth, pattern_sibling_index
                    ORDER BY ABS(target_sibling_index::INTEGER - pattern_sibling_index::INTEGER),
                             captured_node_id DESC
                ) as rn
            FROM captures_single_raw
            -- Exclude root-level captures: tree-sitter propagates names to parent
            -- nodes (e.g., function_definition gets name __F__ from function name),
            -- creating unintended duplicate capture names at depth 0.
            WHERE pattern_rel_depth > 0
              -- Only check captures that actually appear >1 time in the pattern
              -- Exclude parent_is_wildcard nodes: tree-sitter propagates names from
              -- parent to child (e.g., method_definition name → property_identifier),
              -- creating duplicate capture names with different peeks.
              AND capture_name IN (
                  SELECT p.capture_name FROM pattern p
                  WHERE p.is_wildcard AND p.capture_name IS NOT NULL
                    AND NOT p.is_variadic AND p.rel_depth > 0
                    AND NOT p.parent_is_wildcard
                  GROUP BY p.capture_name HAVING count(*) > 1
              )
        ),

        -- Find (candidate, capture_name) pairs where different positions disagree
        same_name_inconsistent AS (
            SELECT candidate_file, candidate_root, capture_name
            FROM same_name_ranked
            WHERE rn = 1
            GROUP BY candidate_file, candidate_root, capture_name
            HAVING COUNT(DISTINCT captured_peek) > 1
        )

    SELECT
        ROW_NUMBER() OVER (ORDER BY mc.file_path, mc.candidate_root) as match_id,
        mc.candidate_root as root_node_id,
        mc.file_path,
        mc.start_line,
        mc.end_line,
        mc.peek,
        ca.captures
    FROM matched_candidates mc
    LEFT JOIN captures_agg ca ON mc.file_path = ca.candidate_file
                              AND mc.candidate_root = ca.candidate_root
    WHERE EXISTS (
        SELECT 1 FROM variadic_plus_valid vpv
        WHERE vpv.file_path = mc.file_path
          AND vpv.candidate_root = mc.candidate_root
    )
    -- Variadic max constraint: reject matches where captures exceed max (<?> max=1, <~> max=0)
    AND EXISTS (
        SELECT 1 FROM variadic_max_valid vmv
        WHERE vmv.file_path = mc.file_path
          AND vmv.candidate_root = mc.candidate_root
    )
    -- Same-name constraint: reject matches where any same-name capture has inconsistent peeks
    AND NOT EXISTS (
        SELECT 1 FROM same_name_inconsistent sni
        WHERE sni.candidate_file = mc.file_path
          AND sni.candidate_root = mc.candidate_root
    );

-- =============================================================================
-- Convenience Macro for Capture Access
-- =============================================================================
-- Since captures are now LIST-valued, use this to get the first (usually only)
-- element for single wildcard captures.
-- Usage: ast_capture(captures, 'X').name instead of captures['X'][1].name
CREATE OR REPLACE MACRO ast_capture(captures_map, capture_name) AS
    captures_map[capture_name][1];
)SQLMACRO"},
    {"relational_operators.sql", R"SQLMACRO(
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

)SQLMACRO"},
    {"css_selectors.sql", R"SQLMACRO(
-- =============================================================================
-- CSS Selector Query Language for AST (ast_select)
-- =============================================================================
--
-- Bootstraps CSS selector parsing using tree-sitter-css grammar via parse_ast().
-- The parsed selector AST is walked to build SQL conditions against the target AST.
--
-- Supported syntax:
--   type                          - Match by AST node type
--   type#name                     - Match by type + node name
--   .semantic                     - Match by semantic type (e.g., .function, .CALL)
--   A B                           - B is a descendant of A
--   A > B                         - B is a direct child of A
--   A ~ B                         - B is a general sibling after A
--   A + B                         - B immediately follows A
--   :has(selector)                - Contains a descendant matching selector
--   :not(:has(selector))          - Does NOT contain a descendant matching selector
--   [name=value]                  - Attribute filter (name, type, language)
--   Compound: type#name:has(...)  - Multiple conditions on same element
--
-- Usage:
--   SELECT * FROM ast_select('src/**/*.py', '.function:has(return_statement)');
--   SELECT * FROM ast_select('src/*.js', 'class_body > method_definition');
--   SELECT * FROM ast_select('src/*.py', '.function:has(.call#execute):not(:has(try_statement))');
-- =============================================================================

CREATE OR REPLACE MACRO ast_select(
    source,
    selector,
    language := NULL
) AS TABLE
    WITH
        -- Parse the CSS selector FIRST (before source, so DuckDB can evaluate it early)
        sel AS (
            SELECT * FROM parse_ast(selector, 'css')
        ),

        -- Parse source files
        -- TODO: optimize with +schema modes to avoid computing peek/native when unused
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),

        -- Find the top-level selector node (skip stylesheet and ERROR wrappers)
        -- Note: bare words like "function_definition" parse as 'identifier' in CSS,
        -- not 'tag_name'. We treat 'identifier' at the root as equivalent to 'tag_name'.
        sel_root AS (
            SELECT node_id,
                   CASE WHEN type = 'identifier' THEN 'tag_name' ELSE type END as type,
                   name,
                   descendant_count
            FROM sel
            WHERE type NOT IN ('stylesheet', 'ERROR')
              AND depth = (
                  SELECT MIN(depth) FROM sel
                  WHERE type NOT IN ('stylesheet', 'ERROR')
              )
            LIMIT 1
        ),

        -- =====================================================================
        -- Extract selector components from the parsed CSS AST
        -- =====================================================================

        -- The root selector type determines the query strategy
        root_type AS (
            SELECT type FROM sel_root
        ),

        -- For combinators (descendant, child, sibling): extract left and right types
        -- Left = first tag_name child, Right = last tag_name child
        combinator_parts AS (
            SELECT
                (SELECT s.name FROM sel s
                 WHERE s.parent_id = (SELECT node_id FROM sel_root) AND s.type = 'tag_name'
                 ORDER BY s.sibling_index ASC LIMIT 1) as left_type,
                (SELECT s.name FROM sel s
                 WHERE s.parent_id = (SELECT node_id FROM sel_root) AND s.type = 'tag_name'
                 ORDER BY s.sibling_index DESC LIMIT 1) as right_type
        ),

        -- For simple/compound selectors: extract type, #name, .class filters
        -- Type filter: find the tag_name in the selector tree (may be nested in compound selectors)
        -- Exclude tag_names inside :has()/:not() arguments — those are descendant filters, not the base type
        simple_type AS (
            SELECT COALESCE(
                -- tag_name anywhere in selector (shallowest first), excluding inside arguments
                (SELECT s.name FROM sel s
                 WHERE s.type = 'tag_name'
                   AND s.node_id >= (SELECT node_id FROM sel_root)
                   AND s.node_id <= (SELECT node_id + descendant_count FROM sel_root)
                   -- Not inside an arguments block (those belong to :has/:not)
                   AND NOT EXISTS (
                       SELECT 1 FROM sel args
                       WHERE args.type = 'arguments'
                         AND s.node_id > args.node_id
                         AND s.node_id <= args.node_id + args.descendant_count
                   )
                 ORDER BY s.depth ASC
                 LIMIT 1),
                -- Root IS a tag_name (bare type selector)
                CASE WHEN (SELECT type FROM root_type) = 'tag_name'
                     THEN (SELECT name FROM sel_root) END
            ) as type_filter
        ),

        -- #id name filter
        simple_id AS (
            SELECT s.name as name_filter
            FROM sel s
            WHERE s.type = 'id_name'
              AND s.node_id > (SELECT node_id FROM sel_root)
              AND s.node_id <= (SELECT node_id + descendant_count FROM sel_root)
              -- Only top-level #id (not inside :has() arguments)
              AND NOT EXISTS (
                  SELECT 1 FROM sel args
                  WHERE args.type = 'arguments'
                    AND s.node_id > args.node_id
                    AND s.node_id <= args.node_id + args.descendant_count
              )
            LIMIT 1
        ),

        -- .class semantic filter
        simple_class AS (
            SELECT s.name as class_filter
            FROM sel s
            WHERE s.type = 'class_name'
              -- Must be child of a class_selector, not a pseudo-class name
              AND EXISTS (
                  SELECT 1 FROM sel p
                  WHERE p.node_id = s.parent_id AND p.type = 'class_selector'
              )
            LIMIT 1
        ),

        -- :has() conditions — find pseudo_class_selector nodes with class_name='has'
        -- that are NOT inside a :not() wrapper
        has_conditions AS (
            SELECT
                pcs.node_id as has_id,
                -- Type inside :has() arguments
                (SELECT t.name FROM sel t
                 JOIN sel args ON args.parent_id = pcs.node_id AND args.type = 'arguments'
                 WHERE t.type = 'tag_name'
                   AND t.node_id > args.node_id
                   AND t.node_id <= args.node_id + args.descendant_count
                 LIMIT 1) as has_type,
                -- #name inside :has() arguments
                (SELECT t.name FROM sel t
                 JOIN sel args ON args.parent_id = pcs.node_id AND args.type = 'arguments'
                 WHERE t.type = 'id_name'
                   AND t.node_id > args.node_id
                   AND t.node_id <= args.node_id + args.descendant_count
                 LIMIT 1) as has_name,
                -- .class inside :has() arguments
                (SELECT t.name FROM sel t
                 JOIN sel cs ON cs.type = 'class_selector'
                   AND t.parent_id = cs.node_id AND t.type = 'class_name'
                 JOIN sel args ON args.parent_id = pcs.node_id AND args.type = 'arguments'
                 WHERE cs.node_id > args.node_id
                   AND cs.node_id <= args.node_id + args.descendant_count
                 LIMIT 1) as has_class
            FROM sel pcs
            WHERE pcs.type = 'pseudo_class_selector'
              AND EXISTS (
                  SELECT 1 FROM sel cn
                  WHERE cn.parent_id = pcs.node_id AND cn.type = 'class_name' AND cn.name = 'has'
              )
              -- NOT inside a :not() arguments block
              AND NOT EXISTS (
                  SELECT 1 FROM sel not_args
                  JOIN sel not_pcs ON not_args.parent_id = not_pcs.node_id
                    AND not_pcs.type = 'pseudo_class_selector'
                  JOIN sel not_cn ON not_cn.parent_id = not_pcs.node_id
                    AND not_cn.type = 'class_name' AND not_cn.name = 'not'
                  WHERE not_args.type = 'arguments'
                    AND pcs.node_id > not_args.node_id
                    AND pcs.node_id <= not_args.node_id + not_args.descendant_count
              )
        ),

        -- :not(:has()) conditions — :not() wrapping a :has()
        not_has_conditions AS (
            SELECT
                not_pcs.node_id as not_id,
                -- Find the :has() inside the :not()'s arguments
                (SELECT t.name FROM sel t
                 JOIN sel args ON args.parent_id = has_pcs.node_id AND args.type = 'arguments'
                 WHERE t.type = 'tag_name'
                   AND t.node_id > args.node_id
                   AND t.node_id <= args.node_id + args.descendant_count
                 LIMIT 1) as not_has_type,
                (SELECT t.name FROM sel t
                 JOIN sel args ON args.parent_id = has_pcs.node_id AND args.type = 'arguments'
                 WHERE t.type = 'id_name'
                   AND t.node_id > args.node_id
                   AND t.node_id <= args.node_id + args.descendant_count
                 LIMIT 1) as not_has_name
            FROM sel not_pcs
            JOIN sel not_cn ON not_cn.parent_id = not_pcs.node_id
              AND not_cn.type = 'class_name' AND not_cn.name = 'not'
            JOIN sel not_args ON not_args.parent_id = not_pcs.node_id
              AND not_args.type = 'arguments'
            JOIN sel has_pcs ON has_pcs.type = 'pseudo_class_selector'
              AND has_pcs.node_id > not_args.node_id
              AND has_pcs.node_id <= not_args.node_id + not_args.descendant_count
            JOIN sel has_cn ON has_cn.parent_id = has_pcs.node_id
              AND has_cn.type = 'class_name' AND has_cn.name = 'has'
            WHERE not_pcs.type = 'pseudo_class_selector'
        ),

        -- [attr op value] conditions — supports =, *=, ^=, $= operators
        -- Handles both plain_value and integer_value (for [params=0])
        attr_conditions AS (
            SELECT
                (SELECT a.name FROM sel a WHERE a.parent_id = s.node_id AND a.type = 'attribute_name' LIMIT 1) as attr_name,
                COALESCE(
                    (SELECT a.name FROM sel a WHERE a.parent_id = s.node_id AND a.type = 'plain_value' LIMIT 1),
                    (SELECT a.name FROM sel a WHERE a.parent_id = s.node_id AND a.type = 'integer_value' LIMIT 1)
                ) as attr_value,
                -- Detect operator: =, *=, ^=, $=
                CASE
                    WHEN EXISTS (SELECT 1 FROM sel a WHERE a.parent_id = s.node_id AND a.type = '*=') THEN '*='
                    WHEN EXISTS (SELECT 1 FROM sel a WHERE a.parent_id = s.node_id AND a.type = '^=') THEN '^='
                    WHEN EXISTS (SELECT 1 FROM sel a WHERE a.parent_id = s.node_id AND a.type = '$=') THEN '$='
                    ELSE '='
                END as attr_op
            FROM sel s
            WHERE s.type = 'attribute_selector'
              -- Only top-level attributes (not inside :has() arguments)
              AND NOT EXISTS (
                  SELECT 1 FROM sel args
                  WHERE args.type = 'arguments'
                    AND s.node_id > args.node_id
                    AND s.node_id <= args.node_id + args.descendant_count
              )
        ),

        -- =====================================================================
        -- Additional pseudo-class extraction
        -- =====================================================================

        -- Extract all pseudo-class names for boolean/positional checks
        -- (excludes :has, :not which are handled separately above)
        pseudo_classes AS (
            SELECT cn.name as pseudo_name,
                   -- For pseudo-classes with arguments, extract the argument value
                   -- Handles tag_name, integer_value, AND quoted strings (for :matches)
                   COALESCE(
                       (SELECT t.name FROM sel t
                        JOIN sel args ON args.parent_id = pcs.node_id AND args.type = 'arguments'
                        WHERE (t.type = 'tag_name' OR t.type = 'integer_value')
                          AND t.node_id > args.node_id
                          AND t.node_id <= args.node_id + args.descendant_count
                        LIMIT 1),
                       -- Quoted string argument: strip surrounding quotes
                       (SELECT trim(t.name, '"''') FROM sel t
                        JOIN sel args ON args.parent_id = pcs.node_id AND args.type = 'arguments'
                        WHERE t.type = 'string_value'
                          AND t.node_id > args.node_id
                          AND t.node_id <= args.node_id + args.descendant_count
                        LIMIT 1)
                   ) as pseudo_arg
            FROM sel pcs
            JOIN sel cn ON cn.parent_id = pcs.node_id AND cn.type = 'class_name'
            WHERE pcs.type = 'pseudo_class_selector'
              -- Exclude :has, :not (handled by dedicated CTEs above)
              AND cn.name NOT IN ('has', 'not')
              -- Exclude .class selectors (class_name under class_selector, not pseudo_class_selector)
              -- Only top-level pseudo-classes (not inside :has/:not arguments)
              AND NOT EXISTS (
                  SELECT 1 FROM sel args
                  WHERE args.type = 'arguments'
                    AND pcs.node_id > args.node_id
                    AND pcs.node_id <= args.node_id + args.descendant_count
              )
        ),

        -- :matches("code") — structural substring matching via DFS contiguity
        -- Parse the pattern by extracting the quoted argument from the selector string.

)SQLMACRO"
        R"SQLMACRO(
        -- Exact structural match: db.execute() matches only zero-arg calls,
        -- db.execute("SELECT 1") matches only that exact call.
        -- Use ___ (triple underscore) as wildcard for any name.
        matches_pattern AS (
            SELECT
                row_number() OVER (ORDER BY node_id) - 1 as idx,
                type as pat_type,
                name as pat_name
            FROM parse_ast(
                regexp_extract(selector, ':matches\("([^"]+)"\)', 1),
                COALESCE(language, 'python'))
            WHERE type NOT IN ('module', 'expression_statement', 'program', 'source_file')
        ),
        matches_len AS (
            SELECT COUNT(*) as len FROM matches_pattern
        ),

        -- =====================================================================
        -- Apply selector: single scan with CASE dispatch (no UNION ALL)
        -- =====================================================================

        -- Precompute selector properties as scalars
        sel_props AS (
            SELECT
                (SELECT type FROM root_type) as sel_type,
                (SELECT type_filter FROM simple_type) as type_filter,
                (SELECT type_filter || '_%' FROM simple_type) as type_filter_like,
                (SELECT name_filter FROM simple_id) as name_filter,
                (SELECT class_filter FROM simple_class) as class_filter,
                (SELECT left_type FROM combinator_parts) as left_type,
                (SELECT left_type || '_%' FROM combinator_parts) as left_type_like,
                (SELECT right_type FROM combinator_parts) as right_type,
                (SELECT right_type || '_%' FROM combinator_parts) as right_type_like
        )

    -- Single scan of ast with CASE-based dispatch
    SELECT a.*
    FROM ast a, sel_props sp
    WHERE CASE sp.sel_type

        -- Simple/compound selectors
        -- Bare type matching: `if` matches both `if` and `if_statement`, `if_clause`, etc.
        WHEN 'tag_name' THEN
            (sp.type_filter IS NULL OR a.type = sp.type_filter OR a.type LIKE sp.type_filter_like)
            AND (sp.name_filter IS NULL OR a.name = sp.name_filter)
        WHEN 'id_selector' THEN
            (sp.type_filter IS NULL OR a.type = sp.type_filter OR a.type LIKE sp.type_filter_like)
            AND (sp.name_filter IS NULL OR a.name = sp.name_filter)
        WHEN 'class_selector' THEN
            (sp.class_filter IS NULL
             OR (is_semantic_type(a.semantic_type, UPPER(sp.class_filter))
                 AND NOT is_syntax_only(a.flags)))
        WHEN 'attribute_selector' THEN
            (sp.type_filter IS NULL OR a.type = sp.type_filter OR a.type LIKE sp.type_filter_like)
        WHEN 'pseudo_class_selector' THEN
            (sp.type_filter IS NULL OR a.type = sp.type_filter OR a.type LIKE sp.type_filter_like)
            AND (sp.name_filter IS NULL OR a.name = sp.name_filter)
            AND (sp.class_filter IS NULL
                 OR (is_semantic_type(a.semantic_type, UPPER(sp.class_filter))
                     AND NOT is_syntax_only(a.flags)))

        -- Descendant combinator: A B
        WHEN 'descendant_selector' THEN
            (a.type = sp.right_type OR a.type LIKE sp.right_type_like)
            AND EXISTS (
                SELECT 1 FROM ast anc
                WHERE anc.file_path = a.file_path
                  AND a.node_id > anc.node_id
                  AND a.node_id <= anc.node_id + anc.descendant_count
                  AND (anc.type = sp.left_type OR anc.type LIKE sp.left_type_like)
            )

        -- Child combinator: A > B
        WHEN 'child_selector' THEN
            (a.type = sp.right_type OR a.type LIKE sp.right_type_like)
            AND EXISTS (
                SELECT 1 FROM ast par
                WHERE par.node_id = a.parent_id
                  AND (par.type = sp.left_type OR par.type LIKE sp.left_type_like)
            )

        -- General sibling: A ~ B
        WHEN 'sibling_selector' THEN
            (a.type = sp.right_type OR a.type LIKE sp.right_type_like)
            AND EXISTS (
                SELECT 1 FROM ast sib
                WHERE sib.file_path = a.file_path
                  AND sib.parent_id = a.parent_id
                  AND sib.sibling_index < a.sibling_index
                  AND (sib.type = sp.left_type OR sib.type LIKE sp.left_type_like)
            )

        -- Adjacent sibling: A + B
        WHEN 'adjacent_sibling_selector' THEN
            (a.type = sp.right_type OR a.type LIKE sp.right_type_like)
            AND EXISTS (
                SELECT 1 FROM ast adj
                WHERE adj.file_path = a.file_path
                  AND adj.parent_id = a.parent_id
                  AND adj.sibling_index = a.sibling_index - 1
                  AND (adj.type = sp.left_type OR adj.type LIKE sp.left_type_like)
            )

        ELSE false
    END
    -- :has() filters (for simple/compound selectors)
    AND NOT EXISTS (
        SELECT 1 FROM has_conditions h
        WHERE NOT EXISTS (
            SELECT 1 FROM ast d
            WHERE d.file_path = a.file_path
              AND d.node_id > a.node_id
              AND d.node_id <= a.node_id + a.descendant_count
              AND (h.has_type IS NULL OR d.type = h.has_type)
              AND (h.has_name IS NULL OR d.name = h.has_name)
              AND (h.has_class IS NULL
                   OR is_semantic_type(d.semantic_type, UPPER(h.has_class)))
        )
    )
    -- :not(:has()) filters
    AND NOT EXISTS (
        SELECT 1 FROM not_has_conditions nh
        WHERE EXISTS (
            SELECT 1 FROM ast d
            WHERE d.file_path = a.file_path
              AND d.node_id > a.node_id
              AND d.node_id <= a.node_id + a.descendant_count
              AND (nh.not_has_type IS NULL OR d.type = nh.not_has_type)
              AND (nh.not_has_name IS NULL OR d.name = nh.not_has_name)
        )
    )
    -- [attr op value] filters — supports native extraction columns and CSS operators
    AND NOT EXISTS (
        SELECT 1 FROM attr_conditions ac
        WHERE NOT CASE
            -- Core columns
            WHEN ac.attr_name = 'name' THEN
                CASE ac.attr_op WHEN '*=' THEN a.name LIKE '%' || ac.attr_value || '%'
                                WHEN '^=' THEN a.name LIKE ac.attr_value || '%'
                                WHEN '$=' THEN a.name LIKE '%' || ac.attr_value
                                ELSE a.name = ac.attr_value END
            WHEN ac.attr_name = 'type' THEN a.type = ac.attr_value
            WHEN ac.attr_name = 'language' THEN a.language = ac.attr_value
            WHEN ac.attr_name = 'semantic' THEN is_semantic_type(a.semantic_type, UPPER(ac.attr_value))

            -- Native extraction: modifiers array
            WHEN ac.attr_name = 'modifier' THEN
                CASE ac.attr_op WHEN '*=' THEN list_contains(a.modifiers, ac.attr_value)
                                ELSE list_contains(a.modifiers, ac.attr_value) END

            -- Native extraction: annotations string
            WHEN ac.attr_name = 'annotation' THEN
                CASE ac.attr_op WHEN '*=' THEN a.annotations LIKE '%' || ac.attr_value || '%'
                                WHEN '^=' THEN a.annotations LIKE ac.attr_value || '%'
                                WHEN '$=' THEN a.annotations LIKE '%' || ac.attr_value
                                ELSE a.annotations = ac.attr_value END

            -- Native extraction: qualified_name
            WHEN ac.attr_name = 'qualified' THEN
                CASE ac.attr_op WHEN '*=' THEN a.qualified_name LIKE '%' || ac.attr_value || '%'
                                WHEN '^=' THEN a.qualified_name LIKE ac.attr_value || '%'
                                WHEN '$=' THEN a.qualified_name LIKE '%' || ac.attr_value
                                ELSE a.qualified_name = ac.attr_value END

            -- Native extraction: signature_type
            WHEN ac.attr_name = 'signature' THEN
                CASE ac.attr_op WHEN '*=' THEN a.signature_type LIKE '%' || ac.attr_value || '%'
                                ELSE a.signature_type = ac.attr_value END

            -- Native extraction: parameter count
            WHEN ac.attr_name = 'params' THEN
                len(a.parameters) = CAST(ac.attr_value AS INTEGER)

            -- Peek (source text) content search
            WHEN ac.attr_name = 'peek' THEN
                CASE ac.attr_op WHEN '*=' THEN a.peek LIKE '%' || ac.attr_value || '%'
                                ELSE a.peek = ac.attr_value END

            ELSE false
        END
    )
    -- Additional pseudo-class filters
    -- Each pseudo-class in the selector must be satisfied
    AND NOT EXISTS (
        SELECT 1 FROM pseudo_classes pc
        WHERE NOT CASE pc.pseudo_name

            -- Standard CSS positional pseudo-classes
            WHEN 'first-child' THEN a.sibling_index = 0
            WHEN 'last-child' THEN NOT EXISTS (
                SELECT 1 FROM ast sib
                WHERE sib.parent_id = a.parent_id
                  AND sib.file_path = a.file_path
                  AND sib.sibling_index > a.sibling_index
            )
            WHEN 'nth-child' THEN a.sibling_index = CAST(pc.pseudo_arg AS INTEGER) - 1
            WHEN 'empty' THEN a.children_count = 0
            WHEN 'root' THEN a.depth = 0

            -- :named — has a non-empty name
            WHEN 'named' THEN a.name IS NOT NULL AND a.name != ''

            -- :syntax — is a syntax-only token (keyword, punctuation)
            WHEN 'syntax' THEN is_syntax_only(a.flags)

            -- :definition / :reference / :declaration — NAME_ROLE flags
            WHEN 'definition' THEN is_name_definition(a.flags)
            WHEN 'reference' THEN is_name_reference(a.flags)
            WHEN 'declaration' THEN is_name_declaration(a.flags)

            -- Native extraction pseudo-classes (from modifiers/annotations)
            WHEN 'async' THEN list_contains(a.modifiers, 'async')
            WHEN 'static' THEN list_contains(a.modifiers, 'static')
            WHEN 'abstract' THEN list_contains(a.modifiers, 'abstract')
            WHEN 'const' THEN list_contains(a.modifiers, 'const') OR list_contains(a.modifiers, 'final')
            WHEN 'public' THEN list_contains(a.modifiers, 'public')
            WHEN 'private' THEN list_contains(a.modifiers, 'private')
            WHEN 'protected' THEN list_contains(a.modifiers, 'protected')
            WHEN 'decorated' THEN a.annotations IS NOT NULL AND a.annotations != ''
            WHEN 'typed' THEN a.signature_type IS NOT NULL AND a.signature_type != ''
            WHEN 'void' THEN a.signature_type IS NULL OR a.signature_type = '' OR a.signature_type = 'void' OR a.signature_type = 'None'
            WHEN 'variadic' THEN a.parameters::VARCHAR LIKE '%*%' OR a.parameters::VARCHAR LIKE '%...%'

            -- :matches("code") — exact structural substring match
            -- The parsed pattern must appear as a contiguous node slice in the
            -- target's subtree. db.execute() matches zero-arg calls only.
            -- Use ___ (triple underscore) as wildcard for any name.
            WHEN 'matches' THEN (SELECT len FROM matches_len) > 0 AND EXISTS (
                SELECT 1 FROM ast t
                WHERE t.file_path = a.file_path
                  AND t.node_id >= a.node_id
                  AND t.node_id <= a.node_id + a.descendant_count
                  -- Root type matches
                  AND t.type = (SELECT pat_type FROM matches_pattern WHERE idx = 0)
                  -- All pattern nodes match contiguously
                  AND NOT EXISTS (
                      SELECT 1 FROM matches_pattern mp
                      JOIN ast t2 ON t2.node_id = t.node_id + mp.idx
                                 AND t2.file_path = a.file_path
                      WHERE t2.type != mp.pat_type
                         OR (mp.pat_name IS NOT NULL AND mp.pat_name != ''
                             AND mp.pat_name != '___' AND t2.name != mp.pat_name)
                  )
                  -- Verify we have enough nodes
                  AND t.node_id + (SELECT len FROM matches_len) - 1
                      <= a.node_id + a.descendant_count
            )

            -- :scope — either bare (is a scope node) or with arg (within scope of type)
            WHEN 'scope' THEN
                CASE WHEN pc.pseudo_arg IS NULL
                    -- Bare :scope — node is a scope boundary
                    THEN is_scope(a.flags)
                    -- :scope(type) — node is within the nearest ancestor of that type,
                    -- excluding subtrees of nested nodes of the same type
                    ELSE EXISTS (
                        SELECT 1 FROM ast scope_anc
                        WHERE scope_anc.file_path = a.file_path
                          AND a.node_id > scope_anc.node_id
                          AND a.node_id <= scope_anc.node_id + scope_anc.descendant_count
                          AND (scope_anc.type = pc.pseudo_arg
                               OR scope_anc.type LIKE pc.pseudo_arg || '_%')
                          -- Exclude if there's a CLOSER ancestor of the same type between us
                          AND NOT EXISTS (
                              SELECT 1 FROM ast nested
                              WHERE nested.file_path = a.file_path
                                AND nested.node_id > scope_anc.node_id
                                AND nested.node_id < a.node_id
                                AND a.node_id <= nested.node_id + nested.descendant_count
                                AND (nested.type = pc.pseudo_arg
                                     OR nested.type LIKE pc.pseudo_arg || '_%')
                          )
                    )
                END

            -- :precedes(type) — this node comes before a sibling of the given type
            WHEN 'precedes' THEN EXISTS (
                SELECT 1 FROM ast after_sib

)SQLMACRO"
        R"SQLMACRO(
                WHERE after_sib.file_path = a.file_path
                  AND after_sib.parent_id = a.parent_id
                  AND after_sib.sibling_index > a.sibling_index
                  AND (after_sib.type = pc.pseudo_arg
                       OR after_sib.type LIKE pc.pseudo_arg || '_%')
            )

            -- :follows(type) — this node comes after a sibling of the given type
            WHEN 'follows' THEN EXISTS (
                SELECT 1 FROM ast before_sib
                WHERE before_sib.file_path = a.file_path
                  AND before_sib.parent_id = a.parent_id
                  AND before_sib.sibling_index < a.sibling_index
                  AND (before_sib.type = pc.pseudo_arg
                       OR before_sib.type LIKE pc.pseudo_arg || '_%')
            )

            ELSE true  -- Unknown pseudo-classes are ignored (not errors)
        END
    )
    ORDER BY a.file_path, a.node_id;
)SQLMACRO"},
};

} // namespace duckdb
