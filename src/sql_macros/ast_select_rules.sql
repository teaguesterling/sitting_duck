-- =============================================================================
-- ast_select_rules / ast_select_list: Multi-rule CSS selector dispatch
-- =============================================================================
--
-- Parses a multi-rule CSS query like:
--
--   .fn { show: signature; }
--   #main { show: body; }
--   .class#Config { show: outline; expand: .fn#__init__; }
--
-- and matches each rule's selector against the source AST independently,
-- returning per-rule metadata (rule_index, selector text, declarations map)
-- alongside the matched AST nodes.
--
-- Two forms are provided:
--
--   ast_select_rules(source, query, language := NULL)
--     Flat form: one row per (rule, match) pair.
--     Columns: rule_index, selector, declarations, <all read_ast columns>.
--
--   ast_select_list(source, query, language := NULL)
--     Nested form: one row per rule, with matches packed into a LIST<STRUCT>.
--     Columns: rule_index, selector, declarations, matches.
--
-- STATUS (as of commit where this file is introduced): WIP.
--
-- The implementation dispatches ast_select() per rule via a lateral join with
-- the selector as a column reference. Column-valued selector dispatch through
-- ast_select currently crashes at bind time with:
--
--   INTERNAL Error: Failed to bind column reference "" [N.M]:
--   inequal types (BIGINT != VARCHAR)
--
-- This is a DuckDB v1.5.1 planner regression in the dependent-join
-- flattening / ColumnBindingResolver code path. See:
--   - duckdb/duckdb#21890 (related, open, in-progress)
--   - duckdb/duckdb#21604 (related, closed)
--   - duckdb/duckdb#21788 (related, closed)
--   - workspace/sandbox/duckdb_correlated_macro_bug.sql for a standalone repro
--
-- This macro is shipped in the intentional "as-designed" form so that once
-- upstream DuckDB lands the fix, sitting_duck picks up ast_select_rules
-- support with zero additional SQL work. Until then, any call to
-- ast_select_rules or ast_select_list will crash at bind time.
--
-- =============================================================================
-- DESIGN NOTES
-- =============================================================================
--
-- The full query is parsed ONCE using parse_ast() on the literal query string.
-- This works even though parse_ast is a table function with a literal-only
-- restriction — the `query` macro parameter becomes a literal when the macro
-- is called, and the substitution happens before bind.
--
-- Per-rule metadata extraction is straightforward:
--   - rule_sets: identify all `rule_set` nodes, assign source-order rule_index
--   - rule_selectors: extract the selector text from each rule's `selectors`
--     child via `.name` (populated by sitting_duck's CSS grammar)
--   - rule_declarations: MAP(property_name → raw value span), where the value
--     span is the text between `:` and `;`, trimmed. Empty declaration blocks
--     produce an empty MAP, not NULL.
--
-- The declaration values are stored as **raw spans**, not typed values. So for
-- `{ show: signature; }` the map is `{show: 'signature'}`, for `{ depth: 2; }`
-- it's `{depth: '2'}` (VARCHAR — cast as needed), and for a multi-value
-- declaration like `{ trace: callers, dependents; }` it's
-- `{trace: 'callers, dependents'}` (consumer splits). See the earlier design
-- discussion: VARCHAR + raw span is lossless, dependency-free, and upgradeable
-- to MAP<VARCHAR, JSON> later if richer typing is needed.
--
-- The final dispatch is a `LEFT JOIN LATERAL ast_select(source, rsel.selector,
-- language := language)`. LEFT JOIN is important: rules that match nothing
-- still emit one row per rule with NULLs in the read_ast columns, preserving
-- the invariant that N rules in produces N distinct rule_index values out.

CREATE OR REPLACE MACRO ast_select_rules(source, query, language := NULL) AS TABLE
    WITH
        -- Parse the entire query as CSS, one pass. Literal at bind time
        -- because `query` is the macro's own parameter — parse_ast's
        -- table-function literal-only restriction is satisfied via macro
        -- parameter substitution.
        --
        -- IMPORTANT: `query` must be fully-formed CSS, i.e. include braces
        -- even for empty declaration blocks:
        --   GOOD: '.fn#main {}'
        --   GOOD: '.fn#main { show: body; }'
        --   BAD:  '.fn#main'   (bare selector — use ast_select instead)
        --
        -- The natural normalization `CASE WHEN strpos(query,'{')=0 THEN
        -- query || ' {}' ELSE query END` would require a subquery wrapper,
        -- which parse_ast (table function) rejects with "Table function
        -- cannot contain subqueries". ast_select handles bare selectors
        -- just fine; ast_select_rules's niche is multi-rule queries where
        -- callers are expected to include the declaration blocks naturally.
        full_css AS (
            SELECT * FROM parse_ast(query, 'css')
        ),

        -- Each rule_set in the parsed CSS is one CSS rule.
        rule_sets AS (
            SELECT node_id AS rule_id,
                   row_number() OVER (ORDER BY node_id) - 1 AS rule_index
            FROM full_css
            WHERE type = 'rule_set'
        ),

        -- Extract the selector text for each rule. sitting_duck's CSS grammar
        -- populates the `selectors` node's .name field with the raw selector
        -- text (e.g., ".fn#main" or "function_definition:has(return_statement)").
        rule_selectors AS (
            SELECT rs.rule_id,
                   rs.rule_index,
                   sel.name AS selector
            FROM rule_sets rs
            JOIN full_css sel ON sel.parent_id = rs.rule_id AND sel.type = 'selectors'
        ),

        -- Extract per-rule declarations as MAP(property_name → raw value span).
        -- The value is the text between `:` and `;` in the declaration's peek,
        -- with leading/trailing whitespace and the trailing `;` stripped.
        --
        -- Empty declaration blocks (e.g., `.fn {}`) produce an empty MAP{}
        -- via the COALESCE fallback, not NULL.
        rule_declarations AS (
            SELECT rs.rule_id,
                   COALESCE(
                       MAP(
                           list(pn.name)
                               FILTER (WHERE pn.name IS NOT NULL),
                           list(
                               rtrim(
                                   ltrim(
                                       substring(d.peek FROM position(':' IN d.peek) + 1)
                                   ),
                                   '; '
                               )
                           )
                               FILTER (WHERE d.peek IS NOT NULL)
                       ),
                       MAP([]::VARCHAR[], []::VARCHAR[])
                   ) AS declarations
            FROM rule_sets rs
            LEFT JOIN full_css b  ON b.parent_id  = rs.rule_id AND b.type = 'block'
            LEFT JOIN full_css d  ON d.parent_id  = b.node_id  AND d.type = 'declaration'
            LEFT JOIN full_css pn ON pn.parent_id = d.node_id  AND pn.type = 'property_name'
            GROUP BY rs.rule_id
        )

    -- Dispatch ast_select per rule. This is the line blocked on the upstream
    -- DuckDB planner bug: ast_select's `sel` CTE is parameterized from the
    -- `selector` argument, and the outer body has correlated NOT EXISTS
    -- patterns over that parameterized CTE that crash during bind.
    --
    -- LEFT JOIN LATERAL preserves rules that match zero nodes: they emit one
    -- row with NULL read_ast columns, so the caller's N-rules-in-N-rows-out
    -- invariant holds.
    SELECT rsel.rule_index,
           rsel.selector,
           rd.declarations,
           m.*
    FROM rule_selectors rsel
    JOIN rule_declarations rd USING (rule_id)
    LEFT JOIN LATERAL ast_select(source, rsel.selector, language := language) m ON TRUE;


-- =============================================================================
-- ast_select_list: nested form, one row per rule with matches packed as LIST
-- =============================================================================
--
-- Thin GROUP BY wrapper over ast_select_rules. The `list(m)` aggregation
-- collects matched rows as whole-row STRUCTs (verified: DuckDB produces
-- LIST<STRUCT<all read_ast columns>>). The FILTER clause drops the NULL
-- sentinel row produced by the LEFT JOIN LATERAL for zero-match rules, so
-- rules that matched nothing get an empty list instead of a list containing
-- a single null struct.

CREATE OR REPLACE MACRO ast_select_list(source, query, language := NULL) AS TABLE
    SELECT rule_index,
           any_value(selector)     AS selector,
           any_value(declarations) AS declarations,
           list(m) FILTER (WHERE m.node_id IS NOT NULL) AS matches
    FROM ast_select_rules(source, query, language := language) m
    GROUP BY rule_index
    ORDER BY rule_index;
