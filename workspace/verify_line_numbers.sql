-- Verify line numbers against actual source code
-- This compares our extracted line numbers with manual verification

-- Get detailed function information
WITH extracted_functions AS (
    SELECT 
        CASE 
            WHEN full_name LIKE '%::%' 
            THEN split_part(full_name, '::', 2)
            ELSE full_name
        END as function_name,
        start_line,
        end_line,
        end_line - start_line + 1 as calculated_line_count,
        full_name as qualified_name
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
            FROM read_ast('src/unified_ast_backend.cpp')
            WHERE type = 'function_declarator' AND semantic_type = 112
        ) fd
        JOIN read_ast('src/unified_ast_backend.cpp') c ON c.parent_id = fd.node_id
        GROUP BY fd.node_id, fd.start_line, fd.end_line
    )
    WHERE full_name IS NOT NULL
),
-- Manual verification data (from actual source inspection)
actual_functions AS (
    SELECT * FROM (VALUES
        ('ParseToASTResult', 10, 128, 119),     -- Actually starts line 10, ends line 128
        ('PopulateSemanticFields', 139, 153, 15), -- Starts 139, ends 153
        ('GetFlatTableSchema', 165, 177, 13),   -- Starts 165, ends 177  
        ('GetFlatTableColumnNames', 188, 188, 1), -- Single line function header
        ('GetASTStructSchema', 199, 220, 22)    -- Need to verify this one
    ) AS t(function_name, actual_start, actual_end, actual_line_count)
)
-- Compare extracted vs actual
SELECT 
    ef.function_name,
    ef.start_line as extracted_start,
    af.actual_start,
    ef.start_line - af.actual_start as start_diff,
    ef.end_line as extracted_end,
    af.actual_end,
    ef.end_line - af.actual_end as end_diff,
    ef.calculated_line_count as extracted_count,
    af.actual_line_count,
    ef.calculated_line_count - af.actual_line_count as count_diff,
    CASE 
        WHEN ef.start_line = af.actual_start AND ef.end_line = af.actual_end 
        THEN '✅ CORRECT'
        WHEN abs(ef.start_line - af.actual_start) <= 1 AND abs(ef.end_line - af.actual_end) <= 1
        THEN '⚠️  CLOSE'
        ELSE '❌ WRONG'
    END as accuracy
FROM extracted_functions ef
LEFT JOIN actual_functions af ON ef.function_name = af.function_name
ORDER BY ef.start_line;