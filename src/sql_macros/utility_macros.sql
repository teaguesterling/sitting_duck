-- Utility Macros for Common Operations
-- These combine multiple operations for convenience

-- Normalize integer value to integer array
CREATE OR REPLACE MACRO ast_normalize_to_int_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::INTEGER[]
        WHEN typeof(val) = 'INTEGER[]' THEN val
        WHEN typeof(val) = 'BIGINT[]' THEN val::INTEGER[]
        WHEN typeof(val) = 'INTEGER' THEN [val]::INTEGER[]
        WHEN typeof(val) = 'BIGINT' THEN [val::INTEGER]::INTEGER[]
        ELSE []::INTEGER[]
    END
);

-- Normalize varchar value to varchar array  
CREATE OR REPLACE MACRO ast_normalize_to_varchar_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::VARCHAR[]
        WHEN typeof(val) = 'VARCHAR[]' THEN val
        WHEN typeof(val) = 'VARCHAR' THEN [val]::VARCHAR[]
        ELSE []::VARCHAR[]
    END
);

-- Find functions and methods together
CREATE OR REPLACE MACRO ast_all_functions(nodes) AS (
    ast_find_type(nodes, ['function_definition', 'method_definition'])
);

-- Find all definition types (functions, classes, methods)
CREATE OR REPLACE MACRO ast_all_definitions(nodes) AS (
    ast_find_type(nodes, ['function_definition', 'class_definition', 'method_definition'])
);

-- Find all literal values
CREATE OR REPLACE MACRO ast_all_literals(nodes) AS (
    ast_find_type(nodes, ['string', 'integer', 'float', 'boolean', 'null'])
);

-- Find all control flow statements
CREATE OR REPLACE MACRO ast_control_flow(nodes) AS (
    ast_find_type(nodes, ['if_statement', 'while_statement', 'for_statement', 'try_statement', 'with_statement'])
);

-- Get nodes at top level (depths 0-2)
CREATE OR REPLACE MACRO ast_top_level(nodes) AS (
    ast_at_depth(nodes, [0, 1, 2])
);

-- Get nodes at mid level (depths 3-5)
CREATE OR REPLACE MACRO ast_mid_level(nodes) AS (
    ast_at_depth(nodes, [3, 4, 5])
);

-- Get deeply nested nodes (depth > 5)
CREATE OR REPLACE MACRO ast_deep_nodes(nodes) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE json_extract(je.value, '$.depth')::INTEGER > 5)
);