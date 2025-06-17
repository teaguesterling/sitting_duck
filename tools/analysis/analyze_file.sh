#!/bin/bash
# Simple file function analyzer using DuckDB AST extension
# Usage: ./analyze_file.sh <file_path> [database_path]

FILE_PATH="$1"
DB_PATH="${2:-test2.db}"

if [ -z "$FILE_PATH" ]; then
    echo "Usage: $0 <file_path> [database_path]"
    echo "Example: $0 src/semantic_type_functions.cpp"
    echo "Example: $0 src/unified_ast_backend.cpp test2.db"
    exit 1
fi

echo "Function Analysis for: $FILE_PATH"
echo "========================================"
echo

./build/release/duckdb "$DB_PATH" -c "
WITH func_declarators AS (
    SELECT 
        file_path, node_id, parent_id, start_line, end_line, children_count, descendant_count
    FROM nodes
    WHERE type = 'function_declarator'
      AND semantic_type = 112
      AND file_path = '$FILE_PATH'
),
declarator_info AS (
    SELECT 
        d.file_path, d.node_id, d.start_line, d.end_line, d.parent_id, d.descendant_count,
        MAX(CASE WHEN c.type = 'identifier' THEN c.peek END) as full_name,
        SUM(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count
    FROM func_declarators d
    JOIN nodes c ON c.parent_id = d.node_id AND c.file_path = d.file_path
    GROUP BY d.file_path, d.node_id, d.start_line, d.end_line, d.parent_id, d.descendant_count
),
definition_info AS (
    SELECT 
        di.*, p.type as parent_type, p.start_line as def_start_line, p.end_line as def_end_line,
        p.descendant_count as def_complexity, substring(p.peek, 1, 80) as function_signature
    FROM declarator_info di
    LEFT JOIN nodes p ON p.node_id = di.parent_id AND p.file_path = di.file_path
)
SELECT 
    full_name as function_name,
    CASE 
        WHEN full_name LIKE '%::%' THEN 'method'
        ELSE 'function'
    END as type,
    COALESCE(def_start_line, start_line) as start_line,
    COALESCE(def_end_line, end_line) as end_line,
    COALESCE(def_end_line, end_line) - COALESCE(def_start_line, start_line) + 1 as lines,
    param_count as params,
    COALESCE(def_complexity, descendant_count) as complexity,
    function_signature
FROM definition_info
WHERE full_name IS NOT NULL
ORDER BY start_line;
"

echo
echo "Summary:"
./build/release/duckdb "$DB_PATH" -c "
WITH func_summary AS (
    SELECT 
        d.node_id, d.parent_id, d.start_line, d.end_line, d.descendant_count,
        SUM(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count,
        p.start_line as def_start_line, p.end_line as def_end_line, p.descendant_count as def_complexity
    FROM (
        SELECT node_id, parent_id, start_line, end_line, descendant_count
        FROM nodes
        WHERE type = 'function_declarator' AND semantic_type = 112 AND file_path = '$FILE_PATH'
    ) d
    JOIN nodes c ON c.parent_id = d.node_id AND c.file_path = '$FILE_PATH'
    LEFT JOIN nodes p ON p.node_id = d.parent_id AND p.file_path = '$FILE_PATH'
    GROUP BY d.node_id, d.parent_id, d.start_line, d.end_line, d.descendant_count, 
             p.start_line, p.end_line, p.descendant_count
)
SELECT 
    COUNT(*) as total_functions,
    printf('%.1f', AVG(COALESCE(def_end_line, end_line) - COALESCE(def_start_line, start_line) + 1)) as avg_lines,
    printf('%.1f', AVG(param_count)) as avg_parameters,
    printf('%.1f', AVG(COALESCE(def_complexity, descendant_count))) as avg_complexity,
    MAX(COALESCE(def_complexity, descendant_count)) as max_complexity
FROM func_summary;
"