#!/bin/bash

# Extract actual function body source code
# Usage: ./extract_function_body.sh <file_path> <function_name>

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DUCKDB="$PROJECT_DIR/build/release/duckdb"

if [ $# -lt 2 ]; then
    echo "Usage: $0 <file_path> <function_name> [--with-line-numbers]"
    echo ""
    echo "Examples:"
    echo "  $0 src/unified_ast_backend.cpp ParseToASTResult"
    echo "  $0 src/unified_ast_backend.cpp PopulateSemanticFields --with-line-numbers"
    exit 1
fi

FILE_PATH="$1"
FUNCTION_NAME="$2"
WITH_LINES=""

if [ "$3" = "--with-line-numbers" ]; then
    WITH_LINES="-n"
fi

# Check if file exists
if [ ! -f "$FILE_PATH" ]; then
    echo "Error: File '$FILE_PATH' not found"
    exit 1
fi

# Get function line numbers using our corrected extraction
echo "=== Extracting function: $FUNCTION_NAME ==="

# Check if jq is available
if ! command -v jq &> /dev/null; then
    echo "Error: jq is required for JSON parsing but not found"
    echo "Please install jq: sudo apt-get install jq"
    exit 1
fi

# Find matching functions using JSON output (safe single read_ast call)
MATCHES_JSON=$("$DUCKDB" -json -s "
WITH full_ast AS (
    FROM read_ast('$FILE_PATH')
),
function_definitions AS (
    SELECT node_id, start_line, end_line
    FROM full_ast
    WHERE type = 'function_definition' AND semantic_type = 112
),
function_info AS (
    SELECT 
        fd.start_line,
        fd.end_line,
        MAX(CASE 
            WHEN decl.sibling_index = 1 AND child.type IN ('identifier', 'qualified_identifier')
            THEN child.name
        END) as full_name
    FROM function_definitions fd
    JOIN full_ast decl ON decl.parent_id = fd.node_id AND decl.sibling_index = 1 AND decl.type = 'function_declarator'
    LEFT JOIN full_ast child ON child.parent_id = decl.node_id
    GROUP BY fd.node_id, fd.start_line, fd.end_line
)
SELECT full_name, start_line, end_line
FROM function_info
WHERE full_name ILIKE '%$FUNCTION_NAME%' AND full_name IS NOT NULL
ORDER BY start_line;
")

# Debug: show what we got
if [ -z "$MATCHES_JSON" ]; then
    echo "Debug: Empty JSON result"
    MATCH_COUNT=0
else
    # Count matches
    MATCH_COUNT=$(echo "$MATCHES_JSON" | jq '. | length' 2>/dev/null || echo "0")
fi

if [ "$MATCH_COUNT" -eq 0 ]; then
    echo "Error: No functions matching '$FUNCTION_NAME' found in $FILE_PATH"
    echo ""
    echo "Available functions:"
    "$DUCKDB" -json -s "
    WITH full_ast AS (FROM read_ast('$FILE_PATH')),
    function_definitions AS (SELECT node_id FROM full_ast WHERE type = 'function_definition' AND semantic_type = 112),
    function_info AS (
        SELECT MAX(CASE WHEN child.type IN ('qualified_identifier') THEN child.name END) as full_name
        FROM function_definitions fd
        JOIN full_ast decl ON decl.parent_id = fd.node_id AND decl.sibling_index = 1 AND decl.type = 'function_declarator'
        LEFT JOIN full_ast child ON child.parent_id = decl.node_id
        GROUP BY fd.node_id
    )
    SELECT full_name FROM function_info WHERE full_name IS NOT NULL ORDER BY full_name;
    " | jq -r '.[] | "  " + .full_name'
    exit 1
elif [ "$MATCH_COUNT" -gt 1 ]; then
    echo "Multiple functions match '$FUNCTION_NAME':"
    echo "$MATCHES_JSON" | jq -r '.[] | "  " + .full_name + " (lines " + (.start_line|tostring) + "-" + (.end_line|tostring) + ")"'
    echo ""
    echo "Please be more specific. Using first match:"
    START_LINE=$(echo "$MATCHES_JSON" | jq -r '.[0].start_line')
    END_LINE=$(echo "$MATCHES_JSON" | jq -r '.[0].end_line')
    SELECTED_FUNCTION=$(echo "$MATCHES_JSON" | jq -r '.[0].full_name')
    echo "Selected: $SELECTED_FUNCTION"
else
    START_LINE=$(echo "$MATCHES_JSON" | jq -r '.[0].start_line')
    END_LINE=$(echo "$MATCHES_JSON" | jq -r '.[0].end_line')
fi

echo "Lines: $START_LINE-$END_LINE"
echo "Location: $FILE_PATH:$START_LINE"
echo ""

# Extract the actual function body
sed -n "${START_LINE},${END_LINE}p" "$FILE_PATH" | sed "${WITH_LINES}"