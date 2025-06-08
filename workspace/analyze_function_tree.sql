-- Analyze function tree structure to understand child patterns
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- First, create a helper to get immediate children of a node
CREATE OR REPLACE MACRO get_children(file_path, parent_node_id) AS TABLE
    SELECT * FROM read_ast(file_path) 
    WHERE parent_id = parent_node_id
    ORDER BY sibling_index;

-- Analyze function_declarator structure
CREATE OR REPLACE MACRO analyze_function_declarator(file_path) AS TABLE
    WITH declarators AS (
        SELECT 
            node_id,
            start_line,
            children_count
        FROM read_ast(file_path)
        WHERE type = 'function_declarator'
          AND semantic_type = 112
    ),
    declarator_children AS (
        SELECT 
            d.node_id,
            d.start_line,
            c.sibling_index,
            c.type as child_type,
            c.name as child_name,
            c.semantic_type as child_semantic_type
        FROM declarators d
        JOIN read_ast(file_path) c ON c.parent_id = d.node_id
    )
    SELECT 
        node_id,
        start_line,
        STRING_AGG(
            'idx=' || sibling_index || 
            ':type=' || child_type || 
            ':name=' || COALESCE(child_name, 'NULL'),
            ' | ' ORDER BY sibling_index
        ) as children_pattern
    FROM declarator_children
    GROUP BY node_id, start_line
    ORDER BY start_line;

-- Run analysis
FROM analyze_function_declarator('src/unified_ast_backend.cpp');