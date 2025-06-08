-- Test JSON aggregate functions with AST data
LOAD json;
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- First, let's understand our data structure
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
)
SELECT 
    node_count,
    json_extract_string(nodes, '$[0].type') as root_type,
    json_extract_string(nodes, '$[1].type') as second_type
FROM ast_data;

-- Example 1: Group all function names into a JSON array
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
),
function_nodes AS (
    SELECT 
        json_extract_string(node, '$.content') as function_name,
        json_extract_string(node, '$.type') as node_type
    FROM (
        SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node
        FROM ast_data
    )
    WHERE json_extract_string(node, '$.type') = 'function_definition'
)
SELECT json_group_array(function_name) as all_functions
FROM function_nodes;

-- Example 2: Create a JSON object mapping node types to their counts
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
),
node_types AS (
    SELECT 
        json_extract_string(node, '$.type') as node_type
    FROM (
        SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node
        FROM ast_data
    )
)
SELECT json_group_object(node_type, cnt) as type_counts
FROM (
    SELECT node_type, COUNT(*) as cnt
    FROM node_types
    GROUP BY node_type
);

-- Example 3: Extract all identifiers and group by their context
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
),
identifiers AS (
    SELECT 
        json_extract_string(node, '$.content') as identifier_name,
        json_extract(node, '$.start_row') as line_number
    FROM (
        SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node
        FROM ast_data
    )
    WHERE json_extract_string(node, '$.type') = 'identifier'
        AND json_extract_string(node, '$.content') IS NOT NULL
)
SELECT json_group_array(DISTINCT identifier_name) as unique_identifiers
FROM identifiers;

-- Example 4: Reconstruct a simplified AST with only function definitions
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
),
functions AS (
    SELECT 
        json_extract(node, '$') as full_node
    FROM (
        SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node
        FROM ast_data
    )
    WHERE json_extract_string(node, '$.type') = 'function_definition'
)
SELECT json_group_array(full_node) as function_ast
FROM functions;

-- Example 5: Create a structured summary of the code
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
),
code_elements AS (
    SELECT 
        json_extract_string(node, '$.type') as element_type,
        json_extract_string(node, '$.content') as element_content,
        json_extract(node, '$.start_row') as line_number
    FROM (
        SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node
        FROM ast_data
    )
    WHERE json_extract_string(node, '$.type') IN ('function_definition', 'string', 'identifier')
        AND json_extract_string(node, '$.content') IS NOT NULL
)
SELECT 
    json_group_object(
        element_type,
        json_group_array(element_content)
    ) as code_summary
FROM code_elements
GROUP BY element_type;

-- Example 6: Build a custom AST view with parent-child relationships
-- This would be more complex but shows the potential
WITH ast_data AS (
    SELECT * FROM read_ast_objects('test/data/python/simple.py', 'python')
),
nodes_with_index AS (
    SELECT 
        row_number() OVER () - 1 as node_index,
        node,
        json_extract_string(node, '$.type') as node_type,
        json_extract(node, '$.children') as children
    FROM (
        SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node
        FROM ast_data
    )
)
SELECT 
    json_group_array(
        json_object(
            'index', node_index,
            'type', node_type,
            'has_children', children IS NOT NULL
        )
    ) as simplified_ast
FROM nodes_with_index
WHERE node_type IN ('module', 'function_definition', 'class_definition');