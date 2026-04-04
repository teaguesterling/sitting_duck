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

        -- Parse source files with +schema: compute only normalized context and lines,
        -- but keep all columns in schema so attribute/pseudo-class checks compile
        ast AS (
            SELECT * FROM read_ast(source, language,
                context := 'normalized+schema',
                peek := 'none+schema')
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
        -- Exact structural match: db.execute() matches only zero-arg calls,
        -- db.execute("SELECT 1") matches only that exact call.
        -- Use ___ (triple underscore) as wildcard for any name.
        matches_pattern AS (
            SELECT
                row_number() OVER (ORDER BY node.node_id) - 1 as idx,
                node.type as pat_type,
                node.name as pat_name
            FROM (
                SELECT unnest(parse_ast_list(
                    regexp_extract(selector, ':matches\("([^"]+)"\)', 1),
                    COALESCE(language, 'python')
                )) as node
            )
            WHERE node.type NOT IN ('module', 'expression_statement', 'program', 'source_file')
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
