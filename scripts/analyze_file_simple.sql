-- Simple function analysis query
WITH func_declarators AS (
    SELECT 
        file_path,
        node_id,
        parent_id,
        start_line,
        end_line,
        children_count,
        descendant_count
    FROM nodes
    WHERE type = 'function_declarator'
      AND semantic_type = 112
      AND file_path = ?
),
declarator_info AS (
    SELECT 
        d.file_path,
        d.node_id,
        d.start_line,
        d.end_line,
        d.parent_id,
        d.descendant_count,
        -- Extract name from identifier child
        MAX(CASE 
            WHEN c.type = 'identifier' THEN c.peek
            WHEN c.type = 'qualified_identifier' THEN c.peek
        END) as full_name,
        -- Count parameters from parameter_list children
        SUM(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count
    FROM func_declarators d
    JOIN nodes c ON c.parent_id = d.node_id AND c.file_path = d.file_path
    GROUP BY d.file_path, d.node_id, d.start_line, d.end_line, d.parent_id, d.descendant_count
),
definition_info AS (
    SELECT 
        di.file_path,
        di.full_name,
        di.start_line,
        di.end_line,
        di.param_count,
        di.descendant_count,
        p.type as parent_type,
        p.start_line as def_start_line,
        p.end_line as def_end_line,
        p.descendant_count as def_complexity,
        substring(p.peek, 1, 80) as function_signature
    FROM declarator_info di
    LEFT JOIN nodes p ON p.node_id = di.parent_id AND p.file_path = di.file_path
)
SELECT 
    full_name as function_name,
    -- Extract class name from qualified names
    CASE 
        WHEN full_name LIKE '%::%' THEN 
            REGEXP_EXTRACT(full_name, '^(.+)::[^:]+$', 1)
        ELSE NULL
    END as class_name,
    -- Extract simple function name
    CASE 
        WHEN full_name LIKE '%::%' THEN 
            REGEXP_EXTRACT(full_name, '::([^:]+)$', 1)
        ELSE full_name
    END as simple_name,
    -- Determine if it's a method or function
    CASE 
        WHEN full_name LIKE '%::%' THEN 'method'
        ELSE 'function'
    END as function_type,
    -- Use definition boundaries if available, else declarator
    COALESCE(def_start_line, start_line) as start_line,
    COALESCE(def_end_line, end_line) as end_line,
    COALESCE(def_end_line, end_line) - COALESCE(def_start_line, start_line) + 1 as line_count,
    param_count as parameter_count,
    -- Use definition complexity if available
    COALESCE(def_complexity, descendant_count) as complexity,
    parent_type,
    function_signature
FROM definition_info
WHERE full_name IS NOT NULL
ORDER BY start_line;