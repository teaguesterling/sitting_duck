-- =============================================================================
-- Scope Resolution Macros
-- =============================================================================
--
-- Macros for scope-aware querying: exports, imports, and name resolution.
-- These use the scope STRUCT column from read_ast().
--
-- scope.current:  nearest enclosing scope's node_id (on every node)
-- scope.function: nearest enclosing function (on every node)
-- scope.class:    nearest enclosing class (on every node)
-- scope.module:   nearest enclosing module/namespace (on every node)
-- scope.stack:    LIST<STRUCT<id, kind SEMANTIC_TYPE>> of enclosing scopes,
--                 outermost first (on scope nodes only).
-- =============================================================================

-- =============================================================================
-- ast_exports: Module-level definitions visible outside the file
-- =============================================================================
--
-- Returns definitions at module scope (depth <= 2) that are not private.
-- For Python: excludes _ prefixed names.
-- For other languages: includes all top-level definitions (use :exported
-- pseudo-class or [modifier=public] for language-specific filtering).
--
-- Usage:
--   SELECT * FROM ast_exports('src/**/*.py');
--   SELECT name, type FROM ast_exports('src/*.go') WHERE name = upper(left(name, 1)) || right(name, -1);

CREATE OR REPLACE MACRO ast_exports(
    source,
    language := NULL
) AS TABLE
    SELECT *
    FROM read_ast(source, language)
    WHERE is_name_definition(flags)
      AND (scope.current IS NULL OR scope.current <= 0)  -- module-level scope
      AND name IS NOT NULL
      AND name != ''
      AND name NOT LIKE '\_%'  -- exclude Python-style private
    ORDER BY file_path, start_line;


-- =============================================================================
-- ast_imports: Import statements with source module and imported names
-- =============================================================================
--
-- Extracts (file_path, source_module, imported_name) from import statements.
-- Uses semantic type EXTERNAL_IMPORT to work across languages.
--
-- Usage:
--   SELECT * FROM ast_imports('src/**/*.py');
--   SELECT source_module, imported_name FROM ast_imports('src/*.js');

CREATE OR REPLACE MACRO ast_imports(
    source,
    language := NULL
) AS TABLE
    WITH
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- Find import statements via semantic type
        import_nodes AS (
            SELECT node_id, name as source_module, type as import_type,
                   descendant_count, file_path, start_line, language
            FROM ast
            WHERE is_semantic_type(semantic_type, 'IMPORT')
              AND NOT is_syntax_only(flags)
              AND descendant_count > 0  -- exclude keyword tokens
        ),
        -- Extract imported names: named identifier children of import nodes
        -- that are not the module name itself
        imported_names AS (
            SELECT
                i.file_path,
                i.source_module,
                i.import_type,
                a.name as imported_name,
                i.start_line
            FROM import_nodes i
            JOIN ast a ON a.node_id > i.node_id
              AND a.node_id <= i.node_id + i.descendant_count
              AND a.type IN ('dotted_name', 'identifier', 'import_specifier',
                            'import_default_specifier', 'scoped_identifier')
              AND a.name IS NOT NULL AND a.name != ''
              AND a.name != i.source_module  -- exclude the module name itself
              AND a.depth = (
                  -- Get the shallowest named children (avoid deep duplicates)
                  SELECT MIN(a2.depth) FROM ast a2
                  WHERE a2.node_id > i.node_id
                    AND a2.node_id <= i.node_id + i.descendant_count
                    AND a2.type IN ('dotted_name', 'identifier', 'import_specifier',
                                   'import_default_specifier', 'scoped_identifier')
                    AND a2.name IS NOT NULL AND a2.name != ''
                    AND a2.name != i.source_module
              )
        )
    SELECT file_path, source_module, imported_name, import_type, start_line
    FROM imported_names
    ORDER BY file_path, start_line, imported_name;


-- =============================================================================
-- ast_resolve: Resolve references to their definitions via scope chain
-- =============================================================================
--
-- For each reference node, walks the scope chain to find the nearest
-- enclosing definition with the same name. Returns (ref_node_id, def_node_id)
-- pairs.
--
-- Usage:
--   SELECT * FROM ast_resolve('src/main.py');
--   SELECT r.ref_name, r.def_line, r.scope_hops FROM ast_resolve('src/*.py') r;

CREATE OR REPLACE MACRO ast_resolve(
    source,
    language := NULL
) AS TABLE
    WITH
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- All references with their scope chain (typed stack: LIST<STRUCT<id, kind>>)
        refs AS (
            SELECT
                a.node_id as ref_node_id,
                a.name as ref_name,
                a.type as ref_type,
                a.start_line as ref_line,
                a.file_path,
                a.scope.current AS scope_current,
                -- Get the enclosing-scope node's stack chain.
                s.scope.stack AS scope_stack
            FROM ast a
            JOIN ast s ON s.node_id = a.scope.current AND s.file_path = a.file_path
            WHERE is_name_reference(a.flags)
              AND a.name IS NOT NULL AND a.name != ''
              AND s.scope.stack IS NOT NULL
        ),
        -- Unnest scope chain with position (for distance ranking).
        -- scope.stack entries are STRUCT<id, kind>, so project .id for the join key.
        search_scopes AS (
            SELECT
                r.ref_node_id, r.ref_name, r.ref_type, r.ref_line, r.file_path,
                unnest(r.scope_stack).id as search_scope_id,
                generate_series(1, len(r.scope_stack)) as scope_distance
            FROM refs r
        ),
        -- Find definitions in each scope
        candidates AS (
            SELECT
                ss.ref_node_id,
                ss.ref_name,
                ss.ref_type,
                ss.ref_line,
                ss.file_path,
                ss.scope_distance,
                d.node_id as def_node_id,
                d.type as def_type,
                d.start_line as def_line,
                d.qualified_name as def_qualified_name
            FROM search_scopes ss
            JOIN ast d ON d.scope.current = ss.search_scope_id
              AND d.file_path = ss.file_path
              AND d.name = ss.ref_name
              AND is_name_definition(d.flags)
        ),
        -- Pick the closest definition (highest scope_distance = innermost scope)
        resolved AS (
            SELECT DISTINCT ON (ref_node_id)
                ref_node_id, ref_name, ref_type, ref_line, file_path,
                def_node_id, def_type, def_line, def_qualified_name,
                scope_distance as scope_hops
            FROM candidates
            ORDER BY ref_node_id, scope_distance DESC
        )
    SELECT * FROM resolved
    ORDER BY file_path, ref_line;


-- =============================================================================
-- ast_callees: What does each function call?
-- =============================================================================
--
-- For each function definition, finds all call expressions within its scope.
-- Returns (caller_name, caller_line, callee_name, callee_line) pairs.
--
-- Usage:
--   SELECT * FROM ast_callees('src/**/*.py');
--   SELECT caller, callee FROM ast_callees('src/*.py') WHERE caller = 'main';

CREATE OR REPLACE MACRO ast_callees(
    source,
    language := NULL
) AS TABLE
    -- Every node now carries scope.function — the node_id of its nearest
    -- enclosing function — precomputed at parse time. So "find calls inside
    -- function F" collapses from an O(calls x funcs) range join into a
    -- simple hash join on scope.function. The range-join version this
    -- replaces took up to 20 seconds on DuckDB's own source tree; this
    -- runs in under a second.
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT f.name         AS caller,
           f.qualified_name AS caller_qualified,
           f.start_line   AS caller_line,
           c.file_path,
           c.name         AS callee,
           c.start_line   AS callee_line,
           c.peek         AS call_peek
    FROM ast c
    JOIN ast f
      ON f.node_id = c.scope.function
     AND f.file_path = c.file_path
    WHERE is_semantic_type(c.semantic_type, 'CALL')
      AND c.name IS NOT NULL AND c.name != ''
      AND f.name IS NOT NULL AND f.name != ''
    ORDER BY c.file_path, f.start_line, c.start_line;


-- =============================================================================
-- ast_callers: Who calls a given function?
-- =============================================================================
--
-- Finds all call sites for each function, with the enclosing function as caller.
-- Uses scope.function (precomputed at parse time) as a direct hash lookup —
-- no range join, no ROW_NUMBER window, no nearest-ancestor dance.
--
-- Module-level calls (no enclosing function) get caller = '<module>',
-- caller_line = 0.
--
-- Usage:
--   SELECT * FROM ast_callers('src/**/*.py');
--   SELECT caller, callee FROM ast_callers('src/*.py') WHERE callee = 'execute';

CREATE OR REPLACE MACRO ast_callers(
    source,
    language := NULL
) AS TABLE
    WITH ast AS (
        SELECT * FROM read_ast(source, language)
    )
    SELECT
        COALESCE(f.name, '<module>')       AS caller,
        COALESCE(f.start_line, 0::UINTEGER) AS caller_line,
        c.name       AS callee,
        c.start_line AS call_line,
        c.file_path
    FROM ast c
    LEFT JOIN ast f
      ON f.node_id = c.scope.function
     AND f.file_path = c.file_path
    WHERE is_semantic_type(c.semantic_type, 'CALL')
      AND c.name IS NOT NULL AND c.name != ''
    ORDER BY c.file_path, c.start_line;
