-- AST Entrypoint Macro
-- Provides a single starting point for all AST operations with type normalization

-- Main entrypoint that normalizes different input types to a standard nodes array
CREATE OR REPLACE MACRO ast(input) AS (
    CASE 
        -- Input is NULL
        WHEN input IS NULL THEN 
            '[]'::JSON
            
        -- Input is already a JSON array (raw nodes)
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'ARRAY' THEN 
            input
            
        -- Input is a JSON object (single node), wrap in array
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'OBJECT' THEN 
            json_array(input)
            
        -- Input is a struct that might have a 'nodes' field
        WHEN typeof(input) = 'STRUCT' THEN
            COALESCE(
                TRY_CAST(json_extract(input::JSON, '$.nodes') AS JSON),
                '[]'::JSON
            )
            
        -- Input is a VARCHAR, try to parse as JSON
        WHEN typeof(input) = 'VARCHAR' THEN 
            COALESCE(TRY_CAST(input AS JSON), '[]'::JSON)
            
        -- Fallback: empty array
        ELSE '[]'::JSON
    END
);

-- Method-style macros that work with the ast() entrypoint
-- These provide the chainable interface

CREATE OR REPLACE MACRO find_type(nodes, types) AS (
    ast_find_type(ast(nodes), types)
);

CREATE OR REPLACE MACRO function_names(nodes) AS (
    ast_function_names(ast(nodes))
);

CREATE OR REPLACE MACRO class_names(nodes) AS (
    ast_class_names(ast(nodes))
);

CREATE OR REPLACE MACRO identifiers(nodes) AS (
    ast_identifiers(ast(nodes))
);

CREATE OR REPLACE MACRO at_depth(nodes, depths) AS (
    ast_at_depth(ast(nodes), depths)
);

CREATE OR REPLACE MACRO children_of(nodes, parent_id) AS (
    ast_children_of(ast(nodes), parent_id)
);

CREATE OR REPLACE MACRO summary(nodes) AS (
    ast_summary(ast(nodes))
);

CREATE OR REPLACE MACRO function_details(nodes) AS (
    ast_function_details(ast(nodes))
);

CREATE OR REPLACE MACRO type_counts(nodes) AS (
    ast_type_counts(ast(nodes))
);

CREATE OR REPLACE MACRO strings(nodes) AS (
    ast_strings(ast(nodes))
);

-- Utility methods for working with arrays/results
CREATE OR REPLACE MACRO count_elements(arr) AS (
    json_array_length(COALESCE(arr, '[]'::JSON))
);

CREATE OR REPLACE MACRO first_element(arr) AS (
    json_extract(COALESCE(arr, '[]'::JSON), '$[0]')
);

CREATE OR REPLACE MACRO last_element(arr) AS (
    json_extract(
        COALESCE(arr, '[]'::JSON), 
        '$[' || (json_array_length(COALESCE(arr, '[]'::JSON)) - 1) || ']'
    )
);

CREATE OR REPLACE MACRO extract_names(nodes) AS (
    COALESCE(
        (SELECT json_group_array(json_extract_string(je.value, '$.name'))
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.name') IS NOT NULL),
        '[]'::JSON
    )
);

-- Advanced chaining helpers
CREATE OR REPLACE MACRO where_type(nodes, type_name) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.type') = type_name),
        '[]'::JSON
    )
);

CREATE OR REPLACE MACRO where_depth(nodes, target_depth) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract(je.value, '$.depth')::INTEGER = target_depth),
        '[]'::JSON
    )
);