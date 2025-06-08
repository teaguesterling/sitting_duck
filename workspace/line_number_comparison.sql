-- Comparison of WRONG vs CORRECTED line number extraction

WITH ast_data AS (
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
),

-- OLD WRONG WAY: Using function_declarator nodes
wrong_extraction AS (
    SELECT 
        CASE 
            WHEN full_name LIKE '%::%' 
            THEN split_part(full_name, '::', 2)
            ELSE full_name
        END as function_name,
        start_line as wrong_start,
        end_line as wrong_end,
        end_line - start_line + 1 as wrong_line_count,
        'function_declarator' as wrong_source
    FROM (
        SELECT 
            fd.start_line,
            fd.end_line,
            MAX(CASE 
                WHEN c.type IN ('identifier', 'qualified_identifier') 
                THEN c.name
            END) as full_name
        FROM (
            SELECT node_id, parent_id, start_line, end_line
            FROM ast_data
            WHERE type = 'function_declarator' AND semantic_type = 112
        ) fd
        JOIN ast_data c ON c.parent_id = fd.node_id
        GROUP BY fd.node_id, fd.start_line, fd.end_line
    ) sub
    WHERE full_name IS NOT NULL
),

-- NEW CORRECT WAY: Using function_definition nodes
correct_extraction AS (
    SELECT 
        CASE 
            WHEN full_name LIKE '%::%' 
            THEN split_part(full_name, '::', 2)
            ELSE full_name
        END as function_name,
        start_line as correct_start,
        end_line as correct_end,
        end_line - start_line + 1 as correct_line_count,
        'function_definition' as correct_source
    FROM (
        SELECT 
            fd.start_line,
            fd.end_line,
            MAX(CASE 
                WHEN decl.sibling_index = 1 AND child.type IN ('identifier', 'qualified_identifier')
                THEN child.name
            END) as full_name
        FROM (
            SELECT node_id, start_line, end_line
            FROM ast_data
            WHERE type = 'function_definition' AND semantic_type = 112
        ) fd
        JOIN ast_data decl ON decl.parent_id = fd.node_id AND decl.sibling_index = 1 
        LEFT JOIN ast_data child ON child.parent_id = decl.node_id
        GROUP BY fd.node_id, fd.start_line, fd.end_line
    ) sub
    WHERE full_name IS NOT NULL
)

-- Compare side by side
SELECT 
    w.function_name,
    w.wrong_start,
    c.correct_start,
    c.correct_start - w.wrong_start as start_diff,
    w.wrong_end,
    c.correct_end,
    c.correct_end - w.wrong_end as end_diff,
    w.wrong_line_count,
    c.correct_line_count,
    c.correct_line_count - w.wrong_line_count as size_improvement,
    CASE 
        WHEN c.correct_line_count > w.wrong_line_count * 3 
        THEN '✅ MAJOR FIX'
        WHEN c.correct_line_count > w.wrong_line_count 
        THEN '✅ IMPROVEMENT'
        ELSE '⚠️  CHECK'
    END as status
FROM wrong_extraction w
JOIN correct_extraction c ON w.function_name = c.function_name
ORDER BY w.wrong_start
LIMIT 5;