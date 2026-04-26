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
      AND is_exported(flags)
      AND COALESCE(scope.current, 0) = 0
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
        -- Find top-level import nodes via semantic type.
        -- Filters:
        --   name != '' excludes structural wrappers (JS import_clause, Go import_declaration)
        --   parent NOT EXTERNAL_IMPORT excludes sub-import nodes (Python aliased_import,
        --     JS import_specifier) that also carry the semantic type from their parent
        import_nodes AS (
            SELECT a.node_id, a.name as source_module, a.type as import_type,
                   a.descendant_count, a.file_path, a.start_line
            FROM ast a
            LEFT JOIN ast p ON p.node_id = a.parent_id AND p.file_path = a.file_path
            WHERE a.semantic_type = 'EXTERNAL_IMPORT'
              AND NOT is_syntax_only(a.flags)
              AND a.descendant_count > 0
              AND a.name IS NOT NULL AND a.name != ''
              AND (p.semantic_type IS NULL OR p.semantic_type != 'EXTERNAL_IMPORT')
        ),
        -- Locate the 'import' keyword within each import node (split point)
        import_keywords AS (
            SELECT i.node_id as import_id, i.file_path,
                   MIN(a.node_id) as keyword_id
            FROM import_nodes i
            JOIN ast a ON a.parent_id = i.node_id
              AND a.file_path = i.file_path
              AND a.type IN ('import', 'use')
            GROUP BY i.node_id, i.file_path
        ),
        -- Case 1: from-imports (Python import_from_statement)
        -- Imported names are direct children after the 'import' keyword
        from_import_names AS (
            SELECT i.file_path, i.source_module, a.name as imported_name,
                   i.import_type, i.start_line
            FROM import_nodes i
            JOIN import_keywords k ON k.import_id = i.node_id
              AND k.file_path = i.file_path
            JOIN ast a ON a.parent_id = i.node_id
              AND a.file_path = i.file_path
              AND a.node_id > k.keyword_id
              AND a.type IN ('dotted_name', 'identifier')
              AND a.name IS NOT NULL AND a.name != ''
            WHERE i.import_type IN ('import_from_statement')
        ),
        -- Case 2: statement-imports with named specifiers (JS import { X } from 'Y',
        --         Python import X as Y, Go aliased import_spec)
        specifier_names AS (
            SELECT i.file_path, i.source_module, a.name as imported_name,
                   i.import_type, i.start_line
            FROM import_nodes i
            JOIN ast a ON a.node_id > i.node_id
              AND a.node_id <= i.node_id + i.descendant_count
              AND a.file_path = i.file_path
              AND a.type IN ('import_specifier', 'import_default_specifier',
                            'namespace_import', 'aliased_import',
                            'import_spec')
              AND a.name IS NOT NULL AND a.name != ''
            WHERE i.import_type NOT IN ('import_from_statement')
        ),
        -- Case 3: JS default imports (import React from 'react')
        -- The default name is an identifier direct child of import_clause
        js_default_names AS (
            SELECT i.file_path, i.source_module, a.name as imported_name,
                   i.import_type, i.start_line
            FROM import_nodes i
            JOIN ast clause ON clause.parent_id = i.node_id
              AND clause.file_path = i.file_path
              AND clause.type = 'import_clause'
            JOIN ast a ON a.parent_id = clause.node_id
              AND a.file_path = i.file_path
              AND a.type = 'identifier'
              AND a.name IS NOT NULL AND a.name != ''
            WHERE i.import_type NOT IN ('import_from_statement')
              AND NOT EXISTS (
                  SELECT 1 FROM specifier_names s
                  WHERE s.file_path = i.file_path AND s.start_line = i.start_line
              )
        ),
        -- Case 3b: Rust simple use (use std::collections::HashMap)
        -- The imported name is the last identifier child of the
        -- top-level scoped_identifier
        rust_use_names AS (
            SELECT i.file_path, i.source_module, a.name as imported_name,
                   i.import_type, i.start_line
            FROM import_nodes i
            JOIN ast sc ON sc.parent_id = i.node_id
              AND sc.file_path = i.file_path
              AND sc.type = 'scoped_identifier'
            JOIN ast a ON a.parent_id = sc.node_id
              AND a.file_path = i.file_path
              AND a.type = 'identifier'
              AND a.name IS NOT NULL AND a.name != ''
              AND a.node_id = (
                  SELECT MAX(a2.node_id) FROM ast a2
                  WHERE a2.parent_id = sc.node_id
                    AND a2.file_path = i.file_path
                    AND a2.type = 'identifier'
              )
            WHERE i.import_type NOT IN ('import_from_statement')
        ),
        -- Case 3c: Rust use_list imports (use std::io::{Read, Write})
        -- Identifiers inside use_list are the imported names
        use_list_names AS (
            SELECT i.file_path, i.source_module, a.name as imported_name,
                   i.import_type, i.start_line
            FROM import_nodes i
            JOIN ast ul ON ul.node_id > i.node_id
              AND ul.node_id <= i.node_id + i.descendant_count
              AND ul.file_path = i.file_path
              AND ul.type = 'use_list'
            JOIN ast a ON a.parent_id = ul.node_id
              AND a.file_path = i.file_path
              AND a.type = 'identifier'
              AND a.name IS NOT NULL AND a.name != ''
            WHERE i.import_type NOT IN ('import_from_statement')
        ),
        -- Case 4: Bare imports (Python import os, Go import "fmt", Rust use std::io)
        -- No separate imported name — the module itself is the import
        bare_import_names AS (
            SELECT i.file_path, i.source_module,
                   i.source_module as imported_name,
                   i.import_type, i.start_line
            FROM import_nodes i
            WHERE i.source_module IS NOT NULL AND i.source_module != ''
              AND NOT EXISTS (
                  SELECT 1 FROM from_import_names f
                  WHERE f.file_path = i.file_path AND f.start_line = i.start_line
              )
              AND NOT EXISTS (
                  SELECT 1 FROM specifier_names s
                  WHERE s.file_path = i.file_path AND s.start_line = i.start_line
              )
              AND NOT EXISTS (
                  SELECT 1 FROM js_default_names j
                  WHERE j.file_path = i.file_path AND j.start_line = i.start_line
              )
              AND NOT EXISTS (
                  SELECT 1 FROM rust_use_names r
                  WHERE r.file_path = i.file_path AND r.start_line = i.start_line
              )
              AND NOT EXISTS (
                  SELECT 1 FROM use_list_names u
                  WHERE u.file_path = i.file_path AND u.start_line = i.start_line
              )
        ),
        all_imports AS (
            SELECT * FROM from_import_names
            UNION ALL SELECT * FROM specifier_names
            UNION ALL SELECT * FROM js_default_names
            UNION ALL SELECT * FROM rust_use_names
            UNION ALL SELECT * FROM use_list_names
            UNION ALL SELECT * FROM bare_import_names
        )
    SELECT file_path, source_module, imported_name, import_type, start_line
    FROM all_imports
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
-- See also: ast_get_calls/ast_call_graph in tree_navigation.sql for richer
-- call extraction with call-type classification (function/method/constructor/macro).
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
    WHERE c.semantic_type = 'COMPUTATION_CALL'
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
    WHERE c.semantic_type = 'COMPUTATION_CALL'
      AND c.name IS NOT NULL AND c.name != ''
    ORDER BY c.file_path, c.start_line;


-- =============================================================================
-- ast_find_references: Find all uses of a symbol via scope-chain resolution
-- =============================================================================
--
-- Given a target name, finds every reference and call site that resolves to
-- a definition of that name through the scope chain. Handles shadowed names
-- correctly: if two scopes define the same name, only references that resolve
-- to the specific definition are returned.
--
-- Usage:
--   SELECT * FROM ast_find_references('src/**/*.py', 'process');
--   SELECT * FROM ast_find_references('src/main.py', 'x') WHERE ref_kind = 'call';
--   SELECT * FROM ast_find_references('src/*.py', 'Service', language := 'python');

CREATE OR REPLACE MACRO ast_find_references(
    source,
    target_name,
    language := NULL
) AS TABLE
    WITH
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),
        -- Target definitions: all definitions matching the name
        target_defs AS (
            SELECT
                a.node_id AS def_node_id,
                a.name,
                a.type AS def_type,
                a.file_path,
                a.start_line AS def_line,
                a.peek AS def_peek,
                a.scope.current AS def_scope
            FROM ast a
            WHERE is_name_definition(a.flags)
              AND a.name = target_name
        ),
        -- All identifier references to the target name with scope chain.
        -- Excludes name-binding identifiers inside definition/declaration
        -- nodes (e.g., the 'process' identifier inside 'def process():').
        refs AS (
            SELECT
                a.node_id AS ref_node_id,
                a.name AS ref_name,
                a.type AS ref_type,
                a.start_line AS ref_line,
                a.file_path,
                a.parent_id,
                a.scope.current AS scope_current,
                s.scope.stack AS scope_stack
            FROM ast a
            JOIN ast s ON s.node_id = a.scope.current AND s.file_path = a.file_path
            LEFT JOIN ast p ON p.node_id = a.parent_id AND p.file_path = a.file_path
            WHERE is_name_reference(a.flags)
              AND a.name = target_name
              AND s.scope.stack IS NOT NULL
              AND NOT COALESCE(binds_name(p.flags), false)
        ),
        -- Unnest scope chain for each reference
        search_scopes AS (
            SELECT
                r.ref_node_id, r.ref_name, r.ref_type, r.ref_line,
                r.file_path, r.parent_id,
                unnest(r.scope_stack).id AS search_scope_id,
                generate_series(1, len(r.scope_stack)) AS scope_distance
            FROM refs r
        ),
        -- Find which target definition each reference resolves to
        candidates AS (
            SELECT
                ss.ref_node_id,
                ss.ref_line,
                ss.ref_type,
                ss.file_path,
                ss.parent_id,
                ss.scope_distance,
                td.def_node_id,
                td.def_line
            FROM search_scopes ss
            JOIN ast d ON d.scope.current = ss.search_scope_id
              AND d.file_path = ss.file_path
              AND d.name = ss.ref_name
              AND is_name_definition(d.flags)
            JOIN target_defs td ON td.def_node_id = d.node_id
              AND td.file_path = d.file_path
        ),
        -- Pick closest definition per reference (innermost scope)
        resolved_refs AS (
            SELECT DISTINCT ON (ref_node_id)
                ref_node_id, ref_line, ref_type, file_path, parent_id,
                def_node_id, def_line
            FROM candidates
            ORDER BY ref_node_id, scope_distance DESC
        ),
        -- Classify: if the ref's parent is a call node, this is a call site.
        -- For calls, promote to the call node (type, line, peek).
        -- For references, use the parent's peek for surrounding context.
        classified_refs AS (
            SELECT
                rr.file_path,
                target_name AS name,
                CASE
                    WHEN is_function_call(p.semantic_type) THEN 'call'
                    ELSE 'reference'
                END AS ref_kind,
                COALESCE(
                    CASE WHEN is_function_call(p.semantic_type) THEN p.type END,
                    rr.ref_type
                ) AS node_type,
                COALESCE(
                    CASE WHEN is_function_call(p.semantic_type) THEN p.start_line END,
                    rr.ref_line
                ) AS start_line,
                COALESCE(p.peek, ref_node.peek) AS peek,
                COALESCE(f.name, '<module>') AS scope_name,
                rr.def_node_id,
                rr.ref_node_id
            FROM resolved_refs rr
            LEFT JOIN ast p ON p.node_id = rr.parent_id AND p.file_path = rr.file_path
            LEFT JOIN ast ref_node ON ref_node.node_id = rr.ref_node_id AND ref_node.file_path = rr.file_path
            LEFT JOIN ast f ON f.node_id = ref_node.scope.function AND f.file_path = rr.file_path
        ),
        -- Definition sites
        def_sites AS (
            SELECT
                td.file_path,
                target_name AS name,
                'definition' AS ref_kind,
                td.def_type AS node_type,
                td.def_line AS start_line,
                td.def_peek AS peek,
                COALESCE(f.name, '<module>') AS scope_name,
                td.def_node_id
            FROM target_defs td
            LEFT JOIN ast d ON d.node_id = td.def_node_id AND d.file_path = td.file_path
            LEFT JOIN ast f ON f.node_id = d.scope.function AND f.file_path = td.file_path
        ),
        combined AS (
            SELECT file_path, name, ref_kind, node_type, start_line,
                   peek, scope_name, def_node_id
            FROM def_sites
            UNION
            SELECT file_path, name, ref_kind, node_type, start_line,
                   peek, scope_name, def_node_id
            FROM classified_refs
        )
    SELECT
        file_path,
        name,
        ref_kind,
        node_type,
        start_line,
        peek,
        scope_name,
        def_node_id
    FROM combined
    ORDER BY file_path, start_line, ref_kind;
