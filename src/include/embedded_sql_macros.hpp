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
-- Usage: SELECT * FROM ast_definitions(my_ast_table)
CREATE OR REPLACE MACRO ast_definitions(ast_table) AS TABLE
    SELECT
        name,
        CASE
            WHEN is_function_definition(semantic_type) THEN 'function'
            WHEN is_class_definition(semantic_type) THEN 'class'
            WHEN is_variable_definition(semantic_type) THEN 'variable'
            WHEN is_module_definition(semantic_type) THEN 'module'
            WHEN is_type_definition(semantic_type) THEN 'type'
            ELSE 'other'
        END AS definition_type,
        language,
        file_path,
        start_line,
        end_line,
        node_id,
        type,
        semantic_type
    FROM query_table(ast_table)
    WHERE is_definition(semantic_type)
      AND name IS NOT NULL AND name != ''
    ORDER BY file_path, start_line;

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

-- =============================================================================
-- Analysis Macros
-- =============================================================================

-- Compute metrics for all functions in the AST
-- Returns one row per function with complexity metrics.
-- Uses function scope (excludes nested function internals) for accurate counts.
-- Usage: SELECT * FROM ast_function_metrics(my_ast_table) WHERE cyclomatic > 10
CREATE OR REPLACE MACRO ast_function_metrics(ast_table) AS TABLE
    WITH
        -- All function definitions
        functions AS (
            SELECT
                node_id AS func_id,
                name,
                file_path,
                language,
                start_line,
                end_line,
                depth AS func_depth,
                descendant_count
            FROM query_table(ast_table)
            WHERE is_function_definition(semantic_type)
              AND name IS NOT NULL AND name != ''
        ),
        -- For each function, identify nested functions within it
        nested_funcs AS (
            SELECT
                nf.node_id AS nested_id,
                nf.descendant_count AS nested_count,
                f.func_id AS parent_func_id
            FROM functions f
            JOIN query_table(ast_table) nf
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
            LEFT JOIN query_table(ast_table) n
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
        file_path,
        name,
        language,
        start_line,
        end_line,
        end_line - start_line + 1 AS lines,
        return_count,
        conditionals,
        loops,
        conditionals + loops + 1 AS cyclomatic,
        COALESCE(max_node_depth - func_depth, 0) AS max_depth
    FROM function_metrics
    ORDER BY file_path, start_line;

-- Find functions that contain a specific node type
-- Returns functions with at least one matching node in their scope.
-- Uses function scope (excludes nested function internals).
-- Usage: SELECT * FROM ast_functions_containing(my_ast_table, 'try_statement')
-- Usage: SELECT * FROM ast_functions_containing(my_ast_table, 'call') WHERE match_name = 'eval'
CREATE OR REPLACE MACRO ast_functions_containing(ast_table, target_type) AS TABLE
    WITH
        -- All function definitions
        functions AS (
            SELECT
                node_id AS func_id,
                name AS func_name,
                file_path,
                language,
                start_line,
                end_line,
                descendant_count
            FROM query_table(ast_table)
            WHERE is_function_definition(semantic_type)
              AND name IS NOT NULL AND name != ''
        ),
        -- For each function, identify nested functions within it
        nested_funcs AS (
            SELECT
                nf.node_id AS nested_id,
                nf.descendant_count AS nested_count,
                f.func_id AS parent_func_id
            FROM functions f
            JOIN query_table(ast_table) nf

)SQLMACRO"
        R"SQLMACRO(
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
            JOIN query_table(ast_table) n
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
        file_path,
        func_name,
        language,
        start_line AS func_start_line,
        end_line AS func_end_line,
        match_name,
        match_line,
        match_peek
    FROM matches
    ORDER BY file_path, func_start_line, match_line;

-- Analyze nesting depth per function
-- Returns depth statistics for identifying deeply nested code.
-- Uses function scope (excludes nested function internals).
-- Usage: SELECT * FROM ast_nesting_analysis(my_ast_table) WHERE max_depth > 5
CREATE OR REPLACE MACRO ast_nesting_analysis(ast_table) AS TABLE
    WITH
        -- All function definitions
        functions AS (
            SELECT
                node_id AS func_id,
                name,
                file_path,
                language,
                start_line,
                end_line,
                depth AS func_depth,
                descendant_count
            FROM query_table(ast_table)
            WHERE is_function_definition(semantic_type)
              AND name IS NOT NULL AND name != ''
        ),
        -- For each function, identify nested functions within it
        nested_funcs AS (
            SELECT
                nf.node_id AS nested_id,
                nf.descendant_count AS nested_count,
                f.func_id AS parent_func_id
            FROM functions f
            JOIN query_table(ast_table) nf
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
                -- Max relative depth
                MAX(n.depth - f.func_depth) AS max_depth,
                -- Average relative depth
                ROUND(AVG(n.depth - f.func_depth), 2) AS avg_depth,
                -- Count of deeply nested nodes (relative depth > 5)
                COUNT(CASE WHEN n.depth - f.func_depth > 5 THEN 1 END) AS deep_nodes,
                -- Total nodes in scope
                COUNT(*) AS total_nodes
            FROM functions f
            LEFT JOIN query_table(ast_table) n
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
        file_path,
        name,
        language,
        start_line,
        end_line,
        max_depth,
        avg_depth,
        deep_nodes,
        total_nodes
    FROM depth_stats
    ORDER BY max_depth DESC, file_path, start_line;

-- Automated security concern detection
-- Scans for common security anti-patterns across languages.
-- Usage: SELECT * FROM ast_security_audit(my_ast_table) WHERE risk_level = 'high'
CREATE OR REPLACE MACRO ast_security_audit(ast_table) AS TABLE
    WITH
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
                node_id,
                file_path,
                language,
                name,
                start_line,
                peek
            FROM query_table(ast_table)
            WHERE is_function_call(semantic_type)
              AND name IS NOT NULL AND name != ''
        ),
        -- Find the containing function for each call
        containing_funcs AS (
            SELECT
                c.node_id AS call_node_id,
                f.name AS function_name
            FROM calls c
            LEFT JOIN query_table(ast_table) f
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
        file_path,
        language,
        start_line,
        function_name,
        risk_category,
        risk_level,
        finding,
        matched_pattern,
        context
    FROM findings
    ORDER BY
        CASE risk_level WHEN 'high' THEN 1 WHEN 'medium' THEN 2 ELSE 3 END,
        file_path, start_line;

-- Find potentially dead (unused) code
-- Identifies functions and classes that are never referenced in the codebase.
-- Note: This is heuristic - cannot detect dynamic usage or cross-file references
-- when analyzing a single file. Best used on entire codebase.
-- Usage: SELECT * FROM ast_dead_code(my_ast_table)
CREATE OR REPLACE MACRO ast_dead_code(ast_table) AS TABLE
    WITH
        -- All function definitions
        function_defs AS (
            SELECT
                node_id,
                name,
                file_path,
                language,
                start_line,
                end_line,
                type,
                'function' AS definition_type
            FROM query_table(ast_table)
            WHERE is_function_definition(semantic_type)
              AND name IS NOT NULL AND name != ''
              -- Exclude special methods (constructors, dunder methods, etc.)
              AND name NOT LIKE '\_\_%' ESCAPE '\'
              AND name NOT IN ('main', 'setup', 'teardown', 'init', 'constructor')
        ),
        -- All class definitions
        class_defs AS (
            SELECT
                node_id,
                name,
                file_path,
                language,
                start_line,
                end_line,
                type,
                'class' AS definition_type
            FROM query_table(ast_table)
            WHERE is_class_definition(semantic_type)
              AND name IS NOT NULL AND name != ''
        ),
        -- All definitions combined
        all_defs AS (
            SELECT * FROM function_defs
            UNION ALL
            SELECT * FROM class_defs
        ),
        -- All function calls (references to functions)
        function_calls AS (
            SELECT DISTINCT name
            FROM query_table(ast_table)
            WHERE is_function_call(semantic_type)
              AND name IS NOT NULL AND name != ''
        ),
        -- All identifier references (for classes and other references)
        -- Exclude identifiers that are the name of a definition (parent is definition type)
        identifier_refs AS (
            SELECT DISTINCT ast.name
            FROM query_table(ast_table) ast
            LEFT JOIN query_table(ast_table) parent ON parent.node_id = ast.parent_id
            WHERE is_identifier(ast.semantic_type)
              AND ast.name IS NOT NULL AND ast.name != ''
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
        file_path,
        name,
        language,
        start_line,
        end_line,
        type,
        definition_type,
        reason
    FROM dead_code
    ORDER BY file_path, start_line;
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
--   -- Find all eval() calls, capture the argument
--   SELECT * FROM ast_match('code', 'eval(__X__)', 'python');
--
--   -- Find 3-arg calls with literal 2 in middle, capture func and last arg
--   SELECT * FROM ast_match('code', '__F__(__, 2, __X__)', 'python');
--
--   -- Cross-language: Python pattern matches any language
--   SELECT * FROM ast_match('code', '__F__(__X__)', 'python', match_by := 'semantic_type');
--
--   -- Variadic: Find functions with ANY body that ends with return
--   SELECT * FROM ast_match('code', 'def __F__(__):
--       %__BODY<*>__%
--       return __Y__', 'python');
--
--   -- Unnest captures for flat output
--   SELECT m.peek, c.capture, c.name
--   FROM ast_match('code', '__F__(__X__)', 'python') m,
--        LATERAL (SELECT unnest(map_values(m.captures)) as c) sub;
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
-- Legacy: %__X<*>__% or %__X<+>__%
-- HTML: %__<X*>__% or %__<X+>__% or %__<*>__% or %__<+>__%
CREATE OR REPLACE MACRO pattern_has_variadic(pattern_str) AS
    -- Legacy syntax: <*> or <+> inside %__...__%
    regexp_matches(pattern_str, '%__[A-Z]*<[^>]*[*+][^>]*>__%')
    -- HTML syntax: * or + as modifier inside %__<...>__%
    OR regexp_matches(pattern_str, '%__<[^>]*[*+][^>]*>__%');

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
CREATE OR REPLACE MACRO ast_pattern(pattern_str, lang) AS TABLE
    WITH
        pattern_raw AS (
            SELECT * FROM parse_ast(pattern_str, lang)
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
        -- Use is_punctuation(semantic_type) instead of is_syntax_only(flags)
        -- See bug 009: is_syntax_only doesn't work for PARSER_DELIMITER nodes
        is_punctuation(semantic_type) as is_syntax
    FROM pattern_raw
    WHERE depth >= (SELECT min_depth FROM root_depth);

-- =============================================================================
-- Pattern List (Scalar Macro - for use in table macros)
-- Returns all pattern nodes; filtering happens in ast_match based on match_syntax
-- =============================================================================

-- Note: Takes a CLEANED pattern string. Use clean_pattern() first for extended wildcards.
CREATE OR REPLACE MACRO ast_pattern_list(pattern_str, lang) AS (
    SELECT list({
        rel_depth: rel_depth,
        sibling_index: sibling_index,
        pattern_type: pattern_type,
        pattern_name: pattern_name,
        pattern_semantic_type: pattern_semantic_type,
        pattern_descendant_count: descendant_count,
        is_wildcard: is_wildcard,
        capture_name: capture_name,
        is_syntax: is_syntax
    })
    FROM ast_pattern(pattern_str, lang)
);

-- =============================================================================
-- Main Matching Macro
-- =============================================================================

-- Find AST nodes matching a pattern
-- Usage:
--   SELECT * FROM ast_match('my_ast_table', 'eval(__X__)', 'python');
--   SELECT * FROM ast_match('my_ast_table', 'eval(__X__)', 'python', match_syntax := true);
--   SELECT * FROM ast_match('my_ast_table', '__F__(__X__)', 'python', match_by := 'semantic_type');
--
-- Parameters:
--   ast_table    - Name of table containing AST data (from read_ast)
--   pattern_str  - Code pattern with __WILDCARDS__ (e.g., 'eval(__X__)')
--   lang         - Language for parsing pattern (default: 'python')
--   match_syntax - If true, punctuation must match exactly (default: false)
--   match_by     - 'type' for tree-sitter types, 'semantic_type' for cross-language (default: 'type')
--
CREATE OR REPLACE MACRO ast_match(
    ast_table,
    pattern_str,
    lang := 'python',
    match_syntax := false,
    match_by := 'type',
    depth_fuzz := 0  -- Allow +/- this many levels of depth flexibility
) AS TABLE
    WITH
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
                -- Pattern-level variadic detection: * or + in any extended wildcard syntax
                -- Legacy: %__X<*>__%, HTML: %__<X*>__% or %__<*>__%
                (regexp_matches(pattern_str, '%__[A-Z]*<[^>]*[*+][^>]*>__%')
                 OR regexp_matches(pattern_str, '%__<[^>]*[*+][^>]*>__%')) as pattern_has_variadic,
                -- Per-wildcard variadic detection: this specific wildcard has variadic syntax
                -- Legacy: %__NAME<*>__%, HTML: %__<NAME*>__%
                unnest.capture_name IS NOT NULL AND (
                    -- Legacy syntax

)SQLMACRO"
        R"SQLMACRO(
                    pattern_str LIKE '%' || '%__' || unnest.capture_name || '<*>__%' || '%'
                    OR pattern_str LIKE '%' || '%__' || unnest.capture_name || '<+>__%' || '%'
                    -- HTML syntax
                    OR pattern_str LIKE '%' || '%__<' || unnest.capture_name || '*%>__%' || '%'
                    OR pattern_str LIKE '%' || '%__<' || unnest.capture_name || '+%>__%' || '%'
                ) as is_variadic
            FROM (SELECT unnest(ast_pattern_list(
                -- Clean pattern: handle both HTML and legacy syntax
                regexp_replace(
                    regexp_replace(
                        regexp_replace(
                            regexp_replace(pattern_str, '%__([A-Z][A-Z0-9_]*)<[^>]+>__%', '__\1__', 'g'),
                            '%__<[^>]+>__%', '__', 'g'),
                        '%__<([A-Z_][A-Z0-9_]*)[^>]*>__%', '__\1__', 'g'),
                    '%__<[*+~?][^>]*>__%', '__', 'g'),
                lang)) as unnest)
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
            FROM query_table(ast_table) t, pattern_root pr
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
                  AND NOT EXISTS (
                      SELECT 1
                      FROM query_table(ast_table) t
                      WHERE t.file_path = c.file_path  -- Must be same file (node_ids are per-file)
                        AND t.node_id >= c.candidate_root
                        AND t.node_id <= c.candidate_root + c.candidate_descendants
                        -- Depth matching with optional fuzz (cast to INTEGER to avoid UINT32 overflow)
                        AND ABS((t.depth::INTEGER - c.candidate_depth::INTEGER) - p.rel_depth::INTEGER) <= depth_fuzz
                        -- Sibling matching: exact when no variadics, relaxed when variadics present
                        AND (p.rel_depth = 0
                             OR p.pattern_has_variadic  -- Skip sibling check if variadic present
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

        -- Extract captured nodes for each wildcard in each match
        -- Note: variadic wildcards are not captured as single values (would need LIST)
        captures_raw AS (
            SELECT
                mc.candidate_root,
                mc.file_path as candidate_file,
                mc.candidate_depth,
                mc.candidate_descendants,
                p.capture_name,
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
            JOIN query_table(ast_table) t ON
                t.file_path = mc.file_path  -- Must be same file (node_ids are per-file)
                AND t.node_id >= mc.candidate_root
                AND t.node_id <= mc.candidate_root + mc.candidate_descendants
                -- Depth matching with optional fuzz (cast to INTEGER to avoid UINT32 overflow)
                AND ABS((t.depth::INTEGER - mc.candidate_depth::INTEGER) - p.rel_depth::INTEGER) <= depth_fuzz
                -- Sibling matching: exact when no variadics, relaxed when variadics present
                AND (p.rel_depth = 0
                     OR p.pattern_has_variadic  -- Skip sibling check if variadic present
                     OR t.sibling_index = p.sibling_index)
            WHERE p.is_wildcard = true
              AND p.capture_name IS NOT NULL
              AND NOT p.is_variadic  -- Don't capture variadic wildcards (would need LIST)
        ),

        -- Deduplicate captures - prefer nodes at expected sibling position, then deepest
        captures_dedup AS (
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
            FROM captures_raw
            -- Prefer exact sibling match, then closest sibling, then deepest node
            ORDER BY candidate_file, candidate_root, capture_name,
                     ABS(target_sibling_index::INTEGER - pattern_sibling_index::INTEGER),
                     captured_node_id DESC
        ),

        -- Aggregate captures into a MAP for each match
        -- Include capture name in the struct for easy unnesting
        captures_agg AS (
            SELECT
                candidate_file,
                candidate_root,
                map_from_entries(list((
                    capture_name,
                    {
                        capture: capture_name,
                        node_id: captured_node_id,
                        type: captured_type,
                        name: captured_name,
                        peek: captured_peek,
                        start_line: captured_start_line,
                        end_line: captured_end_line
                    }
                ))) as captures
            FROM captures_dedup
            GROUP BY candidate_file, candidate_root
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
                              AND mc.candidate_root = ca.candidate_root;
)SQLMACRO"},
};

} // namespace duckdb
