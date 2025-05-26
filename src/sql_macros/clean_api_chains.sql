-- Clean API Chain Methods
-- These work with the ast() entrypoint for method chaining

-- ===================================
-- CHAIN METHODS FOR ast() ENTRYPOINT
-- ===================================

-- Chain-friendly versions of core functions (use clean API internally)
CREATE OR REPLACE MACRO get_type(nodes, types) AS (
    ast_get_type(nodes, types)
);

CREATE OR REPLACE MACRO get_names(nodes, node_type := NULL) AS (
    ast_get_names(nodes, node_type)
);

CREATE OR REPLACE MACRO get_depth(nodes, depths) AS (
    ast_get_depth(nodes, depths)
);

CREATE OR REPLACE MACRO get_line(nodes, lines) AS (
    ast_get_line(nodes, lines)
);

CREATE OR REPLACE MACRO get_range(nodes, start_line, end_line) AS (
    ast_get_range(nodes, start_line, end_line)
);

CREATE OR REPLACE MACRO filter_pattern(nodes, pattern, field := 'name') AS (
    ast_filter_pattern(nodes, pattern, field)
);

CREATE OR REPLACE MACRO filter_depth_range(nodes, min_depth := 0, max_depth := NULL) AS (
    ast_filter_depth_range(nodes, min_depth, max_depth)
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

CREATE OR REPLACE MACRO nav_siblings(nodes, node_id, include_self := false) AS (
    ast_nav_siblings(nodes, node_id, include_self)
);

-- Analysis methods
CREATE OR REPLACE MACRO analyze_summary(nodes) AS (
    ast_analyze_summary(nodes)
);

CREATE OR REPLACE MACRO analyze_complexity(nodes) AS (
    ast_analyze_complexity(nodes)
);

CREATE OR REPLACE MACRO analyze_types(nodes) AS (
    ast_analyze_types(nodes)
);

-- ===================================
-- TERMINATING CHAIN METHODS
-- ===================================

-- Count elements (terminates chain, returns INTEGER)
CREATE OR REPLACE MACRO count(arr) AS (
    json_array_length(COALESCE(arr, '[]'::JSON))
);

-- Get first element (terminates chain, returns JSON)
CREATE OR REPLACE MACRO first(arr) AS (
    json_extract(COALESCE(arr, '[]'::JSON), '$[0]')
);

-- Get last element (terminates chain, returns JSON)
CREATE OR REPLACE MACRO last(arr) AS (
    json_extract(
        COALESCE(arr, '[]'::JSON), 
        '$[' || (json_array_length(COALESCE(arr, '[]'::JSON)) - 1) || ']'
    )
);

-- Extract names as VARCHAR array (terminates chain)
CREATE OR REPLACE MACRO extract_names(nodes) AS (
    COALESCE(
        (SELECT array_agg(json_extract_string(je.value, '$.name'))
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.name') IS NOT NULL),
        []::VARCHAR[]
    )
);

-- Extract types as VARCHAR array (terminates chain)
CREATE OR REPLACE MACRO extract_types(nodes) AS (
    COALESCE(
        (SELECT array_agg(DISTINCT json_extract_string(je.value, '$.type'))
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.type') IS NOT NULL),
        []::VARCHAR[]
    )
);

-- Extract IDs as INTEGER array (terminates chain)
CREATE OR REPLACE MACRO extract_ids(nodes) AS (
    COALESCE(
        (SELECT array_agg(json_extract(je.value, '$.id')::INTEGER)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract(je.value, '$.id') IS NOT NULL),
        []::INTEGER[]
    )
);

-- ===================================
-- UTILITY CHAIN METHODS
-- ===================================

-- Check if result is empty (returns BOOLEAN)
CREATE OR REPLACE MACRO is_empty(arr) AS (
    json_array_length(COALESCE(arr, '[]'::JSON)) = 0
);

-- Check if result has any elements (returns BOOLEAN)  
CREATE OR REPLACE MACRO has_any(arr) AS (
    json_array_length(COALESCE(arr, '[]'::JSON)) > 0
);

-- Get specific element by index (returns JSON)
CREATE OR REPLACE MACRO at_index(arr, idx) AS (
    json_extract(COALESCE(arr, '[]'::JSON), '$[' || idx || ']')
);

-- ===================================
-- BACKWARD COMPATIBILITY ALIASES
-- ===================================

-- Keep some common old names as aliases during transition
CREATE OR REPLACE MACRO find_type(nodes, types) AS (
    ast_get_type(nodes, types)
);

CREATE OR REPLACE MACRO function_names(nodes) AS (
    ast_get_names(nodes, 'function_definition')
);

CREATE OR REPLACE MACRO class_names(nodes) AS (
    ast_get_names(nodes, 'class_definition')
);

CREATE OR REPLACE MACRO at_depth(nodes, depths) AS (
    ast_get_depth(nodes, depths)
);

CREATE OR REPLACE MACRO children_of(nodes, parent_id) AS (
    ast_nav_children(nodes, parent_id)
);

CREATE OR REPLACE MACRO summary(nodes) AS (
    ast_analyze_summary(nodes)
);

-- Chain count method (backward compatibility)
CREATE OR REPLACE MACRO count_elements(arr) AS (
    json_array_length(COALESCE(arr, '[]'::JSON))
);