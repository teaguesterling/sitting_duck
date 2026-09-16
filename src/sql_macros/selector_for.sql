-- =============================================================================
-- ast_selector_for: node -> selector ("Copy selector", as in browser devtools)
-- =============================================================================
--
-- Given one node of a pre-parsed AST table, return selectors that select it, ranked
-- the way a person picks one: exactly this node first, then fewest matches, then
-- shortest. Candidate strategies:
--
--   class_name    .fn#load                            the node's semantic class and name
--   type_name     function_definition#load            its tree-sitter type and name
--   receiver      .call#execute[receiver="db"]        calls: the object invoked on
--   in_function   .fn#load .call#execute              inside the nearest named function
--   in_class      .class#Config .fn#load              inside the nearest named class
--   location      call#execute[file$="app.py"][line=42]   exact fallback
--
-- Every candidate is a selector ast_select_from accepts, and `matches` is how many
-- nodes it selects in the same table. The counts are computed here, in SQL, with the
-- predicates css_selectors.sql applies to that selector shape -- a table function takes
-- literal arguments, so the candidates cannot be run through ast_select_from row by
-- row. css_selectors.sql is the SOURCE OF TRUTH: this file mirrors its matching, so
-- every selector-semantics change there (#139 constituents, #133/#141 combinators,
-- #151 exact bare types) must be reflected here. In particular: a bare type step is an
-- EXACT match (#151, was prefix); the standalone .class arm skips syntax-only tokens and
-- constituents, the combinator arms only syntax-only tokens.
-- test/sql/ast_selector_for.test runs the returned selectors through ast_select_from and
-- requires the same counts, so the two stay in step.
--
-- A name that is not a plain identifier cannot be written as #name, and a file name
-- containing a double quote cannot be written in [file$="..."]; candidates that would
-- need either are left out. Within one file, a name repeated on the same line by
-- nodes of the same type stays ambiguous: `matches` says so.
--
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_selector_for('my_ast', 'src/app.py', 42);
-- =============================================================================
CREATE OR REPLACE MACRO ast_selector_for(
    source,
    target_file,
    target_node_id
) AS TABLE
    WITH
        ast AS (
            SELECT * FROM query_table(source)
        ),
        tgt AS (
            SELECT * FROM ast
            WHERE file_path = target_file AND node_id = target_node_id
        ),
        -- Evaluated as the outer relation of the final join, so it fires even when
        -- there are no candidates to join against.
        target_validation AS (
            SELECT CASE
                WHEN NOT EXISTS (SELECT 1 FROM tgt) THEN error(format(
                    'ast_selector_for: node {} of file "{}" is not in the source table.',
                    target_node_id, target_file))
                ELSE true
            END AS ok
        ),
        -- The preferred .class alias for each semantic super-type
        -- (docs/reference/semantic-aliases.md). Nodes of other super-types get
        -- type-based candidates only.
        class_alias(sem, alias) AS (
            VALUES ('DEFINITION_FUNCTION', 'fn'), ('DEFINITION_CLASS', 'class'),
                   ('DEFINITION_VARIABLE', 'var'), ('DEFINITION_MODULE', 'mod'),
                   ('COMPUTATION_CALL', 'call'), ('COMPUTATION_ACCESS', 'member'),
                   ('EXTERNAL_IMPORT', 'import'), ('FLOW_CONDITIONAL', 'if'),
                   ('FLOW_LOOP', 'loop'), ('FLOW_JUMP', 'jump'),
                   ('ERROR_TRY', 'try'), ('ERROR_CATCH', 'catch'), ('ERROR_THROW', 'throw'),
                   ('LITERAL_STRING', 'str'), ('LITERAL_NUMBER', 'num')
        ),
        t AS (
            SELECT tg.*,
                   ca.alias,
                   regexp_full_match(COALESCE(tg.name, ''), '[A-Za-z_][A-Za-z0-9_]*') AS name_ok,
                   regexp_full_match(tg.type, '[A-Za-z_][A-Za-z0-9_]*') AS type_ok,
                   regexp_full_match(COALESCE(tg.receiver, ''), '[A-Za-z_][A-Za-z0-9_]*') AS receiver_ok,
                   regexp_extract(tg.file_path, '[^/\\]*$') AS base_name,
                   -- escaped the way css_selectors.sql escapes attr values for LIKE
                   replace(replace(replace(regexp_extract(tg.file_path, '[^/\\]*$'),
                           '\', '\\'), '%', '\%'), '_', '\_') AS base_esc,
                   fn.name AS fn_name,
                   regexp_full_match(COALESCE(fn.name, ''), '[A-Za-z_][A-Za-z0-9_]*') AS fn_ok,
                   cl.name AS class_name,
                   regexp_full_match(COALESCE(cl.name, ''), '[A-Za-z_][A-Za-z0-9_]*') AS class_ok
            FROM tgt tg
            LEFT JOIN class_alias ca ON ca.sem = tg.semantic_type::VARCHAR
            LEFT JOIN ast fn ON fn.file_path = tg.file_path AND fn.node_id = tg.scope.function
            LEFT JOIN ast cl ON cl.file_path = tg.file_path AND cl.node_id = tg.scope.class
        ),
        candidates AS (
            -- .class#name: the standalone .class arm
            SELECT 'class_name' AS strategy,
                   '.' || t.alias || '#' || t.name AS selector,
                   (SELECT count(*) FROM ast a
                    WHERE a.name = t.name
                      AND is_semantic_type(a.semantic_type, UPPER(t.alias))
                      AND NOT is_syntax_only(a.flags)
                      AND NOT is_constituent(a.flags)) AS matches
            FROM t
            WHERE t.alias IS NOT NULL AND t.name_ok
              AND NOT is_syntax_only(t.flags) AND NOT is_constituent(t.flags)

            UNION ALL

            -- type#name: the standalone type arm (bare type = exact match, #151)
            SELECT 'type_name',
                   t.type || '#' || t.name,
                   (SELECT count(*) FROM ast a
                    WHERE a.name = t.name
                      AND (a.type = t.type))
            FROM t
            WHERE t.type_ok AND t.name_ok

            UNION ALL

            -- .class#name[receiver="x"]: standalone .class arm + the receiver attribute
            SELECT 'receiver',
                   '.' || t.alias || '#' || t.name || '[receiver="' || t.receiver || '"]',
                   (SELECT count(*) FROM ast a
                    WHERE a.name = t.name
                      AND a.receiver = t.receiver
                      AND is_semantic_type(a.semantic_type, UPPER(t.alias))
                      AND NOT is_syntax_only(a.flags)
                      AND NOT is_constituent(a.flags))
            FROM t
            WHERE t.alias IS NOT NULL AND t.name_ok AND t.receiver_ok
              AND NOT is_syntax_only(t.flags) AND NOT is_constituent(t.flags)

            UNION ALL

            -- .fn#outer .class#name: the descendant combinator arm
            SELECT 'in_function',
                   '.fn#' || t.fn_name || ' .' || t.alias || '#' || t.name,
                   (SELECT count(*) FROM ast a
                    WHERE a.name = t.name
                      AND is_semantic_type(a.semantic_type, UPPER(t.alias))
                      AND NOT is_syntax_only(a.flags)
                      AND EXISTS (
                          SELECT 1 FROM ast anc
                          WHERE anc.file_path = a.file_path
                            AND a.node_id > anc.node_id
                            AND a.node_id <= anc.node_id + anc.descendant_count
                            AND is_semantic_type(anc.semantic_type, 'FN')
                            AND NOT is_syntax_only(anc.flags)
                            AND anc.name = t.fn_name))
            FROM t
            WHERE t.alias IS NOT NULL AND t.name_ok AND t.fn_ok AND NOT is_syntax_only(t.flags)

            UNION ALL

            -- .class#Outer .class#name: the descendant combinator arm
            SELECT 'in_class',
                   '.class#' || t.class_name || ' .' || t.alias || '#' || t.name,
                   (SELECT count(*) FROM ast a
                    WHERE a.name = t.name
                      AND is_semantic_type(a.semantic_type, UPPER(t.alias))
                      AND NOT is_syntax_only(a.flags)
                      AND EXISTS (
                          SELECT 1 FROM ast anc
                          WHERE anc.file_path = a.file_path
                            AND a.node_id > anc.node_id
                            AND a.node_id <= anc.node_id + anc.descendant_count
                            AND is_semantic_type(anc.semantic_type, 'CLASS')
                            AND NOT is_syntax_only(anc.flags)
                            AND anc.name = t.class_name))
            FROM t
            WHERE t.alias IS NOT NULL AND t.name_ok AND t.class_ok AND NOT is_syntax_only(t.flags)

            UNION ALL

            -- type#name[file$="x"][line=N]: the exact fallback (#name left out when the
            -- name cannot be written as one)
            SELECT 'location',
                   t.type || CASE WHEN t.name_ok THEN '#' || t.name ELSE '' END
                          || '[file$="' || t.base_name || '"][line=' || t.start_line || ']',
                   (SELECT count(*) FROM ast a
                    WHERE (a.type = t.type)
                      AND (NOT t.name_ok OR a.name = t.name)
                      AND a.file_path LIKE '%' || t.base_esc ESCAPE '\'
                      AND a.start_line = t.start_line)
            FROM t
            WHERE t.type_ok AND t.base_name != '' AND NOT contains(t.base_name, '"')
        )
    SELECT row_number() OVER (ORDER BY c.matches <> 1, c.matches, length(c.selector), c.selector) AS rank,
           c.selector,
           c.strategy,
           c.matches,
           c.matches = 1 AS is_unique
    FROM target_validation v
    LEFT JOIN candidates c ON true
    WHERE v.ok AND c.selector IS NOT NULL
    ORDER BY rank;
