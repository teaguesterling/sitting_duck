-- Tree-based function extraction using AST structure, not peek
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- First, let's understand the structure of function nodes
WITH function_structure AS (
    SELECT 
        p.node_id as parent_id,
        p.type as parent_type,
        p.name as parent_name,
        p.children_count,
        c.node_id as child_id,
        c.type as child_type,
        c.name as child_name,
        c.sibling_index,
        p.start_line
    FROM read_ast('src/unified_ast_backend.cpp') p
    JOIN read_ast('src/unified_ast_backend.cpp') c
        ON c.parent_id = p.node_id
    WHERE p.type IN ('function_definition', 'function_declarator')
      AND p.semantic_type = 112
    ORDER BY p.start_line, c.sibling_index
)
SELECT * FROM function_structure LIMIT 20;

-- Let's see what children function_declarator nodes have
WITH declarator_children AS (
    SELECT 
        p.node_id,
        p.type,
        p.start_line,
        STRING_AGG(c.type || ':' || COALESCE(c.name, 'null'), ', ' ORDER BY c.sibling_index) as child_info
    FROM read_ast('src/unified_ast_backend.cpp') p
    LEFT JOIN read_ast('src/unified_ast_backend.cpp') c
        ON c.parent_id = p.node_id
    WHERE p.type = 'function_declarator'
    GROUP BY p.node_id, p.type, p.start_line
)
SELECT * FROM declarator_children ORDER BY start_line LIMIT 10;