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


)SQLMACRO"},
};

} // namespace duckdb
