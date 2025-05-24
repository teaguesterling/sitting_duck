-- SQL Macros for AST querying with proper JSON type handling
-- These handle JSON type casting internally for clean user experience

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
     WHERE json_extract_string(je.value, '$.type') = 'function_definition'
       AND json_extract_string(je.value, '$.name') IS NOT NULL)
);

-- Get all class names
CREATE OR REPLACE MACRO ast_class_names(nodes) AS (
    (SELECT json_group_array(json_extract_string(je.value, '$.name'))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'class_definition'
       AND json_extract_string(je.value, '$.name') IS NOT NULL)
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

-- Get all identifiers (handles proper field name)
CREATE OR REPLACE MACRO ast_identifiers(nodes) AS (
    (SELECT json_group_array(json_extract_string(je.value, '$.name'))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'identifier'
       AND json_extract_string(je.value, '$.name') IS NOT NULL)
);

-- Get all string literals
CREATE OR REPLACE MACRO ast_strings(nodes) AS (
    (SELECT json_group_array(json_extract_string(je.value, '$.content'))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'string'
       AND json_extract_string(je.value, '$.content') IS NOT NULL)
);

-- Get function details with proper type casting for line numbers
CREATE OR REPLACE MACRO ast_function_details(nodes) AS (
    (SELECT json_group_array(
        json_object(
            'name', json_extract_string(je.value, '$.name'),
            'start_line', json_extract(je.value, '$.start.line')::INTEGER,
            'end_line', json_extract(je.value, '$.end.line')::INTEGER,
            'depth', json_extract(je.value, '$.depth')::INTEGER,
            'id', json_extract(je.value, '$.id')::INTEGER
        ))
     FROM json_each(nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'function_definition'
       AND json_extract_string(je.value, '$.name') IS NOT NULL)
);

-- Get a summary of the AST structure (handles all type casting internally)
CREATE OR REPLACE MACRO ast_summary(nodes) AS (
    json_object(
        'total_nodes', json_array_length(nodes),
        'node_types', ast_type_counts(nodes),
        'functions', ast_function_names(nodes),
        'classes', ast_class_names(nodes),
        'max_depth', (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
                      FROM json_each(nodes) AS je),
        'function_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                          WHERE json_extract_string(je.value, '$.type') = 'function_definition'),
        'class_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                       WHERE json_extract_string(je.value, '$.type') = 'class_definition')
    )
);

-- Handle empty results gracefully - return empty array instead of NULL
CREATE OR REPLACE MACRO ast_safe_find_type(nodes, node_type) AS (
    COALESCE(ast_find_type(nodes, node_type), '[]'::JSON)
);

-- Handle empty function lists gracefully
CREATE OR REPLACE MACRO ast_safe_function_names(nodes) AS (
    COALESCE(ast_function_names(nodes), '[]'::JSON)
);

-- Find nodes by line number (with proper integer comparison)
CREATE OR REPLACE MACRO ast_at_line(nodes, line_number) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE json_extract(je.value, '$.start.line')::INTEGER <= line_number
       AND json_extract(je.value, '$.end.line')::INTEGER >= line_number)
);

-- Get nodes in line number range
CREATE OR REPLACE MACRO ast_in_line_range(nodes, start_line, end_line) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE json_extract(je.value, '$.start.line')::INTEGER >= start_line
       AND json_extract(je.value, '$.end.line')::INTEGER <= end_line)
);

-- Extract all nodes with their essential info (proper type casting)
CREATE OR REPLACE MACRO ast_node_info(nodes) AS (
    (SELECT json_group_array(
        json_object(
            'id', json_extract(je.value, '$.id')::INTEGER,
            'type', json_extract_string(je.value, '$.type'),
            'name', json_extract_string(je.value, '$.name'),
            'start_line', json_extract(je.value, '$.start.line')::INTEGER,
            'end_line', json_extract(je.value, '$.end.line')::INTEGER,
            'depth', json_extract(je.value, '$.depth')::INTEGER
        ))
     FROM json_each(nodes) AS je)
);

-- Find nodes containing specific text (searches name, content fields)
CREATE OR REPLACE MACRO ast_contains_text(nodes, search_text) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE COALESCE(json_extract_string(je.value, '$.name'), '') LIKE '%' || search_text || '%'
        OR COALESCE(json_extract_string(je.value, '$.content'), '') LIKE '%' || search_text || '%')
);

-- Get complexity metrics (with proper numeric handling)
CREATE OR REPLACE MACRO ast_complexity(nodes) AS (
    json_object(
        'total_nodes', json_array_length(nodes),
        'avg_depth', (SELECT AVG(json_extract(je.value, '$.depth')::INTEGER) 
                      FROM json_each(nodes) AS je),
        'max_depth', (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
                      FROM json_each(nodes) AS je),
        'function_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                          WHERE json_extract_string(je.value, '$.type') = 'function_definition'),
        'lines_of_code', (SELECT MAX(json_extract(je.value, '$.end.line')::INTEGER) 
                         FROM json_each(nodes) AS je)
    )
);