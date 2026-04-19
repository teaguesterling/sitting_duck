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
--
-- For repeated queries against the same source, parse once and use ast_select_from:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_select_from('my_ast', '.class:named');
--   SELECT * FROM ast_select_from('my_ast', '.func:has(return_statement)');
-- =============================================================================

-- =============================================================================
-- ast_select_from: the selector engine, operates on a pre-parsed AST table.
-- The `source` parameter is a table name (string) resolved via query_table().
-- This separates parsing (expensive) from querying (cheap), enabling callers
-- to parse once and query many times with different selectors.
-- =============================================================================
CREATE OR REPLACE MACRO ast_select_from(
    source,
    selector
) AS TABLE
    WITH
        -- Parse the CSS selector FIRST (before source, so DuckDB can evaluate it early).
        -- Uses parse_ast_list_table (table macro wrapping parse_ast_list scalar) instead
        -- of parse_ast (table function) so that `selector` can be a column reference,
        -- not just a literal — required for ast_select_rules to dispatch one ast_select
        -- call per rule via lateral join.
        sel AS (
            SELECT * FROM parse_ast_list_table(selector, 'css')
        ),

        -- =====================================================================
        -- Typed views over `sel`, one per node type the macro cares about.
        -- These exist so downstream CTEs can join different node types as
        -- separate relations rather than self-correlating two aliases of `sel`
        -- (which trips a DuckDB planner bug when ast_select is called with a
        -- column-valued selector — see commit 86587bc for details).
        -- Each typed CTE references `sel` exactly once.
        -- =====================================================================
        sel_tag_names AS (
            SELECT node_id, parent_id, name, depth, descendant_count, sibling_index
            FROM sel WHERE type = 'tag_name'
        ),
        sel_class_names AS (
            SELECT node_id, parent_id, name, depth, descendant_count, sibling_index
            FROM sel WHERE type = 'class_name'
        ),
        sel_id_names AS (
            SELECT node_id, parent_id, name, depth, descendant_count, sibling_index
            FROM sel WHERE type = 'id_name'
        ),
        sel_class_selectors AS (
            SELECT node_id, parent_id, name, depth, descendant_count, sibling_index
            FROM sel WHERE type = 'class_selector'
        ),
        sel_pseudo_classes AS (
            SELECT node_id, parent_id, name, depth, descendant_count, sibling_index
            FROM sel WHERE type = 'pseudo_class_selector'
        ),
        sel_pseudo_elements AS (
            SELECT node_id, parent_id, name, depth, descendant_count, sibling_index
            FROM sel WHERE type = 'pseudo_element_selector'
        ),
        sel_arg_blocks AS (
            SELECT node_id, parent_id, descendant_count
            FROM sel WHERE type = 'arguments'
        ),
        sel_attribute_selectors AS (
            SELECT node_id, parent_id, descendant_count, sibling_index
            FROM sel WHERE type = 'attribute_selector'
        ),
        sel_attribute_names AS (
            SELECT node_id, parent_id, name
            FROM sel WHERE type = 'attribute_name'
        ),
        sel_integer_values AS (
            SELECT node_id, parent_id, name
            FROM sel WHERE type = 'integer_value'
        ),
        sel_string_values AS (
            SELECT node_id, parent_id, name
            FROM sel WHERE type = 'string_value'
        ),
        sel_plain_values AS (
            SELECT node_id, parent_id, name
            FROM sel WHERE type = 'plain_value'
        ),
        sel_attr_op_starswith AS (  -- *=
            SELECT node_id, parent_id FROM sel WHERE type = '*='
        ),
        sel_attr_op_caret AS (       -- ^=
            SELECT node_id, parent_id FROM sel WHERE type = '^='
        ),
        sel_attr_op_dollar AS (      -- $=
            SELECT node_id, parent_id FROM sel WHERE type = '$='
        ),

        -- Parse source files. Keep native context (needed for attribute filters like
        -- [params=N] and pseudo-classes like :decorated, :typed, :async, etc.) but
        -- skip peek computation since no selector feature references it. The +schema
        -- suffix keeps the peek column in the schema as NULL.
        ast AS (
            SELECT * FROM query_table(source)
        ),

        -- Find the top-level selector node (skip stylesheet, ERROR, and pseudo_element wrappers)
        -- Note: bare words like "function_definition" parse as 'identifier' in CSS,
        -- not 'tag_name'. We treat 'identifier' at the root as equivalent to 'tag_name'.
        -- pseudo_element_selector wraps the real selector — unwrap to its first child.
        sel_root_raw AS (
            SELECT node_id, type, name, descendant_count
            FROM sel
            WHERE type NOT IN ('stylesheet', 'ERROR')
              AND depth = (
                  SELECT MIN(depth) FROM sel
                  WHERE type NOT IN ('stylesheet', 'ERROR')
              )
            LIMIT 1
        ),
        -- For pseudo_element_selector roots, we need to "unwrap" to the inner
        -- selector child. Precompute the chosen child node for any pseudo-element
        -- root via a window function so sel_root can do a simple JOIN instead of
        -- correlated LIMIT 1 scalar subqueries.
        sel_pseudo_element_unwrap AS (
            SELECT c.parent_id AS pe_node_id,
                   c.node_id,
                   row_number() OVER (
                       PARTITION BY c.parent_id
                       ORDER BY
                           -- Prefer non-tag, non-syntax children (id_selector, class_selector, etc.)
                           CASE WHEN c.type NOT IN ('::', 'tag_name', 'pseudo_element_selector') THEN 0
                                WHEN c.type = 'tag_name' THEN 1
                                ELSE 2 END,
                           c.sibling_index
                   ) AS rn
            FROM sel c
            WHERE c.parent_id IN (
                SELECT node_id FROM sel_root_raw WHERE type = 'pseudo_element_selector'
            )
              AND c.type NOT IN ('::', 'pseudo_element_selector')
        ),
        -- When the CSS parser wraps a combinator selector inside a
        -- pseudo_class_selector (e.g., "A > B:has(X)" parses as
        -- pseudo_class_selector(child_selector(A,B), :has(X))), unwrap to the
        -- combinator so the combinator CASE branches fire. The :has / :not /
        -- pseudo-class filters still apply because they iterate over all sel
        -- nodes, not just sel_root.
        --
        -- Handles nested pcs wraps too: "A > B:has(X):not(Y):first-child"
        -- parses as pcs(pcs(pcs(child_selector, :has), :not), :first-child),
        -- so the combinator is deep inside. We find ANY combinator that is a
        -- descendant of sel_root_raw AND is outside any arguments block
        -- (i.e., not inside a :has(...) / :not(...) arg, which is where we
        -- DON'T want to re-root). The shallowest such combinator is the one
        -- the user wrote as the base of the selector.
        --
        -- When sel_root_raw isn't a pseudo_class_selector, or when the pcs
        -- has no combinator in its base chain (e.g., plain type:has(...)),
        -- this CTE is empty and sel_root stays as-is.
        sel_pseudo_class_unwrap AS (
            SELECT c.node_id
            FROM sel c
            LEFT JOIN sel_arg_blocks a
              ON c.node_id > a.node_id
             AND c.node_id <= a.node_id + a.descendant_count
            WHERE c.type IN ('child_selector', 'descendant_selector',
                             'sibling_selector', 'adjacent_sibling_selector')
              AND a.node_id IS NULL
              AND c.node_id > (SELECT node_id FROM sel_root_raw
                               WHERE type = 'pseudo_class_selector')
              AND c.node_id <= (SELECT node_id + descendant_count FROM sel_root_raw
                                WHERE type = 'pseudo_class_selector')
            QUALIFY row_number() OVER (ORDER BY c.depth ASC, c.node_id ASC) = 1
        ),
        -- Resolve the chosen_id for each raw root: a combinator descendant of
        -- a pseudo_class_selector takes precedence (handles nested pcs wraps
        -- like "A > B:has(X):not(Y)"), then the unwrapped child of a
        -- pseudo_element_selector, then the raw node itself.
        -- sel_pseudo_class_unwrap is a zero-or-one-row CTE, so CROSS JOIN with
        -- it would be fine; we use LEFT JOIN (... ON true) instead so it's
        -- absent-safe.
        sel_root_resolved AS (
            SELECT raw.node_id AS raw_id,
                   COALESCE(pcu.node_id, u.node_id, raw.node_id) AS chosen_id
            FROM sel_root_raw raw
            LEFT JOIN sel_pseudo_class_unwrap pcu ON true
            LEFT JOIN sel_pseudo_element_unwrap u
              ON u.pe_node_id = raw.node_id AND u.rn = 1
        ),
        sel_root AS (
            SELECT s.node_id,
                   CASE WHEN s.type = 'identifier' THEN 'tag_name' ELSE s.type END as type,
                   s.name,
                   s.descendant_count
            FROM sel s
            JOIN sel_root_resolved r ON s.node_id = r.chosen_id
            LIMIT 1
        ),

        -- =====================================================================
        -- Extract selector components from the parsed CSS AST
        -- =====================================================================

        -- The root selector type determines the query strategy
        root_type AS (
            SELECT type FROM sel_root
        ),

        -- For combinators (descendant, child, sibling): extract left and right types.
        -- Left = first tag_name child of sel_root, Right = last tag_name child.
        -- Refactored to use sel_tag_names + a single window pass instead of two
        -- correlated LIMIT 1 scalar subqueries on sel.
        combinator_parts AS (
            SELECT
                MAX(name) FILTER (WHERE first_rn = 1) AS left_type,
                MAX(name) FILTER (WHERE last_rn = 1) AS right_type
            FROM (
                SELECT t.name,
                       row_number() OVER (ORDER BY t.sibling_index ASC) AS first_rn,
                       row_number() OVER (ORDER BY t.sibling_index DESC) AS last_rn
                FROM sel_tag_names t
                WHERE t.parent_id = (SELECT node_id FROM sel_root)
            )
        ),

        -- For simple/compound selectors: extract type, #name, .class filters.
        -- Type filter: find the tag_name in the selector tree (may be nested in compound selectors).
        -- Exclude tag_names inside :has()/:not() arguments — those are descendant filters, not the base type.
        --
        -- Implementation notes:
        --   * Uses sel_tag_names and sel_arg_blocks (typed views over sel) so we
        --     don't self-correlate `sel` — that pattern hits a DuckDB planner bug
        --     when ast_select is called with a column-valued selector.
        --   * `simple_type_candidates` ranks valid tag_names by depth (shallowest
        --     first) so we can pick the rank-1 row with an UNCORRELATED scalar
        --     subquery (`WHERE rn = 1`) instead of a correlated `LIMIT 1`.
        simple_type_candidates AS (
            SELECT t.name, t.depth,
                   row_number() OVER (ORDER BY t.depth ASC, t.node_id ASC) AS rn
            FROM sel_tag_names t
            LEFT JOIN sel_arg_blocks a
              ON t.node_id > a.node_id
             AND t.node_id <= a.node_id + a.descendant_count
            WHERE t.node_id >= (SELECT node_id FROM sel_root)
              AND t.node_id <= (SELECT node_id + descendant_count FROM sel_root)
              AND a.node_id IS NULL  -- not contained in any arguments block
        ),
        simple_type AS (
            SELECT COALESCE(
                (SELECT name FROM simple_type_candidates WHERE rn = 1),
                -- Root IS a tag_name (bare type selector)
                CASE WHEN (SELECT type FROM root_type) = 'tag_name'
                     THEN (SELECT name FROM sel_root) END
            ) as type_filter
        ),

        -- #id name filter
        -- Top-level #id (not inside :has()/:not() arguments).
        -- Same shape as simple_type: rank candidates by node order, pick rn=1 via
        -- an uncorrelated subquery so we don't self-correlate `sel`.
        simple_id_candidates AS (
            SELECT i.name,
                   row_number() OVER (ORDER BY i.node_id ASC) AS rn
            FROM sel_id_names i
            LEFT JOIN sel_arg_blocks a
              ON i.node_id > a.node_id
             AND i.node_id <= a.node_id + a.descendant_count
            WHERE i.node_id > (SELECT node_id FROM sel_root)
              AND i.node_id <= (SELECT node_id + descendant_count FROM sel_root)
              AND a.node_id IS NULL  -- not inside any arguments block
        ),
        simple_id AS (
            SELECT (SELECT name FROM simple_id_candidates WHERE rn = 1) as name_filter
        ),

        -- .class semantic filter: a class_name whose parent is a class_selector
        -- (not a pseudo_class_selector — :has, :not, etc. also have class_name children).
        -- Anti-joins sel_arg_blocks so .class inside :has(.foo) / :not(.foo)
        -- doesn't leak into the top-level class_filter. Joins typed views
        -- instead of self-correlating sel.
        simple_class_candidates AS (
            SELECT cn.name,
                   row_number() OVER (ORDER BY cn.node_id ASC) AS rn
            FROM sel_class_names cn
            INNER JOIN sel_class_selectors cs ON cs.node_id = cn.parent_id
            LEFT JOIN sel_arg_blocks a
              ON cs.node_id > a.node_id
             AND cs.node_id <= a.node_id + a.descendant_count
            WHERE a.node_id IS NULL  -- not inside any arguments block
        ),
        simple_class AS (
            SELECT (SELECT name FROM simple_class_candidates WHERE rn = 1) as class_filter
        ),

        -- =====================================================================
        -- Helper CTEs for pseudo-class / attribute extraction.
        -- These precompute "first child of type X inside the args of pcs Y"
        -- relationships so the downstream CTEs can use flat LEFT JOINs instead
        -- of correlated LIMIT 1 scalar subqueries (which trip the planner bug
        -- when ast_select is called with a column-valued selector).
        -- =====================================================================

        -- For each pseudo_class_selector pcs, the (single) arguments block under it.
        sel_pcs_to_args AS (
            SELECT pcs.node_id AS pcs_id,
                   a.node_id AS args_id,
                   a.descendant_count AS args_descendants
            FROM sel_pseudo_classes pcs
            JOIN sel_arg_blocks a ON a.parent_id = pcs.node_id
        ),

        -- First tag_name in each pcs's args block
        sel_pcs_first_tag_name AS (
            SELECT pcs_id, name FROM (
                SELECT pa.pcs_id, t.name,
                       row_number() OVER (PARTITION BY pa.pcs_id ORDER BY t.node_id) AS rn
                FROM sel_pcs_to_args pa
                JOIN sel_tag_names t
                  ON t.node_id > pa.args_id
                 AND t.node_id <= pa.args_id + pa.args_descendants
            ) WHERE rn = 1
        ),

        -- First id_name in each pcs's args block
        sel_pcs_first_id_name AS (
            SELECT pcs_id, name FROM (
                SELECT pa.pcs_id, i.name,
                       row_number() OVER (PARTITION BY pa.pcs_id ORDER BY i.node_id) AS rn
                FROM sel_pcs_to_args pa
                JOIN sel_id_names i
                  ON i.node_id > pa.args_id
                 AND i.node_id <= pa.args_id + pa.args_descendants
            ) WHERE rn = 1
        ),

        -- First .class (class_name whose parent is a class_selector) in each pcs's args
        sel_pcs_first_class_name AS (
            SELECT pcs_id, name FROM (
                SELECT pa.pcs_id, cn.name,
                       row_number() OVER (PARTITION BY pa.pcs_id ORDER BY cs.node_id) AS rn
                FROM sel_pcs_to_args pa
                JOIN sel_class_selectors cs
                  ON cs.node_id > pa.args_id
                 AND cs.node_id <= pa.args_id + pa.args_descendants
                JOIN sel_class_names cn ON cn.parent_id = cs.node_id
            ) WHERE rn = 1
        ),

        -- First (tag_name OR integer_value) in each pcs's args — for pseudo_classes pseudo_arg
        sel_pcs_first_tag_or_int AS (
            SELECT pcs_id, name FROM (
                SELECT pa.pcs_id, n.name,
                       row_number() OVER (PARTITION BY pa.pcs_id ORDER BY n.node_id) AS rn
                FROM sel_pcs_to_args pa
                JOIN (
                    SELECT node_id, name FROM sel_tag_names
                    UNION ALL
                    SELECT node_id, name FROM sel_integer_values
                ) n
                  ON n.node_id > pa.args_id
                 AND n.node_id <= pa.args_id + pa.args_descendants
            ) WHERE rn = 1
        ),

        -- First string_value (trimmed) in each pcs's args — for pseudo_classes pseudo_arg
        sel_pcs_first_string AS (
            SELECT pcs_id, trim(name, '"''') AS name FROM (
                SELECT pa.pcs_id, sv.name,
                       row_number() OVER (PARTITION BY pa.pcs_id ORDER BY sv.node_id) AS rn
                FROM sel_pcs_to_args pa
                JOIN sel_string_values sv
                  ON sv.node_id > pa.args_id
                 AND sv.node_id <= pa.args_id + pa.args_descendants
            ) WHERE rn = 1
        ),

        -- :not() argument blocks (the args block of any pseudo_class_selector named 'not')
        sel_not_arg_blocks AS (
            SELECT a.node_id, a.descendant_count
            FROM sel_arg_blocks a
            JOIN sel_pseudo_classes np ON np.node_id = a.parent_id
            JOIN sel_class_names ncn ON ncn.parent_id = np.node_id AND ncn.name = 'not'
        ),

        -- pcs nodes NOT inside any :not() arg block (anti-join helper)
        sel_pcs_outside_not_args AS (
            SELECT pcs.node_id AS pcs_id
            FROM sel_pseudo_classes pcs
            LEFT JOIN sel_not_arg_blocks na
              ON pcs.node_id > na.node_id
             AND pcs.node_id <= na.node_id + na.descendant_count
            GROUP BY pcs.node_id
            HAVING max(na.node_id) IS NULL
        ),

        -- pcs nodes NOT inside ANY arg block (anti-join helper for top-level only)
        sel_pcs_outside_any_args AS (
            SELECT pcs.node_id AS pcs_id
            FROM sel_pseudo_classes pcs
            LEFT JOIN sel_arg_blocks a
              ON pcs.node_id > a.node_id
             AND pcs.node_id <= a.node_id + a.descendant_count
            GROUP BY pcs.node_id
            HAVING max(a.node_id) IS NULL
        ),

        -- attribute_selectors NOT inside any arg block (anti-join helper)
        sel_attr_outside_args AS (
            SELECT s.node_id AS attr_id
            FROM sel_attribute_selectors s
            LEFT JOIN sel_arg_blocks a
              ON s.node_id > a.node_id
             AND s.node_id <= a.node_id + a.descendant_count
            GROUP BY s.node_id
            HAVING max(a.node_id) IS NULL
        ),

        -- For each attribute_selector, look up its child attr_name / value / op
        sel_attr_first_name AS (
            SELECT parent_id AS attr_id, name FROM (
                SELECT an.parent_id, an.name,
                       row_number() OVER (PARTITION BY an.parent_id ORDER BY an.node_id) AS rn
                FROM sel_attribute_names an
            ) WHERE rn = 1
        ),
        sel_attr_first_plain AS (
            SELECT parent_id AS attr_id, name FROM (
                SELECT pv.parent_id, pv.name,
                       row_number() OVER (PARTITION BY pv.parent_id ORDER BY pv.node_id) AS rn
                FROM sel_plain_values pv
            ) WHERE rn = 1
        ),
        sel_attr_first_int AS (
            SELECT parent_id AS attr_id, name FROM (
                SELECT iv.parent_id, iv.name,
                       row_number() OVER (PARTITION BY iv.parent_id ORDER BY iv.node_id) AS rn
                FROM sel_integer_values iv
            ) WHERE rn = 1
        ),

        -- :has() — pseudo_class_selectors named 'has' that are NOT inside :not() args.
        has_conditions AS (
            SELECT pcs.node_id AS has_id,
                   ftn.name AS has_type,
                   fin.name AS has_name,
                   fcn.name AS has_class
            FROM sel_pseudo_classes pcs
            JOIN sel_class_names cn ON cn.parent_id = pcs.node_id AND cn.name = 'has'
            JOIN sel_pcs_outside_not_args nn ON nn.pcs_id = pcs.node_id
            LEFT JOIN sel_pcs_first_tag_name  ftn ON ftn.pcs_id = pcs.node_id
            LEFT JOIN sel_pcs_first_id_name   fin ON fin.pcs_id = pcs.node_id
            LEFT JOIN sel_pcs_first_class_name fcn ON fcn.pcs_id = pcs.node_id
        ),

        -- :not(:has()) conditions — for each :not()'s args block, find a :has() inside it.
        sel_not_to_has AS (
            -- (not_pcs_id, has_pcs_id) pairs: a :has() inside the args of a :not()
            SELECT not_pcs.node_id AS not_pcs_id,
                   has_pcs.node_id AS has_pcs_id
            FROM sel_pseudo_classes not_pcs
            JOIN sel_class_names not_cn ON not_cn.parent_id = not_pcs.node_id AND not_cn.name = 'not'
            JOIN sel_arg_blocks not_args ON not_args.parent_id = not_pcs.node_id
            JOIN sel_pseudo_classes has_pcs
              ON has_pcs.node_id > not_args.node_id
             AND has_pcs.node_id <= not_args.node_id + not_args.descendant_count
            JOIN sel_class_names has_cn ON has_cn.parent_id = has_pcs.node_id AND has_cn.name = 'has'
        ),
        not_has_conditions AS (
            SELECT nh.not_pcs_id AS not_id,
                   ftn.name AS not_has_type,
                   fin.name AS not_has_name,
                   fcn.name AS not_has_class
            FROM sel_not_to_has nh
            LEFT JOIN sel_pcs_first_tag_name   ftn ON ftn.pcs_id = nh.has_pcs_id
            LEFT JOIN sel_pcs_first_id_name    fin ON fin.pcs_id = nh.has_pcs_id
            LEFT JOIN sel_pcs_first_class_name fcn ON fcn.pcs_id = nh.has_pcs_id
        ),

        -- [attr op value] conditions — supports =, *=, ^=, $= operators.
        -- Uses precomputed sel_attr_* helpers for value/op detection so there are
        -- no correlated subqueries.
        attr_conditions AS (
            SELECT fname.name AS attr_name,
                   COALESCE(fplain.name, fint.name) AS attr_value,
                   CASE
                       WHEN starswith.attr_id IS NOT NULL THEN '*='
                       WHEN caret.attr_id    IS NOT NULL THEN '^='
                       WHEN dollar.attr_id   IS NOT NULL THEN '$='
                       ELSE '='
                   END AS attr_op
            FROM sel_attribute_selectors s
            JOIN sel_attr_outside_args outside ON outside.attr_id = s.node_id
            LEFT JOIN sel_attr_first_name  fname  ON fname.attr_id  = s.node_id
            LEFT JOIN sel_attr_first_plain fplain ON fplain.attr_id = s.node_id
            LEFT JOIN sel_attr_first_int   fint   ON fint.attr_id   = s.node_id
            LEFT JOIN (
                SELECT DISTINCT parent_id AS attr_id FROM sel_attr_op_starswith
            ) starswith ON starswith.attr_id = s.node_id
            LEFT JOIN (
                SELECT DISTINCT parent_id AS attr_id FROM sel_attr_op_caret
            ) caret ON caret.attr_id = s.node_id
            LEFT JOIN (
                SELECT DISTINCT parent_id AS attr_id FROM sel_attr_op_dollar
            ) dollar ON dollar.attr_id = s.node_id
        ),

        -- =====================================================================
        -- Additional pseudo-class extraction
        -- =====================================================================

        -- Extract all pseudo-class names for boolean/positional checks
        -- (excludes :has, :not(:has) which are handled separately above).
        -- Includes pseudo-classes wrapped by :not() with negated=true.
        --
        -- Refactored to use precomputed helper CTEs (sel_pcs_first_tag_or_int,
        -- sel_pcs_first_string, sel_pcs_outside_*) instead of correlated LIMIT 1
        -- scalar subqueries on sel.
        pseudo_classes AS (
            -- Top-level pseudo-classes (not negated)
            SELECT cn.name as pseudo_name,
                   COALESCE(ftoi.name, fstr.name) AS pseudo_arg,
                   false as negated
            FROM sel_pseudo_classes pcs
            JOIN sel_class_names cn ON cn.parent_id = pcs.node_id
            JOIN sel_pcs_outside_any_args outside ON outside.pcs_id = pcs.node_id
            LEFT JOIN sel_pcs_first_tag_or_int ftoi ON ftoi.pcs_id = pcs.node_id
            LEFT JOIN sel_pcs_first_string     fstr ON fstr.pcs_id = pcs.node_id
            WHERE cn.name NOT IN ('has', 'not')

            UNION ALL

            -- Pseudo-classes inside a top-level :not() (negated)
            -- :not(:has(...)) is still handled by not_has_conditions, so exclude :has here.
            SELECT cn.name as pseudo_name,
                   COALESCE(ftoi.name, fstr.name) AS pseudo_arg,
                   true as negated
            FROM sel_pseudo_classes pcs
            JOIN sel_class_names cn ON cn.parent_id = pcs.node_id
            -- The inner pseudo-class must be a direct child of :not()'s arguments
            JOIN sel_arg_blocks not_args ON not_args.node_id = pcs.parent_id
            JOIN sel_pseudo_classes not_pcs ON not_pcs.node_id = not_args.parent_id
            JOIN sel_class_names not_cn ON not_cn.parent_id = not_pcs.node_id AND not_cn.name = 'not'
            -- The enclosing :not() must be top-level
            JOIN sel_pcs_outside_any_args outside ON outside.pcs_id = not_pcs.node_id
            LEFT JOIN sel_pcs_first_tag_or_int ftoi ON ftoi.pcs_id = pcs.node_id
            LEFT JOIN sel_pcs_first_string     fstr ON fstr.pcs_id = pcs.node_id
            WHERE cn.name != 'has'
        ),

        -- Detect whether func_apply extension is loaded (provides function_exists)
        has_func_apply AS (
            SELECT EXISTS (
                SELECT 1 FROM duckdb_functions()
                WHERE function_name = 'function_exists'
            ) AS available
        ),

        -- Validate that every pseudo-class name we saw is one we know about.
        -- Unknown names raise an error rather than silently matching/mismatching.
        known_pseudo_class_names(name) AS (
            VALUES ('first-child'), ('last-child'), ('nth-child'), ('empty'), ('root'),
                   ('named'), ('syntax'),
                   ('definition'), ('reference'), ('declaration'),
                   ('async'), ('static'), ('abstract'), ('const'),
                   ('public'), ('private'), ('protected'),
                   ('decorated'), ('typed'), ('void'), ('variadic'),
                   ('calls'), ('called-by'), ('is-called'), ('is-referenced'), ('exported'),
                   ('match'), ('contains'),
                   ('scope'), ('precedes'), ('follows')
        ),
        pseudo_class_validation AS (
            SELECT CASE
                WHEN EXISTS (
                    SELECT 1 FROM pseudo_classes pc
                    WHERE pc.pseudo_name NOT IN (SELECT name FROM known_pseudo_class_names)
                      AND (
                          NOT (SELECT available FROM has_func_apply)
                          OR NOT function_exists(format('ast_selector_predicate_{}', pc.pseudo_name))
                      )
                ) THEN error(
                    CASE WHEN NOT (SELECT available FROM has_func_apply)
                    THEN format(
                        'ast_select: unknown pseudo-class ":{}". '
                        'Custom predicates require the func_apply extension: '
                        'INSTALL func_apply FROM community; LOAD func_apply; '
                        'Then define: CREATE MACRO ast_selector_predicate_{}(node, arg) AS (...)',
                        (SELECT pc.pseudo_name FROM pseudo_classes pc
                         WHERE pc.pseudo_name NOT IN (SELECT name FROM known_pseudo_class_names) LIMIT 1),
                        (SELECT pc.pseudo_name FROM pseudo_classes pc
                         WHERE pc.pseudo_name NOT IN (SELECT name FROM known_pseudo_class_names) LIMIT 1)
                    )
                    ELSE format(
                        'ast_select: unknown pseudo-class ":{}". '
                        'No macro named ast_selector_predicate_{} is registered.',
                        (SELECT pc.pseudo_name FROM pseudo_classes pc
                         WHERE pc.pseudo_name NOT IN (SELECT name FROM known_pseudo_class_names)
                           AND NOT function_exists(format('ast_selector_predicate_{}', pc.pseudo_name))
                         LIMIT 1),
                        (SELECT pc.pseudo_name FROM pseudo_classes pc
                         WHERE pc.pseudo_name NOT IN (SELECT name FROM known_pseudo_class_names)
                           AND NOT function_exists(format('ast_selector_predicate_{}', pc.pseudo_name))
                         LIMIT 1)
                    )
                    END
                )
                WHEN (SELECT COUNT(*) FROM pseudo_classes
                      WHERE pseudo_name IN ('match', 'contains')) > 1
                THEN error(
                    'ast_select: only one :match or :contains pattern per selector. '
                    'To combine multiple patterns, chain ast_select calls via CTE.'
                )
                ELSE true
            END as ok
        ),

        -- Detect pseudo-element (::callers, ::callees, etc.).
        -- Finds the LAST tag_name child of any pseudo_element_selector.
        -- Refactored to use typed CTEs + window function — avoids the correlated
        -- EXISTS that would trip the planner bug on column-valued selectors.
        sel_pe_tag_names_ranked AS (
            SELECT t.name,
                   row_number() OVER (ORDER BY t.sibling_index DESC) AS rn
            FROM sel_tag_names t
            JOIN sel_pseudo_elements pe ON pe.node_id = t.parent_id
        ),
        pseudo_element AS (
            SELECT (SELECT name FROM sel_pe_tag_names_ranked WHERE rn = 1) AS element_name
        ),

        -- :match("code") / :contains("code") — structural code pattern.
        -- The quoted argument is parsed as real code and checked against the target:
        --   :match("code")    — the current node IS the pattern root (exact)
        --   :contains("code") — some descendant IS the pattern root (any depth)
        -- Both share the same parsed pattern. Only the first :match/:contains
        -- occurrence in a selector is extracted (one pattern per selector).
        -- Exact structural match: db.execute() matches only zero-arg calls,
        -- db.execute("SELECT 1") matches only that exact call.
        -- Use ___ (triple underscore) as wildcard for any name.
        --
        -- Language assumption: the pattern is parsed once, in the language of
        -- the first non-NULL row in the ast table. For single-language sources
        -- (the common case) this is exactly what you want. For mixed-language
        -- sources the pattern only matches against nodes from the chosen
        -- language's grammar — patterns won't cross-match across languages.
        -- Per-language pattern parsing is a future enhancement; for now, run
        -- separate ast_select calls per language if you need :match across a
        -- mixed corpus.
        ast_pattern AS (
            SELECT
                row_number() OVER (ORDER BY node.node_id) - 1 as idx,
                node.type as pat_type,
                node.name as pat_name
            FROM (
                SELECT unnest(parse_ast_list(
                    regexp_extract(selector, ':(?:match|contains)\("([^"]+)"\)', 1),
                    COALESCE((SELECT language FROM ast
                              WHERE language IS NOT NULL LIMIT 1), 'python')
                )) as node
            )
            WHERE node.type NOT IN ('module', 'expression_statement', 'program', 'source_file')
        ),
        ast_pattern_len AS (
            SELECT COUNT(*) as len FROM ast_pattern
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
        ),

    -- Dispatch by sel_type using UNION ALL instead of a single CASE.
    -- DuckDB's optimizer decorrelates EXISTS subqueries into always-on hash
    -- joins, so combinator branches embedded inside CASE would execute even
    -- for non-combinator selectors — a 20s+ penalty on large codebases.
    -- UNION ALL with `sp.sel_type = 'X'` equality lets the optimizer prune
    -- unused branches to zero-row inputs, skipping the decorrelated joins.
    --
    -- All three simple filters (type_filter / name_filter / class_filter)
    -- apply consistently to every compound selector root so that e.g.
    -- `.func#name`, `type[attr=val]` match on ALL components, not just the
    -- one that happens to parent the others.
    matched_base AS (
        -- Simple/compound selectors (non-combinator roots).
        -- Bare type matching: `if` matches `if` and `if_statement`, `if_clause`, etc.
        SELECT a.*
        FROM ast a, sel_props sp
        WHERE sp.sel_type IN ('tag_name', 'id_selector', 'class_selector',
                              'attribute_selector', 'pseudo_class_selector')
          AND (sp.type_filter IS NULL OR a.type = sp.type_filter OR a.type LIKE sp.type_filter_like)
          AND (sp.name_filter IS NULL OR a.name = sp.name_filter)
          AND (sp.class_filter IS NULL
               OR (is_semantic_type(a.semantic_type, UPPER(sp.class_filter))
                   AND NOT is_syntax_only(a.flags)))

        UNION ALL

        -- Descendant combinator: A B
        SELECT a.*
        FROM ast a, sel_props sp
        WHERE sp.sel_type = 'descendant_selector'
          AND (a.type = sp.right_type OR a.type LIKE sp.right_type_like)
          AND EXISTS (
              SELECT 1 FROM ast anc
              WHERE anc.file_path = a.file_path
                AND a.node_id > anc.node_id
                AND a.node_id <= anc.node_id + anc.descendant_count
                AND (anc.type = sp.left_type OR anc.type LIKE sp.left_type_like)
          )

        UNION ALL

        -- Child combinator: A > B
        SELECT a.*
        FROM ast a, sel_props sp
        WHERE sp.sel_type = 'child_selector'
          AND (a.type = sp.right_type OR a.type LIKE sp.right_type_like)
          AND EXISTS (
              SELECT 1 FROM ast par
              WHERE par.node_id = a.parent_id
                AND (par.type = sp.left_type OR par.type LIKE sp.left_type_like)
          )

        UNION ALL

        -- General sibling: A ~ B
        SELECT a.*
        FROM ast a, sel_props sp
        WHERE sp.sel_type = 'sibling_selector'
          AND (a.type = sp.right_type OR a.type LIKE sp.right_type_like)
          AND EXISTS (
              SELECT 1 FROM ast sib
              WHERE sib.file_path = a.file_path
                AND sib.parent_id = a.parent_id
                AND sib.sibling_index < a.sibling_index
                AND (sib.type = sp.left_type OR sib.type LIKE sp.left_type_like)
          )

        UNION ALL

        -- Adjacent sibling: A + B
        SELECT a.*
        FROM ast a, sel_props sp
        WHERE sp.sel_type = 'adjacent_sibling_selector'
          AND (a.type = sp.right_type OR a.type LIKE sp.right_type_like)
          AND EXISTS (
              SELECT 1 FROM ast adj
              WHERE adj.file_path = a.file_path
                AND adj.parent_id = a.parent_id
                AND adj.sibling_index = a.sibling_index - 1
                AND (adj.type = sp.left_type OR adj.type LIKE sp.left_type_like)
          )
    ),

    -- Apply :has / :not / attribute / pseudo-class filters to the base matches.
    matched_raw AS (
    SELECT a.*
    FROM matched_base a
    -- :has() filters (for simple/compound selectors)
    WHERE NOT EXISTS (
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
              AND (nh.not_has_class IS NULL
                   OR is_semantic_type(d.semantic_type, UPPER(nh.not_has_class)))
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

            -- Native extraction: qualified_name. The column is a LIST<STRUCT>,
            -- so we render it to the legacy bracket string via
            -- ast_qualified_name_as_string() before applying text comparisons.
            -- Equality (=) also runs against the string form so selectors like
            -- [qualified=F[main]] stay human-writable.
            WHEN ac.attr_name = 'qualified' THEN
                CASE ac.attr_op
                    WHEN '*=' THEN ast_qualified_name_as_string(a.qualified_name) LIKE '%' || ac.attr_value || '%'
                    WHEN '^=' THEN ast_qualified_name_as_string(a.qualified_name) LIKE ac.attr_value || '%'
                    WHEN '$=' THEN ast_qualified_name_as_string(a.qualified_name) LIKE '%' || ac.attr_value
                    ELSE ast_qualified_name_as_string(a.qualified_name) = ac.attr_value
                END

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
    -- Each pseudo-class in the selector must be satisfied.
    -- For non-negated pcs: satisfied iff CASE is true.
    -- For negated pcs (inside :not()): satisfied iff CASE is false.
    -- A pc is unsatisfied when (negated = CASE), so NOT EXISTS of that identifies nodes
    -- where every pc is satisfied.
    AND (SELECT ok FROM pseudo_class_validation)
    AND NOT EXISTS (
        SELECT 1 FROM pseudo_classes pc
        WHERE pc.negated = CASE pc.pseudo_name

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

            -- :calls(name) — this node's scope contains a call to name
            WHEN 'calls' THEN EXISTS (
                SELECT 1 FROM ast callee
                WHERE callee.file_path = a.file_path
                  AND callee.node_id > a.node_id
                  AND callee.node_id <= a.node_id + a.descendant_count
                  AND callee.semantic_type = 'COMPUTATION_CALL'
                  AND (pc.pseudo_arg IS NULL OR callee.name = pc.pseudo_arg)
            )

            -- :called-by(name) — this call site is inside a function named name
            WHEN 'called-by' THEN EXISTS (
                SELECT 1 FROM ast caller_fn
                WHERE caller_fn.file_path = a.file_path
                  AND a.node_id > caller_fn.node_id
                  AND a.node_id <= caller_fn.node_id + caller_fn.descendant_count
                  AND caller_fn.semantic_type = 'DEFINITION_FUNCTION'
                  AND (pc.pseudo_arg IS NULL OR caller_fn.name = pc.pseudo_arg)
                  -- Nearest function (deepest)
                  AND NOT EXISTS (
                      SELECT 1 FROM ast closer
                      WHERE closer.file_path = a.file_path
                        AND a.node_id > closer.node_id
                        AND a.node_id <= closer.node_id + closer.descendant_count
                        AND closer.semantic_type = 'DEFINITION_FUNCTION'
                        AND closer.depth > caller_fn.depth
                  )
            )

            -- :is-called — this function definition is called somewhere in the file
            WHEN 'is-called' THEN
                is_name_definition(a.flags) AND a.name IS NOT NULL AND a.name != ''
                AND EXISTS (
                    SELECT 1 FROM ast ref
                    WHERE ref.file_path = a.file_path
                      AND ref.semantic_type = 'COMPUTATION_CALL'
                      AND ref.name = a.name
                      AND ref.node_id != a.node_id
                )

            -- :is-referenced — this definition is referenced somewhere
            WHEN 'is-referenced' THEN
                is_name_definition(a.flags) AND a.name IS NOT NULL AND a.name != ''
                AND EXISTS (
                    SELECT 1 FROM ast ref
                    WHERE ref.file_path = a.file_path
                      AND is_name_reference(ref.flags)
                      AND ref.name = a.name
                      AND ref.node_id != a.node_id
                )

            -- :exported — module-level public definition
            WHEN 'exported' THEN
                is_name_definition(a.flags)
                AND (a.scope.current IS NULL OR a.scope.current <= 0)
                AND a.name IS NOT NULL AND a.name != ''
                AND a.name NOT LIKE '\_%'

            -- :match("code") — the CURRENT node is the root of the parsed pattern.
            -- Strict: target type must equal pattern root type. Use ___ wildcard
            -- for any name. Unlike :contains, this never iterates descendants —
            -- it's a direct check on node `a`.
            WHEN 'match' THEN (SELECT len FROM ast_pattern_len) > 0
                AND a.type = (SELECT pat_type FROM ast_pattern WHERE idx = 0)
                AND (SELECT len FROM ast_pattern_len) - 1 <= a.descendant_count
                AND NOT EXISTS (
                    SELECT 1 FROM ast_pattern mp
                    JOIN ast t2 ON t2.node_id = a.node_id + mp.idx
                               AND t2.file_path = a.file_path
                    WHERE t2.type != mp.pat_type
                       OR (mp.pat_name IS NOT NULL AND mp.pat_name != ''
                           AND mp.pat_name != '___' AND t2.name != mp.pat_name)
                )

            -- :contains("code") — some descendant of the current node is the
            -- root of the parsed pattern. Equivalent to :has(:match(...)).
            -- Iterates descendants in the DFS node array and runs the same
            -- contiguity check as :match at each candidate root.
            WHEN 'contains' THEN (SELECT len FROM ast_pattern_len) > 0 AND EXISTS (
                SELECT 1 FROM ast t
                WHERE t.file_path = a.file_path
                  AND t.node_id >= a.node_id
                  AND t.node_id <= a.node_id + a.descendant_count
                  AND t.type = (SELECT pat_type FROM ast_pattern WHERE idx = 0)
                  AND NOT EXISTS (
                      SELECT 1 FROM ast_pattern mp
                      JOIN ast t2 ON t2.node_id = t.node_id + mp.idx
                                 AND t2.file_path = a.file_path
                      WHERE t2.type != mp.pat_type
                         OR (mp.pat_name IS NOT NULL AND mp.pat_name != ''
                             AND mp.pat_name != '___' AND t2.name != mp.pat_name)
                  )
                  AND t.node_id + (SELECT len FROM ast_pattern_len) - 1
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

            ELSE apply(
                format('ast_selector_predicate_{}', pc.pseudo_name),
                a,
                pc.pseudo_arg
            )::BOOLEAN
        END
    )),

    -- =====================================================================
    -- Pseudo-element dispatch: navigate FROM matched nodes to related nodes
    -- =====================================================================

    -- The matched nodes (before pseudo-element transformation)
    matched AS (
        SELECT * FROM matched_raw
    ),

    -- ::parent — the parent node
    pe_parent AS (
        SELECT p.* FROM matched m JOIN ast p
          ON p.node_id = m.parent_id AND p.file_path = m.file_path
    ),
    -- ::scope — the enclosing scope node
    pe_scope AS (
        SELECT s.* FROM matched m JOIN ast s
          ON s.node_id = m.scope.current AND s.file_path = m.file_path
    ),
    -- ::next-sibling — the next sibling
    pe_next AS (
        SELECT n.* FROM matched m JOIN ast n
          ON n.parent_id = m.parent_id AND n.file_path = m.file_path
          AND n.sibling_index = m.sibling_index + 1
    ),
    -- ::prev-sibling — the previous sibling
    pe_prev AS (
        SELECT p.* FROM matched m JOIN ast p
          ON p.parent_id = m.parent_id AND p.file_path = m.file_path
          AND p.sibling_index = m.sibling_index - 1
    ),
    -- ::callers — functions that call this function.
    -- Every call node carries scope.function (the node_id of its nearest
    -- enclosing function), precomputed at parse time. So "find the function
    -- that contains each call site" collapses from a range join + correlated
    -- NOT EXISTS into a plain hash join. Same semantics (immediate caller),
    -- ~1000x faster on large codebases.
    pe_callers AS (
        SELECT DISTINCT caller_fn.* FROM matched m
        JOIN ast call_node ON call_node.file_path = m.file_path
          AND call_node.semantic_type = 'COMPUTATION_CALL'
          AND call_node.name = m.name
          AND call_node.node_id != m.node_id
        JOIN ast caller_fn
          ON caller_fn.node_id = call_node.scope.function
         AND caller_fn.file_path = call_node.file_path
    ),
    -- ::callees — calls inside this function (transitive — includes calls
    -- inside nested functions and lambdas).
    --
    -- Kept as a subtree range scan rather than rewriting to
    -- `callee.scope.function = m.node_id` because that would change the
    -- semantics: scope.function points to the IMMEDIATE enclosing function,
    -- so a call inside a lambda/nested function would have scope.function
    -- pointing to the inner function, not to m. With the range scan we
    -- pick up everything textually inside m's body. The range scan is
    -- already cheap for the common pseudo-element case where matched is
    -- a single named function (descendant_count is small).
    pe_callees AS (
        SELECT DISTINCT callee.* FROM matched m
        JOIN ast callee ON callee.file_path = m.file_path
          AND callee.node_id > m.node_id
          AND callee.node_id <= m.node_id + m.descendant_count
          AND callee.semantic_type = 'COMPUTATION_CALL'
          AND callee.name IS NOT NULL AND callee.name != ''
    ),
    -- ::parent-definition — nearest ancestor that is a named definition
    pe_parent_def AS (
        SELECT DISTINCT def.* FROM matched m
        JOIN ast def ON def.file_path = m.file_path
          AND m.node_id > def.node_id
          AND m.node_id <= def.node_id + def.descendant_count
          AND is_name_definition(def.flags)
          AND def.name IS NOT NULL AND def.name != ''
          -- Nearest = deepest
          AND NOT EXISTS (
              SELECT 1 FROM ast closer
              WHERE closer.file_path = m.file_path
                AND m.node_id > closer.node_id
                AND m.node_id <= closer.node_id + closer.descendant_count
                AND is_name_definition(closer.flags)
                AND closer.name IS NOT NULL AND closer.name != ''
                AND closer.depth > def.depth
          )
    )

    -- Route to correct output based on pseudo-element (or return matched as-is)
    SELECT * FROM matched
    WHERE (SELECT element_name FROM pseudo_element) IS NULL
    UNION ALL
    SELECT * FROM pe_parent
    WHERE (SELECT element_name FROM pseudo_element) = 'parent'
    UNION ALL
    SELECT * FROM pe_parent_def
    WHERE (SELECT element_name FROM pseudo_element) = 'parent-definition'
    UNION ALL
    SELECT * FROM pe_scope
    WHERE (SELECT element_name FROM pseudo_element) = 'scope'
    UNION ALL
    SELECT * FROM pe_next
    WHERE (SELECT element_name FROM pseudo_element) = 'next-sibling'
    UNION ALL
    SELECT * FROM pe_prev
    WHERE (SELECT element_name FROM pseudo_element) IN ('prev-sibling', 'previous-sibling')
    UNION ALL
    SELECT * FROM pe_callers
    WHERE (SELECT element_name FROM pseudo_element) = 'callers'
    UNION ALL
    SELECT * FROM pe_callees
    WHERE (SELECT element_name FROM pseudo_element) = 'callees'
    ORDER BY file_path, node_id;


-- =============================================================================
-- ast_select: convenience wrapper that parses source files and then runs
-- the selector engine. For one-off queries this is the simplest API.
-- For repeated queries, use ast_select_from with a pre-parsed table.
-- =============================================================================
CREATE OR REPLACE MACRO ast_select(
    source,
    selector,
    language := NULL
) AS TABLE
    -- Parse into a temp name, then delegate to ast_select_from.
    -- We use read_ast directly and wrap with ast_select_from via a CTE trick:
    -- DuckDB table macros can accept subqueries as table arguments.
    WITH __ast_src AS (
        SELECT * FROM read_ast(source, language, peek := 'none+schema')
    )
    SELECT * FROM ast_select_from('__ast_src', selector);
