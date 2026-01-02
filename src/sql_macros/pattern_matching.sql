-- =============================================================================
-- Pattern Matching Macros for AST Querying
-- =============================================================================
--
-- Pattern-by-example matching system that allows users to write code patterns
-- with placeholders to find matching AST structures.
--
-- WILDCARD SYNTAX:
--   __X__      Named wildcard - captures matched node as 'X'
--   __         Anonymous wildcard - matches any node but doesn't capture
--
-- PARAMETERS:
--   match_syntax := false  - Include punctuation/delimiters in matching
--   match_by := 'type'     - Match on 'type' (tree-sitter) or 'semantic_type' (cross-language)
--
-- FUTURE (requires C++ implementation - see tracker/features/030-pattern-matching-cpp.md):
--   wildcards := {}        - MAP of wildcard modifiers for variadic matching, constraints
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
--   -- Unnest captures for flat output
--   SELECT m.peek, c.capture, c.name
--   FROM ast_match('code', '__F__(__X__)', 'python') m,
--        LATERAL (SELECT unnest(map_values(m.captures)) as c) sub;
--
-- NOTE: Wildcards use UPPERCASE (__NAME__), Python dunders use lowercase (__init__)
--       so they don't conflict.
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
-- Pattern Parsing (Table Macro - for inspection)
-- =============================================================================

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

CREATE OR REPLACE MACRO ast_pattern_list(pattern_str, lang) AS (
    SELECT list({
        rel_depth: rel_depth,
        sibling_index: sibling_index,
        pattern_type: pattern_type,
        pattern_name: pattern_name,
        pattern_semantic_type: pattern_semantic_type,
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
    match_by := 'type'
) AS TABLE
    WITH
        -- Unnest the pattern list into rows
        pattern AS (
            SELECT
                unnest.rel_depth,
                unnest.sibling_index,
                unnest.pattern_type,
                unnest.pattern_name,
                unnest.pattern_semantic_type,
                unnest.is_wildcard,
                unnest.capture_name,
                unnest.is_syntax
            FROM (SELECT unnest(ast_pattern_list(pattern_str, lang)) as unnest)
            -- Filter out syntax nodes unless match_syntax is true
            WHERE match_syntax OR NOT unnest.is_syntax
        ),

        -- Get pattern root info for candidate filtering
        pattern_root AS (
            SELECT pattern_type, pattern_name, pattern_semantic_type, is_wildcard
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
                -- For semantic_type, compare base types (mask off language-specific bits)
                CASE WHEN match_by = 'semantic_type'
                     THEN semantic_type_base(t.semantic_type) = semantic_type_base(pr.pattern_semantic_type)
                     ELSE t.type = pr.pattern_type
                END
                -- Name matching (for non-wildcards)
                AND (pr.pattern_name IS NULL OR pr.pattern_name = ''
                     OR pr.is_wildcard
                     OR t.name = pr.pattern_name)
        ),

        -- For each candidate, verify all non-wildcard pattern nodes have matches
        matched_candidates AS (
            SELECT c.*
            FROM candidates c
            WHERE NOT EXISTS (
                -- Find any non-wildcard pattern node without a corresponding match
                SELECT 1
                FROM pattern p
                WHERE p.is_wildcard = false
                  AND NOT EXISTS (
                      SELECT 1
                      FROM query_table(ast_table) t
                      WHERE t.file_path = c.file_path  -- Must be same file (node_ids are per-file)
                        AND t.node_id >= c.candidate_root
                        AND t.node_id <= c.candidate_root + c.candidate_descendants
                        AND t.depth - c.candidate_depth = p.rel_depth
                        -- Skip sibling_index check for root (it can be at any position)
                        AND (p.rel_depth = 0 OR t.sibling_index = p.sibling_index)
                        -- Match on type or semantic_type based on match_by parameter
                        -- For semantic_type, compare base types (mask off language-specific bits)
                        AND CASE WHEN match_by = 'semantic_type'
                                 THEN semantic_type_base(t.semantic_type) = semantic_type_base(p.pattern_semantic_type)
                                 ELSE t.type = p.pattern_type
                            END
                        AND (p.pattern_name IS NULL OR p.pattern_name = '' OR t.name = p.pattern_name)
                  )
            )
        ),

        -- Extract captured nodes for each wildcard in each match
        captures_raw AS (
            SELECT
                mc.candidate_root,
                mc.file_path as candidate_file,
                mc.candidate_depth,
                mc.candidate_descendants,
                p.capture_name,
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
                AND t.depth - mc.candidate_depth = p.rel_depth
                -- Skip sibling_index check for root (it can be at any position)
                AND (p.rel_depth = 0 OR t.sibling_index = p.sibling_index)
            WHERE p.is_wildcard = true
              AND p.capture_name IS NOT NULL
        ),

        -- Deduplicate captures - take the deepest (most specific) node for each capture name
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
            -- Order by node_id descending to get the deepest/most specific node
            ORDER BY candidate_file, candidate_root, capture_name, captured_node_id DESC
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
