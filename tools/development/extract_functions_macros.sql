-- Semantic Function Extraction Macros
-- These macros provide semantic extraction capabilities on existing AST data

-- Extract all functions from AST nodes using tree traversal
CREATE OR REPLACE MACRO extract_functions_from_nodes() AS TABLE
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
        substring(p.peek, 1, 100) as function_signature
    FROM declarator_info di
    LEFT JOIN nodes p ON p.node_id = di.parent_id AND p.file_path = di.file_path
)
SELECT 
    file_path,
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
    param_count as parameter_count,
    -- Use definition complexity if available
    COALESCE(def_complexity, descendant_count) as complexity,
    parent_type,
    function_signature
FROM definition_info
WHERE full_name IS NOT NULL;

-- Extract functions from a specific file pattern
CREATE OR REPLACE MACRO extract_functions_from_file(file_pattern) AS TABLE
SELECT *
FROM extract_functions_from_nodes()
WHERE file_path LIKE file_pattern;

-- Get function statistics by file
CREATE OR REPLACE MACRO function_stats_by_file() AS TABLE
SELECT 
    file_path,
    COUNT(*) as function_count,
    COUNT(DISTINCT class_name) as class_count,
    AVG(parameter_count) as avg_parameters,
    AVG(complexity) as avg_complexity,
    SUM(CASE WHEN function_type = 'method' THEN 1 ELSE 0 END) as method_count,
    SUM(CASE WHEN function_type = 'function' THEN 1 ELSE 0 END) as standalone_function_count
FROM extract_functions_from_nodes()
GROUP BY file_path
ORDER BY function_count DESC;

-- Find most complex functions
CREATE OR REPLACE MACRO most_complex_functions(limit_count := 20) AS TABLE
SELECT 
    function_name,
    class_name,
    file_path,
    start_line,
    end_line,
    parameter_count,
    complexity,
    end_line - start_line + 1 as line_count
FROM extract_functions_from_nodes()
ORDER BY complexity DESC
LIMIT limit_count;

-- Find functions with most parameters
CREATE OR REPLACE MACRO functions_with_most_parameters(limit_count := 20) AS TABLE
SELECT 
    function_name,
    class_name,
    file_path,
    start_line,
    parameter_count,
    complexity
FROM extract_functions_from_nodes()
WHERE parameter_count > 0
ORDER BY parameter_count DESC
LIMIT limit_count;

-- Get function name frequency analysis
CREATE OR REPLACE MACRO function_name_frequency() AS TABLE
SELECT 
    simple_name as function_name,
    COUNT(*) as occurrence_count,
    COUNT(DISTINCT file_path) as files_used_in,
    AVG(parameter_count) as avg_parameters,
    MIN(complexity) as min_complexity,
    MAX(complexity) as max_complexity
FROM extract_functions_from_nodes()
GROUP BY simple_name
ORDER BY occurrence_count DESC;

-- Extract class information with method counts
CREATE OR REPLACE MACRO extract_classes_with_methods() AS TABLE
WITH class_methods AS (
    SELECT 
        file_path,
        class_name,
        COUNT(*) as method_count,
        AVG(parameter_count) as avg_method_parameters,
        AVG(complexity) as avg_method_complexity,
        MIN(start_line) as class_start_line,
        MAX(end_line) as class_end_line
    FROM extract_functions_from_nodes()
    WHERE class_name IS NOT NULL
    GROUP BY file_path, class_name
)
SELECT 
    file_path,
    class_name,
    method_count,
    avg_method_parameters,
    avg_method_complexity,
    class_start_line,
    class_end_line,
    class_end_line - class_start_line + 1 as class_line_span
FROM class_methods
ORDER BY method_count DESC;

-- Summary statistics for the entire codebase
CREATE OR REPLACE MACRO codebase_function_summary() AS TABLE
SELECT 
    COUNT(*) as total_functions,
    COUNT(DISTINCT file_path) as files_with_functions,
    COUNT(DISTINCT class_name) as total_classes,
    AVG(parameter_count) as avg_parameters_per_function,
    AVG(complexity) as avg_complexity_per_function,
    SUM(CASE WHEN function_type = 'method' THEN 1 ELSE 0 END) as total_methods,
    SUM(CASE WHEN function_type = 'function' THEN 1 ELSE 0 END) as total_standalone_functions,
    MAX(parameter_count) as max_parameters,
    MAX(complexity) as max_complexity
FROM extract_functions_from_nodes();