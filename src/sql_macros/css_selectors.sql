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
        -- Parse the source files
        ast AS (
            SELECT * FROM read_ast(source, language)
        ),

        -- Parse the CSS selector using tree-sitter-css
        sel AS (
            SELECT * FROM parse_ast(selector, 'css')
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

        -- [attr=value] conditions
        attr_conditions AS (
            SELECT
                (SELECT a.name FROM sel a WHERE a.parent_id = s.node_id AND a.type = 'attribute_name' LIMIT 1) as attr_name,
                (SELECT a.name FROM sel a WHERE a.parent_id = s.node_id AND a.type = 'plain_value' LIMIT 1) as attr_value
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
        -- Apply selector to target AST
        -- =====================================================================

        -- Simple/compound selector result (type, #id, .class, :has, :not(:has), [attr])
        simple_result AS (
            SELECT a.*
            FROM ast a
            WHERE
                -- Type filter
                ((SELECT type_filter FROM simple_type) IS NULL
                 OR a.type = (SELECT type_filter FROM simple_type))
                -- #id name filter
                AND ((SELECT name_filter FROM simple_id) IS NULL
                     OR a.name = (SELECT name_filter FROM simple_id))
                -- .class semantic filter (exclude syntax-only nodes like keywords/punctuation)
                AND ((SELECT class_filter FROM simple_class) IS NULL
                     OR (is_semantic_type(a.semantic_type, UPPER((SELECT class_filter FROM simple_class)))
                         AND NOT is_syntax_only(a.flags)))
                -- [attr=value] filters
                AND NOT EXISTS (
                    SELECT 1 FROM attr_conditions ac
                    WHERE NOT CASE
                        WHEN ac.attr_name = 'name' THEN a.name = ac.attr_value
                        WHEN ac.attr_name = 'type' THEN a.type = ac.attr_value
                        WHEN ac.attr_name = 'language' THEN a.language = ac.attr_value
                        WHEN ac.attr_name = 'semantic' THEN is_semantic_type(a.semantic_type, UPPER(ac.attr_value))
                        ELSE false
                    END
                )
                -- :has() filters — every :has condition must be satisfied
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
                -- :not(:has()) filters — none of these must be satisfied
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
        ),

        -- Descendant selector: A B
        descendant_result AS (
            SELECT b.*
            FROM ast b
            WHERE EXISTS (
                SELECT 1 FROM ast a
                WHERE a.file_path = b.file_path
                  AND b.node_id > a.node_id
                  AND b.node_id <= a.node_id + a.descendant_count
                  AND a.type = (SELECT left_type FROM combinator_parts)
            )
            AND b.type = (SELECT right_type FROM combinator_parts)
        ),

        -- Child selector: A > B
        child_result AS (
            SELECT b.*
            FROM ast b
            WHERE EXISTS (
                SELECT 1 FROM ast a
                WHERE a.node_id = b.parent_id
                  AND a.type = (SELECT left_type FROM combinator_parts)
            )
            AND b.type = (SELECT right_type FROM combinator_parts)
        ),

        -- General sibling selector: A ~ B
        sibling_result AS (
            SELECT b.*
            FROM ast b
            WHERE EXISTS (
                SELECT 1 FROM ast a
                WHERE a.file_path = b.file_path
                  AND a.parent_id = b.parent_id
                  AND a.sibling_index < b.sibling_index
                  AND a.type = (SELECT left_type FROM combinator_parts)
            )
            AND b.type = (SELECT right_type FROM combinator_parts)
        ),

        -- Adjacent sibling selector: A + B
        adjacent_result AS (
            SELECT b.*
            FROM ast b
            WHERE EXISTS (
                SELECT 1 FROM ast a
                WHERE a.file_path = b.file_path
                  AND a.parent_id = b.parent_id
                  AND a.sibling_index = b.sibling_index - 1
                  AND a.type = (SELECT left_type FROM combinator_parts)
            )
            AND b.type = (SELECT right_type FROM combinator_parts)
        )

    -- Route to the correct result based on selector type
    SELECT * FROM simple_result
    WHERE (SELECT type FROM root_type) IN (
        'tag_name', 'pseudo_class_selector', 'id_selector',
        'class_selector', 'attribute_selector'
    )
    UNION ALL
    SELECT * FROM descendant_result
    WHERE (SELECT type FROM root_type) = 'descendant_selector'
    UNION ALL
    SELECT * FROM child_result
    WHERE (SELECT type FROM root_type) = 'child_selector'
    UNION ALL
    SELECT * FROM sibling_result
    WHERE (SELECT type FROM root_type) = 'sibling_selector'
    UNION ALL
    SELECT * FROM adjacent_result
    WHERE (SELECT type FROM root_type) = 'adjacent_sibling_selector'
    ORDER BY file_path, node_id;
