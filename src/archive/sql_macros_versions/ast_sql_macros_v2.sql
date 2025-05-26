-- SQL Macros for AST querying with json_each/json_tree support
-- These leverage DuckDB 1.3+ JSON table functions for cleaner syntax

-- Find all nodes of a specific type
CREATE OR REPLACE MACRO ast_find_type(nodes, node_type) AS (
    (SELECT json_group_array(je.value) 
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = node_type)
);

-- Get all function names
CREATE OR REPLACE MACRO ast_function_names(nodes) AS (
    (SELECT json_group_array(json_extract_string(je.value, '$.name'))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'function_definition')
);

-- Get all class names
CREATE OR REPLACE MACRO ast_class_names(nodes) AS (
    (SELECT json_group_array(json_extract_string(je.value, '$.name'))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'class_definition')
);

-- Count nodes by type
CREATE OR REPLACE MACRO ast_type_counts(nodes) AS (
    (SELECT json_group_object(node_type, cnt)
     FROM (
         SELECT json_extract_string(je.value, '$.type') as node_type, COUNT(*) as cnt
         FROM json_each(nodes) AS je
         GROUP BY node_type
     ))
);

-- Get nodes at specific depth
CREATE OR REPLACE MACRO ast_at_depth(nodes, target_depth) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE json_extract(je.value, '$.depth')::INTEGER = target_depth)
);

-- Get direct children of a node by ID
CREATE OR REPLACE MACRO ast_children_of(nodes, parent_id) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE json_extract(je.value, '$.parent_id')::INTEGER = parent_id)
);

-- Find nodes containing specific text in name or value
CREATE OR REPLACE MACRO ast_contains_text(nodes, search_text) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE COALESCE(json_extract_string(je.value, '$.name'), '') LIKE '%' || search_text || '%'
        OR COALESCE(json_extract_string(je.value, '$.value'), '') LIKE '%' || search_text || '%'
        OR COALESCE(json_extract_string(je.value, '$.content'), '') LIKE '%' || search_text || '%')
);

-- Get all identifiers
CREATE OR REPLACE MACRO ast_identifiers(nodes) AS (
    (SELECT json_group_array(json_extract_string(je.value, '$.content'))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'identifier'
       AND json_extract_string(je.value, '$.content') IS NOT NULL)
);

-- Get all string literals
CREATE OR REPLACE MACRO ast_strings(nodes) AS (
    (SELECT json_group_array(json_extract_string(je.value, '$.content'))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'string'
       AND json_extract_string(je.value, '$.content') IS NOT NULL)
);

-- Get a summary of the AST structure
CREATE OR REPLACE MACRO ast_summary(nodes) AS (
    json_object(
        'total_nodes', json_array_length(nodes),
        'node_types', ast_type_counts(nodes),
        'functions', ast_function_names(nodes),
        'classes', ast_class_names(nodes),
        'max_depth', (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
                      FROM json_each(nodes) AS je)
    )
);

-- Get complexity metrics for functions
CREATE OR REPLACE MACRO ast_function_complexity(nodes) AS (
    (SELECT json_group_array(
        json_object(
            'name', json_extract_string(je.value, '$.name'),
            'start_line', json_extract(je.value, '$.start.line'),
            'end_line', json_extract(je.value, '$.end.line'),
            'lines', json_extract(je.value, '$.end.line')::INTEGER - 
                     json_extract(je.value, '$.start.line')::INTEGER + 1,
            'children', json_array_length(json_extract(je.value, '$.children'))
        ))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'function_definition')
);

-- Extract all nodes with their paths (useful for tree visualization)
CREATE OR REPLACE MACRO ast_with_paths(nodes) AS (
    (SELECT json_group_array(
        json_object(
            'id', json_extract(je.value, '$.id'),
            'type', json_extract_string(je.value, '$.type'),
            'name', json_extract_string(je.value, '$.name'),
            'parent_id', json_extract(je.value, '$.parent_id'),
            'depth', json_extract(je.value, '$.depth'),
            'index', je.key::INTEGER
        ))
     FROM json_each(nodes) AS je)
);

-- Find all TODOs and FIXMEs in comments and strings
CREATE OR REPLACE MACRO ast_todos(nodes) AS (
    (SELECT json_group_array(
        json_object(
            'type', json_extract_string(je.value, '$.type'),
            'content', json_extract_string(je.value, '$.content'),
            'line', json_extract(je.value, '$.start.line')
        ))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') IN ('comment', 'string')
       AND (json_extract_string(je.value, '$.content') LIKE '%TODO%'
         OR json_extract_string(je.value, '$.content') LIKE '%FIXME%'))
);