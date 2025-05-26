-- Chain Methods for ast() entrypoint
-- These are activated by calling duckdb_ast_register_short_names()

-- ===================================
-- CHAIN METHODS (UNPREFIXED)
-- ===================================
-- These methods are designed to work with the ast() entrypoint
-- They take the normalized JSON array as their first parameter

-- Core extraction methods
CREATE OR REPLACE MACRO get_type(nodes, types) AS (
    ast_get_type(nodes, types)
);

CREATE OR REPLACE MACRO get_names(nodes, node_type := NULL) AS (
    ast_get_names(nodes, node_type := node_type)
);

CREATE OR REPLACE MACRO get_depth(nodes, depths) AS (
    ast_get_depth(nodes, depths)
);

-- Filtering methods
CREATE OR REPLACE MACRO filter_pattern(nodes, pattern, field := 'name') AS (
    ast_filter_pattern(nodes, pattern, field := field)
);

CREATE OR REPLACE MACRO filter_has_name(nodes) AS (
    ast_filter_has_name(nodes)
);

-- Navigation methods
CREATE OR REPLACE MACRO nav_children(nodes, parent_id) AS (
    ast_nav_children(nodes, parent_id)
);

CREATE OR REPLACE MACRO nav_parent(nodes, child_id) AS (
    ast_nav_parent(nodes, child_id)
);

-- Analysis methods
CREATE OR REPLACE MACRO summary(nodes) AS (
    ast_summary(nodes)
);

-- Chain-specific utility methods
CREATE OR REPLACE MACRO count_nodes(nodes) AS (
    json_array_length(nodes)
);

CREATE OR REPLACE MACRO first_node(nodes) AS (
    json_extract(nodes, '$[0]')
);

CREATE OR REPLACE MACRO last_node(nodes) AS (
    json_extract(nodes, '$[-1]')
);

-- Alternative names for better ergonomics
CREATE OR REPLACE MACRO find_type(nodes, types) AS (
    ast_get_type(nodes, types)
);

CREATE OR REPLACE MACRO find_depth(nodes, depths) AS (
    ast_get_depth(nodes, depths)
);

CREATE OR REPLACE MACRO extract_names(nodes, node_type := NULL) AS (
    ast_get_names(nodes, node_type := node_type)
);

CREATE OR REPLACE MACRO children(nodes, parent_id) AS (
    ast_nav_children(nodes, parent_id)
);

CREATE OR REPLACE MACRO parent(nodes, child_id) AS (
    ast_nav_parent(nodes, child_id)
);

CREATE OR REPLACE MACRO len(nodes) AS (
    json_array_length(nodes)
);

CREATE OR REPLACE MACRO size(nodes) AS (
    json_array_length(nodes)
);

-- ===================================
-- SHORT NAME ALIASES (WITHOUT ast_ PREFIX)
-- ===================================
-- These are standalone functions that can be used without ast()

CREATE OR REPLACE MACRO get_type_standalone(nodes, types) AS (
    ast_get_type(nodes, types)
);

CREATE OR REPLACE MACRO get_names_standalone(nodes, node_type := NULL) AS (
    ast_get_names(nodes, node_type := node_type)
);

CREATE OR REPLACE MACRO get_depth_standalone(nodes, depths) AS (
    ast_get_depth(nodes, depths)
);

CREATE OR REPLACE MACRO filter_pattern_standalone(nodes, pattern, field := 'name') AS (
    ast_filter_pattern(nodes, pattern, field := field)
);

CREATE OR REPLACE MACRO filter_has_name_standalone(nodes) AS (
    ast_filter_has_name(nodes)
);

CREATE OR REPLACE MACRO nav_children_standalone(nodes, parent_id) AS (
    ast_nav_children(nodes, parent_id)
);

CREATE OR REPLACE MACRO nav_parent_standalone(nodes, child_id) AS (
    ast_nav_parent(nodes, child_id)
);

CREATE OR REPLACE MACRO summary_standalone(nodes) AS (
    ast_summary(nodes)
);