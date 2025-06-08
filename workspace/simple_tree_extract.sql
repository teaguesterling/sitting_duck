-- Simplified tree-based extraction
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Step 1: Find function declarators and their immediate children
WITH func_declarators AS (
    SELECT 
        node_id,
        start_line,
        end_line,
        descendant_count
    FROM read_ast('src/unified_ast_backend.cpp')
    WHERE type = 'function_declarator'
      AND semantic_type = 112
),
declarator_children AS (
    SELECT 
        d.node_id as parent_id,
        d.start_line,
        c.type as child_type,
        c.name as child_name,
        c.sibling_index
    FROM func_declarators d
    JOIN read_ast('src/unified_ast_backend.cpp') c 
        ON c.parent_id = d.node_id
)
SELECT 
    parent_id,
    start_line,
    MAX(CASE WHEN child_type IN ('qualified_identifier', 'identifier') THEN child_name END) as function_full_name,
    COUNT(CASE WHEN child_type = 'parameter_list' THEN 1 END) as has_params
FROM declarator_children
GROUP BY parent_id, start_line
ORDER BY start_line;