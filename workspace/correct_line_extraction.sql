-- Correct line number extraction using function_definition nodes
-- The function_definition node has the correct start/end span

WITH function_definitions AS (
    -- Start with function_definition nodes (they have the correct line spans)
    SELECT node_id, start_line, end_line, children_count, descendant_count
    FROM read_ast('src/unified_ast_backend.cpp')
    WHERE type = 'function_definition' AND semantic_type = 112
),
function_names AS (
    -- Get function names from the declarator child (sibling_index = 1)
    SELECT 
        fd.node_id,
        fd.start_line,
        fd.end_line,
        fd.end_line - fd.start_line + 1 as line_count,
        MAX(CASE 
            WHEN decl.sibling_index = 1 AND child.type IN ('identifier', 'qualified_identifier')
            THEN child.name
        END) as full_name,
        MAX(CASE 
            WHEN decl.sibling_index = 1 AND child.type = 'parameter_list'
            THEN child.children_count 
            ELSE 0
        END) as param_count
    FROM function_definitions fd
    -- Join to get function_declarator (sibling_index = 1)
    JOIN read_ast('src/unified_ast_backend.cpp') decl 
      ON decl.parent_id = fd.node_id AND decl.sibling_index = 1 AND decl.type = 'function_declarator'
    -- Join to get children of declarator (identifier and parameter_list)
    LEFT JOIN read_ast('src/unified_ast_backend.cpp') child ON child.parent_id = decl.node_id
    GROUP BY fd.node_id, fd.start_line, fd.end_line
)
SELECT 
    CASE 
        WHEN full_name LIKE '%::%' 
        THEN split_part(full_name, '::', 2)
        ELSE full_name
    END as function_name,
    CASE 
        WHEN full_name LIKE '%::%' 
        THEN split_part(full_name, '::', 1)
        ELSE NULL
    END as class_name,
    start_line,
    end_line,
    line_count,
    param_count,
    full_name as qualified_name,
    'CORRECTED' as source
FROM function_names
WHERE full_name IS NOT NULL
ORDER BY start_line
LIMIT 5;