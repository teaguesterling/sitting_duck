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

