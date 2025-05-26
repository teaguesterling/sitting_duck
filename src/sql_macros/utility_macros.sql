-- Utility Macros for Common Operations
-- These combine multiple operations for convenience

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