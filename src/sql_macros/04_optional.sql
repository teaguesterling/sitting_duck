-- Optional Features
-- Additional functionality that can be loaded separately

-- ===================================
-- SHORT NAME REGISTRATION
-- ===================================

-- Register unprefixed aliases for common functions
CREATE OR REPLACE MACRO duckdb_ast_register_short_names() AS (
    -- This would need to be implemented differently since we can't
    -- dynamically create macros from within a macro in DuckDB
    'Short name registration not yet implemented - use ast_* prefixed functions'
);

-- ===================================
-- HELP SYSTEM
-- ===================================

-- Interactive help
CREATE OR REPLACE MACRO ast_help() AS (
    json_object(
        'description', 'DuckDB AST Extension - Minimal API',
        'core_functions', json_array(
            'ast_get_type(nodes, types) - Find nodes by type(s)',
            'ast_get_names(nodes, type?) - Extract names',
            'ast_get_depth(nodes, depths) - Find nodes at depth(s)',
            'ast_filter_pattern(nodes, pattern) - Filter by pattern',
            'ast_filter_has_name(nodes) - Filter to named nodes',
            'ast_nav_children(nodes, id) - Get children',
            'ast_nav_parent(nodes, id) - Get parent',
            'ast_summary(nodes) - Get statistics'
        ),
        'chaining', 'Use ast(nodes).method().method() for chaining',
        'examples', json_array(
            'SELECT ast_get_type(nodes, ''function_definition'') FROM read_ast_objects(''file.py'', ''python'')',
            'SELECT ast(nodes).get_type(''function_definition'').count() FROM read_ast_objects(''file.py'', ''python'')'
        )
    )
);