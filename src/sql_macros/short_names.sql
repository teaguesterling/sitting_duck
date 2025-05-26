-- Short name aliases for AST functions
-- These are registered when users call duckdb_ast_register_short_names()

-- Macro to register all short names
CREATE OR REPLACE MACRO duckdb_ast_register_short_names() AS (
    -- Core extraction functions
    'CREATE OR REPLACE MACRO get_type(nodes, types) AS ast_get_type(nodes, types);' ||
    'CREATE OR REPLACE MACRO get_names(nodes, node_type) AS ast_get_names(nodes, node_type);' ||
    'CREATE OR REPLACE MACRO get_depth(nodes, depths) AS ast_get_depth(nodes, depths);' ||
    'CREATE OR REPLACE MACRO get_line(nodes, lines) AS ast_get_line(nodes, lines);' ||
    'CREATE OR REPLACE MACRO get_range(nodes, start_line, end_line) AS ast_get_range(nodes, start_line, end_line);' ||
    
    -- Filtering functions
    'CREATE OR REPLACE MACRO filter_pattern(nodes, pattern, field) AS ast_filter_pattern(nodes, pattern, field);' ||
    'CREATE OR REPLACE MACRO filter_depth_range(nodes, min_depth, max_depth) AS ast_filter_depth_range(nodes, min_depth, max_depth);' ||
    'CREATE OR REPLACE MACRO filter_has_name(nodes) AS ast_filter_has_name(nodes);' ||
    
    -- Navigation functions
    'CREATE OR REPLACE MACRO nav_children(nodes, parent_id) AS ast_nav_children(nodes, parent_id);' ||
    'CREATE OR REPLACE MACRO nav_parent(nodes, child_id) AS ast_nav_parent(nodes, child_id);' ||
    'CREATE OR REPLACE MACRO nav_siblings(nodes, node_id) AS ast_nav_siblings(nodes, node_id);' ||
    'CREATE OR REPLACE MACRO nav_ancestors(nodes, node_id, levels) AS ast_nav_ancestors(nodes, node_id, levels);' ||
    'CREATE OR REPLACE MACRO nav_descendants(nodes, node_id, levels) AS ast_nav_descendants(nodes, node_id, levels);' ||
    
    -- Analysis functions
    'CREATE OR REPLACE MACRO analyze_summary(nodes) AS ast_analyze_summary(nodes);' ||
    'CREATE OR REPLACE MACRO analyze_complexity(nodes) AS ast_analyze_complexity(nodes);' ||
    'CREATE OR REPLACE MACRO analyze_types(nodes) AS ast_analyze_types(nodes);' ||
    
    -- Legacy compatibility (most common)
    'CREATE OR REPLACE MACRO find_type(nodes, types) AS ast_find_type(nodes, types);' ||
    'CREATE OR REPLACE MACRO function_names(nodes) AS ast_function_names(nodes);' ||
    'CREATE OR REPLACE MACRO class_names(nodes) AS ast_class_names(nodes);' ||
    'CREATE OR REPLACE MACRO at_depth(nodes, depth) AS ast_at_depth(nodes, depth);' ||
    'CREATE OR REPLACE MACRO children_of(nodes, parent_id) AS ast_children_of(nodes, parent_id);' ||
    
    'Short names registered successfully!'
);

-- Macro to unregister short names (cleanup)
CREATE OR REPLACE MACRO duckdb_ast_unregister_short_names() AS (
    -- Core extraction functions
    'DROP MACRO IF EXISTS get_type;' ||
    'DROP MACRO IF EXISTS get_names;' ||
    'DROP MACRO IF EXISTS get_depth;' ||
    'DROP MACRO IF EXISTS get_line;' ||
    'DROP MACRO IF EXISTS get_range;' ||
    
    -- Filtering functions
    'DROP MACRO IF EXISTS filter_pattern;' ||
    'DROP MACRO IF EXISTS filter_depth_range;' ||
    'DROP MACRO IF EXISTS filter_has_name;' ||
    
    -- Navigation functions
    'DROP MACRO IF EXISTS nav_children;' ||
    'DROP MACRO IF EXISTS nav_parent;' ||
    'DROP MACRO IF EXISTS nav_siblings;' ||
    'DROP MACRO IF EXISTS nav_ancestors;' ||
    'DROP MACRO IF EXISTS nav_descendants;' ||
    
    -- Analysis functions
    'DROP MACRO IF EXISTS analyze_summary;' ||
    'DROP MACRO IF EXISTS analyze_complexity;' ||
    'DROP MACRO IF EXISTS analyze_types;' ||
    
    -- Legacy compatibility
    'DROP MACRO IF EXISTS find_type;' ||
    'DROP MACRO IF EXISTS function_names;' ||
    'DROP MACRO IF EXISTS class_names;' ||
    'DROP MACRO IF EXISTS at_depth;' ||
    'DROP MACRO IF EXISTS children_of;' ||
    
    'Short names unregistered successfully!'
);

-- Helper to list available short names
CREATE OR REPLACE MACRO duckdb_ast_list_short_names() AS (
    SELECT * FROM (VALUES
        ('get_type', 'ast_get_type', 'Find nodes by type(s)'),
        ('get_names', 'ast_get_names', 'Extract names, optionally by type'),
        ('get_depth', 'ast_get_depth', 'Find nodes at depth(s)'),
        ('get_line', 'ast_get_line', 'Find nodes at line(s)'),
        ('get_range', 'ast_get_range', 'Find nodes in line range'),
        ('filter_pattern', 'ast_filter_pattern', 'Filter by pattern'),
        ('filter_depth_range', 'ast_filter_depth_range', 'Filter by depth range'),
        ('filter_has_name', 'ast_filter_has_name', 'Filter to nodes with names'),
        ('nav_children', 'ast_nav_children', 'Get direct children'),
        ('nav_parent', 'ast_nav_parent', 'Get parent node'),
        ('nav_siblings', 'ast_nav_siblings', 'Get sibling nodes'),
        ('nav_ancestors', 'ast_nav_ancestors', 'Get ancestors'),
        ('nav_descendants', 'ast_nav_descendants', 'Get descendants'),
        ('analyze_summary', 'ast_analyze_summary', 'Overall statistics'),
        ('analyze_complexity', 'ast_analyze_complexity', 'Complexity metrics'),
        ('analyze_types', 'ast_analyze_types', 'Type counts'),
        ('find_type', 'ast_find_type', 'Legacy: Find nodes by type'),
        ('function_names', 'ast_function_names', 'Legacy: Get function names'),
        ('class_names', 'ast_class_names', 'Legacy: Get class names'),
        ('at_depth', 'ast_at_depth', 'Legacy: Find at depth'),
        ('children_of', 'ast_children_of', 'Legacy: Get children')
    ) AS t(short_name, full_name, description)
);