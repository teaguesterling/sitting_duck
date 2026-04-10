-- =============================================================================
-- parse_ast_list_table: Table-macro wrapper for parse_ast_list
-- =============================================================================
--
-- parse_ast() is a table function that only accepts literal arguments — it
-- cannot be used with column-valued inputs (e.g., per-row dispatch in lateral
-- joins). parse_ast_list() (PR #66) is a scalar variant that DOES accept
-- column-valued inputs but returns a single LIST(STRUCT) value, not a table.
--
-- This table macro bridges the two: it presents the same row-shaped interface
-- as parse_ast() (so it can be used as a drop-in replacement in FROM clauses)
-- while internally using parse_ast_list() so it accepts column-valued args.
--
-- Used by ast_select to enable column-valued selector dispatch, which in turn
-- is required for ast_select_rules to call ast_select once per parsed CSS rule.

CREATE OR REPLACE MACRO parse_ast_list_table(code, language) AS TABLE
    SELECT
        n.node_id,
        n.type,
        n.name,
        n.qualified_name,
        n.start_line,
        n.end_line,
        n.parent_id,
        n.depth,
        n.sibling_index,
        n.children_count,
        n.descendant_count,
        n.scope_id,
        n.scope_stack,
        n.peek,
        n.semantic_type,
        n.flags,
        n.signature_type,
        n.parameters,
        n.modifiers,
        n.annotations,
        n.file_path,
        n.language
    FROM (SELECT unnest(parse_ast_list(code, language)) AS n);
