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
