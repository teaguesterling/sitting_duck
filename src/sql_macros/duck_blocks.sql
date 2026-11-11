-- =============================================================================
-- Duck Blocks Interop: render code structure as document elements
-- =============================================================================
--
-- ast_to_blocks converts parsed ASTs into duck_blocks — the document-element
-- STRUCT spec shared by the markdown / webbed / duck_block_utils extensions
-- (duck_block_utils SPEC_VERSION 1.4; vendored vocabulary + conformance macros
-- in third_party/duck_block_utils/, validated by test/sql/duck_blocks_conformance.test).
--
-- duck_blocks is a SPEC, not a dependency: these macros emit conforming
-- STRUCTs and require nothing to be loaded. If duck_block_utils IS loaded,
-- terminal rendering is one call away:
--
--   LOAD duck_block_utils;
--   PRAGMA duck_block_render;
--   SELECT db_render_blocks(blocks) FROM ast_to_blocks_list('src/main.py');
--
-- Element shape (SPEC_VERSION 1.4):
--   STRUCT(kind VARCHAR, element_type VARCHAR, content VARCHAR, level INTEGER,
--          encoding VARCHAR, attributes MAP(VARCHAR, VARCHAR),
--          element_order INTEGER)
--
-- Spec fine print this producer honors:
--   * kind is always 'block' (we emit no inlines).
--   * A heading's semantic level lives in attributes['heading_level'] as a
--     STRING ('1'..'6'), NOT in the `level` field. `level` is structural
--     nesting depth: required >= 1 for every element since internal 3.0
--     (no NULLs, never semantic). This outline is flat — no section/div
--     containers — so every block we emit is a top-level sibling at level 1.
--   * code blocks carry attributes['language']; metadata uses encoding 'yaml'.
--   * Source positions (agreed with duckeye, 2026-11-11, duckeye#14):
--     attributes['start_line'] and attributes['end_line'] are file line
--     numbers, 1-based and inclusive, as plain decimal digits (no sign,
--     padding, separator or whitespace: DuckDB's ::INTEGER cast is lenient, so
--     a reader cannot rely on it to reject a malformed value).
--       heading: the definition's extent, from its AST node.
--       code:    start_line is the first line; end_line is counted from the
--                content, a final newline excluded.
--     A block with no source position (metadata, summary paragraphs, a row
--     with NULL lines) carries NEITHER key -- never an empty value. Readers
--     must treat both keys as optional.
--   * element_order starts at 0 and increases in document order, restarting
--     per file (each file's element list is a document of its own; when
--     composing multiple files, order by (file_path, element_order)).
--
-- Mapping (style := 'outline', the default):
--   file        -> one leading 'metadata' block (yaml: file_path, language,
--                  definition counts). Files with no definitions yield just
--                  this block (or zero rows with include_metadata := false).
--   definition  -> 'heading' block. heading_level = base_heading_level +
--                  definition-ancestor nesting depth (a top-level def is at
--                  the base), capped at max_heading_level — deeper defs
--                  demote to the cap rather than overflow h6. Content is the
--                  name plus signature when extracted (parameters / return
--                  type from native extraction).
--   body        -> 'code' block. attributes['language'] = source language.
--                  In outline style only LEAF definitions (no nested defs)
--                  get code blocks: a class's body already appears under its
--                  methods' headings, so repeating it would duplicate text.
--
-- Body content is the definition's WHOLE source lines start_line..end_line,
-- cut from the file's root-node text when the table holds the root row
-- (depth 0). Whole lines keep the first line's indent, which a node's own
-- peek loses because it begins at the node's column (duckeye#14). A table
-- without the root row (e.g. only the selected nodes) falls back to the
-- node's own peek: the line numbers stay right; only that first-line indent
-- is lost. Either way the text comes from FULL PEEK. Peek is a presentation substrate, and
-- ast_to_blocks is a presentation feature — this is the sanctioned pairing
-- (peek remains off-limits for correctness features: patching, unparse,
-- fidelity claims). Retained source would be preferred, but no extraction
-- configuration retains per-node source text: source := 'none'/'path'/
-- 'lines'/'full' controls *location* columns only ('full' merely adds
-- start_column/end_column). Consequences:
--   * ast_to_blocks parses internally with peek := 'full'.
--   * ast_to_blocks_from requires the table to carry peek text when bodies
--     are requested; an all-NULL peek column (peek := 'none') raises a
--     targeted error with a re-parse hint rather than rendering empty
--     bodies.
--   * Body fidelity follows the input's extraction config: peek := 'smart'
--     truncates long definitions, and truncated-vs-full peek is not
--     detectable per row — parse with peek := 'full' for complete bodies.
--     (Exact-source extraction is planned v2 work; see the v2 RFC.)

-- =============================================================================
-- ast_to_blocks_from: the converter engine, operates on a pre-parsed AST
-- table (name resolved via query_table). Parse once with
-- read_ast(..., peek := 'full'), convert many times with different styles.
-- =============================================================================
CREATE OR REPLACE MACRO ast_to_blocks_from(
    source,
    style := 'outline',
    include_bodies := true,
    include_metadata := true,
    base_heading_level := 1,
    max_heading_level := 6
) AS TABLE
    WITH
        -- Parameter validation (issue #89 principle: unknown parameter values
        -- must ERROR loudly, never silently render something else).
        params_validation AS (
            SELECT CASE
                WHEN style NOT IN ('outline', 'summary', 'flat') THEN error(
                    'ast_to_blocks: unknown style ''' || style::VARCHAR ||
                    '''. Valid styles: outline, summary, flat.')
                WHEN base_heading_level < 1 OR base_heading_level > 6 THEN error(
                    'ast_to_blocks: base_heading_level must be between 1 and 6 (got ' ||
                    base_heading_level::VARCHAR || ').')
                WHEN max_heading_level < 1 OR max_heading_level > 6 THEN error(
                    'ast_to_blocks: max_heading_level must be between 1 and 6 (got ' ||
                    max_heading_level::VARCHAR || ').')
                ELSE true
            END AS ok
        ),
        ast AS (
            SELECT * FROM query_table(source)
        ),
        files AS (
            SELECT file_path, any_value(language) AS language
            FROM ast
            GROUP BY file_path
        ),
        -- The file's text, for cutting whole lines: the root node's peek.
        roots AS (
            SELECT file_path, any_value(peek) AS root_text, any_value(start_line) AS root_start
            FROM ast
            WHERE depth = 0 AND peek IS NOT NULL
            GROUP BY file_path
        ),
        -- Named definition constructs. The full filter chain (is_definition +
        -- is_construct + non-empty name) is required to exclude keyword tokens
        -- that share their parent's semantic type — see ast_definitions.
        -- Function-scoped variable definitions (parameters, locals) are
        -- implementation detail, not document structure — a function's
        -- parameters already appear in its heading signature, so they are
        -- excluded here (scope.function != 0 means "inside a function").
        defs AS (
            SELECT
                a.file_path,
                a.language,
                a.node_id,
                a.name,
                a.semantic_type,
                a.start_line,
                a.end_line,
                a.descendant_count,
                a.peek,
                a.signature_type,
                a.parameters,
                CASE
                    WHEN is_function_definition(a.semantic_type) THEN 'function'
                    WHEN is_class_definition(a.semantic_type) THEN 'class'
                    WHEN is_variable_definition(a.semantic_type) THEN 'variable'
                    WHEN is_module_definition(a.semantic_type) THEN 'module'
                    WHEN is_type_definition(a.semantic_type) THEN 'type'
                    ELSE 'other'
                END AS definition_kind
            FROM ast a
            WHERE is_definition(a.semantic_type)
              AND is_construct(a.flags)
              AND a.name IS NOT NULL AND a.name != ''
              AND NOT (is_variable_definition(a.semantic_type)
                       AND coalesce(a.scope.function, 0) != 0)
        ),
        -- Bodies come from peek; a table whose peek column was never
        -- materialized (peek := 'none' — every row NULL) can only render
        -- empty code blocks. Materialized peek is never NULL, so all-NULL
        -- means a mis-parsed table, not intent: raise a targeted error with
        -- a re-parse hint (precedent: ast_select's peek_filter_validation;
        -- principle: issue #89). Note this guard cannot distinguish smart
        -- (truncated) from full peek — that difference is documented, not
        -- detectable.
        peek_validation AS (
            SELECT CASE
                WHEN include_bodies AND style IN ('outline', 'flat')
                     AND EXISTS (SELECT 1 FROM defs)
                     AND NOT EXISTS (SELECT 1 FROM defs WHERE peek IS NOT NULL)
                THEN error(
                    'ast_to_blocks: bodies requested but the source has no peek text '
                    '(the peek column is entirely NULL — parsed with peek := ''none''?). '
                    'Re-parse with read_ast(..., peek := ''full''), or pass '
                    'include_bodies := false.')
                ELSE true
            END AS ok
        ),
        def_info AS (
            SELECT
                d.*,
                -- Definition-ancestor nesting depth via DFS pre-order
                -- containment: ancestors are defs whose [node_id,
                -- node_id + descendant_count] range covers this node.
                (SELECT count(*)
                 FROM defs anc
                 WHERE anc.file_path = d.file_path
                   AND anc.node_id < d.node_id
                   AND d.node_id <= anc.node_id + anc.descendant_count) AS nest_depth,
                -- Leaf = contains no nested definitions (same range trick).
                NOT EXISTS (
                    SELECT 1
                    FROM defs child
                    WHERE child.file_path = d.file_path
                      AND child.node_id > d.node_id
                      AND child.node_id <= d.node_id + d.descendant_count) AS is_leaf,
                -- Heading text: name + signature when native extraction
                -- provides one. Functions always show parens; the annotated
                -- type reads ' -> T' for functions, ': T' otherwise. A
                -- signature_type that merely restates the definition kind
                -- (e.g. 'class' on a class) adds nothing and is dropped.
                d.name ||
                CASE WHEN is_function_definition(d.semantic_type)
                     THEN '(' || coalesce(array_to_string(list_transform(d.parameters, lambda p:
                              CASE WHEN p.name IS NULL OR p.name = '' THEN coalesce(p.type, '')
                                   WHEN p.type IS NULL OR p.type = '' THEN p.name
                                   ELSE p.name || ': ' || p.type
                              END), ', '), '') || ')'
                     ELSE '' END ||
                CASE WHEN d.signature_type IS NOT NULL AND d.signature_type != ''
                          AND lower(d.signature_type) != d.definition_kind
                     THEN CASE WHEN is_function_definition(d.semantic_type)
                               THEN ' -> ' ELSE ': ' END || d.signature_type
                     ELSE '' END AS signature,
                -- Whole source lines (see the header); a trailing empty line, from
                -- a node that ends at column 0 of the next line, is not body.
                CASE WHEN r.root_text IS NOT NULL AND d.start_line IS NOT NULL AND d.end_line IS NOT NULL
                     THEN rtrim(array_to_string(
                              string_split(r.root_text, chr(10))[d.start_line - r.root_start + 1
                                                                : d.end_line - r.root_start + 1],
                              chr(10)), chr(10))
                     ELSE d.peek END AS body,
                -- The definition's extent, for its heading. Omitted, not NULL,
                -- when the source has no lines.
                CASE WHEN d.start_line IS NOT NULL AND d.end_line IS NOT NULL
                     THEN MAP {'start_line': d.start_line::VARCHAR,
                               'end_line': d.end_line::VARCHAR}
                     ELSE MAP([], [])::MAP(VARCHAR, VARCHAR) END AS extent_attrs
            FROM defs d
            LEFT JOIN roots r ON r.file_path = d.file_path
        ),
        elements AS (
            -- One metadata block per file (before any definition: sort_node -1).
            SELECT
                f.file_path,
                CAST(-1 AS BIGINT) AS sort_node,
                0 AS sort_part,
                'metadata' AS element_type,
                'file_path: ' || f.file_path || chr(10) ||
                'language: ' || coalesce(f.language, 'unknown') || chr(10) ||
                'definitions: ' || (SELECT count(*) FROM defs d
                                    WHERE d.file_path = f.file_path)::VARCHAR || chr(10) ||
                'functions: ' || (SELECT count(*) FROM defs d
                                  WHERE d.file_path = f.file_path
                                    AND is_function_definition(d.semantic_type))::VARCHAR || chr(10) ||
                'classes: ' || (SELECT count(*) FROM defs d
                                WHERE d.file_path = f.file_path
                                  AND is_class_definition(d.semantic_type))::VARCHAR AS content,
                1 AS level,
                'yaml' AS encoding,
                MAP([], [])::MAP(VARCHAR, VARCHAR) AS attributes
            FROM files f
            WHERE include_metadata

            UNION ALL

            -- One heading per definition. Semantic level goes in
            -- attributes['heading_level'] (string) per spec; structural level
            -- is 1 (flat outline — headings are leaf blocks, not containers).
            SELECT
                d.file_path,
                d.node_id,
                0,
                'heading',
                d.signature,
                1,
                'text',
                map_concat(
                    MAP {'heading_level':
                         CASE WHEN style = 'flat'
                              THEN LEAST(base_heading_level, max_heading_level)
                              ELSE LEAST(base_heading_level + d.nest_depth,
                                         max_heading_level)
                         END::VARCHAR},
                    d.extent_attrs)
            FROM def_info d

            UNION ALL

            -- Summary style: one-line signature paragraph instead of a body.
            SELECT
                d.file_path,
                d.node_id,
                1,
                'paragraph',
                d.definition_kind || ' ' || d.signature ||
                    ' (lines ' || d.start_line::VARCHAR || '-' || d.end_line::VARCHAR || ')',
                1,
                'text',
                MAP([], [])::MAP(VARCHAR, VARCHAR)
            FROM def_info d
            WHERE style = 'summary'

            UNION ALL

            -- Code bodies: leaf defs in outline, every def in flat, never in
            -- summary, never with include_bodies := false. Content is the
            -- definition's whole source lines (see the header). end_line is
            -- counted from that content, a final newline excluded.
            SELECT
                d.file_path,
                d.node_id,
                2,
                'code',
                d.body,
                1,
                'text',
                map_concat(
                    MAP {'language': coalesce(d.language, 'unknown')},
                    CASE WHEN d.start_line IS NOT NULL AND d.body IS NOT NULL
                         THEN MAP {'start_line': d.start_line::VARCHAR,
                                   'end_line': (d.start_line + len(string_split(
                                       CASE WHEN suffix(d.body, chr(10)) THEN d.body[1:-2]
                                            ELSE d.body END, chr(10))) - 1)::VARCHAR}
                         ELSE MAP([], [])::MAP(VARCHAR, VARCHAR) END)
            FROM def_info d
            WHERE include_bodies
              AND (style = 'flat' OR (style = 'outline' AND d.is_leaf))
        ),
        ordered AS (
            SELECT
                e.*,
                CAST(row_number() OVER (
                    PARTITION BY e.file_path
                    ORDER BY e.sort_node, e.sort_part) - 1 AS INTEGER) AS element_order
            FROM elements e
            WHERE (SELECT ok FROM params_validation)
              AND (SELECT ok FROM peek_validation)
        )
    SELECT
        o.file_path,
        o.element_order,
        {
            'kind': 'block',
            'element_type': o.element_type,
            'content': o.content,
            'level': o.level,
            'encoding': o.encoding,
            'attributes': o.attributes,
            'element_order': o.element_order
        } AS block
    FROM ordered o
    ORDER BY o.file_path, o.element_order;

-- =============================================================================
-- ast_to_blocks: convenience wrapper that parses source files (with the
-- peek := 'full' that complete bodies require) and delegates to the
-- converter engine. Same CTE-delegation pattern as
-- ast_select -> ast_select_from.
-- =============================================================================
CREATE OR REPLACE MACRO ast_to_blocks(
    source,
    language := NULL,
    style := 'outline',
    include_bodies := true,
    include_metadata := true,
    base_heading_level := 1,
    max_heading_level := 6
) AS TABLE
    WITH __ast_blocks_src AS (
        SELECT * FROM read_ast(source, language, peek := 'full')
    )
    SELECT * FROM ast_to_blocks_from('__ast_blocks_src',
        style := style,
        include_bodies := include_bodies,
        include_metadata := include_metadata,
        base_heading_level := base_heading_level,
        max_heading_level := max_heading_level);

-- =============================================================================
-- ast_to_blocks_list: one row per file with the blocks pre-aggregated into a
-- LIST(duck_block) ready for consumers that take a whole document, e.g.
-- duck_block_utils rendering:
--
--   SELECT db_render_blocks(blocks) FROM ast_to_blocks_list('src/main.py');
--
-- Equivalent to the general idiom over the row-shaped macro:
--   list(block ORDER BY element_order) ... GROUP BY file_path
-- =============================================================================
CREATE OR REPLACE MACRO ast_to_blocks_list(
    source,
    language := NULL,
    style := 'outline',
    include_bodies := true,
    include_metadata := true,
    base_heading_level := 1,
    max_heading_level := 6
) AS TABLE
    SELECT
        file_path,
        list(block ORDER BY element_order) AS blocks
    FROM ast_to_blocks(source,
        language := language,
        style := style,
        include_bodies := include_bodies,
        include_metadata := include_metadata,
        base_heading_level := base_heading_level,
        max_heading_level := max_heading_level)
    GROUP BY file_path
    ORDER BY file_path;
