#!/bin/bash

# Corrected function extraction with accurate line numbers
# Usage: ./extract_functions_corrected.sh <file_path> [--verbose] [--limit N]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DUCKDB="$PROJECT_DIR/build/release/duckdb"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <file_path> [--verbose] [--limit N]"
    echo ""
    echo "Examples:"
    echo "  $0 src/unified_ast_backend.cpp"
    echo "  $0 src/unified_ast_backend.cpp --limit 5"
    echo "  $0 src/unified_ast_backend.cpp --verbose"
    exit 1
fi

FILE_PATH="$1"
shift

# Parse options
VERBOSE=false
LIMIT=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose)
            VERBOSE=true
            shift
            ;;
        --limit)
            LIMIT="LIMIT $2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check if file exists
if [ ! -f "$FILE_PATH" ]; then
    echo "Error: File '$FILE_PATH' not found"
    exit 1
fi

# Check if DuckDB binary exists
if [ ! -f "$DUCKDB" ]; then
    echo "Error: DuckDB binary not found at $DUCKDB"
    echo "Please run 'make' first to build the extension"
    exit 1
fi

if [ "$VERBOSE" = true ]; then
    echo "Extracting functions from: $FILE_PATH"
    echo "Using corrected line numbers from function_definition nodes"
    echo "Using DuckDB: $DUCKDB"
    echo ""
fi

# Build the corrected SQL query (single read_ast call to avoid segfault)
SQL="
-- Extract functions with CORRECT line numbers using function_definition nodes
WITH ast_data AS (
    SELECT * FROM read_ast('$FILE_PATH')
),
function_definitions AS (
    -- Use function_definition nodes (they have the correct full span)
    SELECT node_id, start_line, end_line, children_count, descendant_count
    FROM ast_data
    WHERE type = 'function_definition' AND semantic_type = 112
),
function_info AS (
    SELECT 
        fd.node_id,
        fd.start_line,
        fd.end_line,
        fd.end_line - fd.start_line + 1 as line_count,
        fd.descendant_count,
        -- Get function name from declarator child (sibling_index = 1)
        MAX(CASE 
            WHEN decl.sibling_index = 1 AND child.type IN ('identifier', 'qualified_identifier')
            THEN child.name
        END) as full_name,
        -- Get parameter count from parameter_list
        MAX(CASE 
            WHEN decl.sibling_index = 1 AND child.type = 'parameter_list'
            THEN child.children_count 
            ELSE 0
        END) as param_count,
        -- Get return type from first child (sibling_index = 0)
        MAX(CASE 
            WHEN ret.sibling_index = 0 
            THEN ret.name || ' (' || ret.type || ')'
        END) as return_info
    FROM function_definitions fd
    -- Join to get function_declarator (sibling_index = 1)
    JOIN ast_data decl ON decl.parent_id = fd.node_id 
                       AND decl.sibling_index = 1 
                       AND decl.type = 'function_declarator'
    -- Join to get children of declarator (name and parameters)
    LEFT JOIN ast_data child ON child.parent_id = decl.node_id
    -- Join to get return type (sibling_index = 0 of function_definition)
    LEFT JOIN ast_data ret ON ret.parent_id = fd.node_id AND ret.sibling_index = 0
    GROUP BY fd.node_id, fd.start_line, fd.end_line, fd.descendant_count
),
-- Calculate complexity metrics
complexity_metrics AS (
    SELECT 
        fi.node_id,
        COUNT(CASE 
            WHEN d.type IN ('if_statement', 'for_statement', 'while_statement', 
                           'do_statement', 'switch_statement', 'conditional_expression')
            THEN 1 
        END) as cyclomatic_complexity,
        COUNT(CASE WHEN d.type = 'call_expression' THEN 1 END) as call_count,
        COUNT(CASE WHEN d.type = 'return_statement' THEN 1 END) as return_count
    FROM function_info fi
    JOIN ast_data d ON d.node_id BETWEEN fi.node_id AND fi.node_id + fi.descendant_count
    GROUP BY fi.node_id
)
SELECT 
    CASE 
        WHEN fi.full_name LIKE '%::%' 
        THEN split_part(fi.full_name, '::', 2)
        ELSE fi.full_name
    END as function_name,
    CASE 
        WHEN fi.full_name LIKE '%::%' 
        THEN split_part(fi.full_name, '::', 1)
        ELSE NULL
    END as class_name,
    CASE 
        WHEN fi.full_name LIKE '%::%' THEN 'method'
        ELSE 'function'
    END as function_type,
    fi.start_line,
    fi.end_line,
    fi.line_count,
    fi.param_count,
    cm.cyclomatic_complexity,
    cm.call_count,
    cm.return_count,
    fi.return_info,
    fi.full_name as qualified_name
FROM function_info fi
LEFT JOIN complexity_metrics cm ON cm.node_id = fi.node_id
WHERE fi.full_name IS NOT NULL
ORDER BY fi.start_line
$LIMIT;
"

# Execute the query
if [ "$VERBOSE" = true ]; then
    echo "Executing corrected query..."
    echo ""
fi

"$DUCKDB" -s "$SQL"