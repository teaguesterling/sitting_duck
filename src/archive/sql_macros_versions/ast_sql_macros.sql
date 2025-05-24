-- SQL Macros for AST querying with natural syntax
-- These can be used with dot notation on JSON columns

-- Find all nodes of a specific type
CREATE OR REPLACE MACRO ast_find_type(nodes, node_type) AS (
    (SELECT json_group_array(node) 
     FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
     WHERE json_extract_string(node, '$.type') = node_type)
);

-- Get all function names
CREATE OR REPLACE MACRO ast_function_names(nodes) AS (
    (SELECT json_group_array(json_extract_string(node, '$.name'))
     FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
     WHERE json_extract_string(node, '$.type') = 'function_definition')
);

-- Get all class names
CREATE OR REPLACE MACRO ast_class_names(nodes) AS (
    (SELECT json_group_array(json_extract_string(node, '$.name'))
     FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
     WHERE json_extract_string(node, '$.type') = 'class_definition')
);

-- Count nodes by type
CREATE OR REPLACE MACRO ast_type_counts(nodes) AS (
    (SELECT json_group_object(node_type, cnt)
     FROM (
         SELECT json_extract_string(node, '$.type') as node_type, COUNT(*) as cnt
         FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
         GROUP BY node_type
     ))
);

-- Get nodes at specific depth
CREATE OR REPLACE MACRO ast_at_depth(nodes, depth) AS (
    (SELECT json_group_array(node)
     FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
     WHERE json_extract(node, '$.depth')::INTEGER = depth)
);

-- Get direct children of a node
CREATE OR REPLACE MACRO ast_children_of(nodes, parent_id) AS (
    (SELECT json_group_array(node)
     FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
     WHERE json_extract(node, '$.parent_id')::INTEGER = parent_id)
);

-- Find nodes containing specific text
CREATE OR REPLACE MACRO ast_contains_text(nodes, search_text) AS (
    (SELECT json_group_array(node)
     FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
     WHERE COALESCE(json_extract_string(node, '$.name'), '') LIKE '%' || search_text || '%'
        OR COALESCE(json_extract_string(node, '$.value'), '') LIKE '%' || search_text || '%')
);

-- Get a summary of the AST structure
CREATE OR REPLACE MACRO ast_summary(nodes) AS (
    json_object(
        'total_nodes', json_array_length(nodes),
        'node_types', ast_type_counts(nodes),
        'functions', ast_function_names(nodes),
        'classes', ast_class_names(nodes),
        'max_depth', (SELECT MAX(json_extract(node, '$.depth')::INTEGER) 
                      FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node))
    )
);

-- Extract imports (Python-specific but can be adapted)
CREATE OR REPLACE MACRO ast_imports(nodes) AS (
    (SELECT json_group_array(
         json_object(
             'module', json_extract_string(node, '$.module'),
             'names', json_extract(node, '$.names')
         ))
     FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
     WHERE json_extract_string(node, '$.type') IN ('import_statement', 'import_from'))
);

-- Get line numbers for specific node types
CREATE OR REPLACE MACRO ast_lines_of_type(nodes, node_type) AS (
    (SELECT json_group_array(json_extract(node, '$.start.line')::INTEGER)
     FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node)
     WHERE json_extract_string(node, '$.type') = node_type)
);