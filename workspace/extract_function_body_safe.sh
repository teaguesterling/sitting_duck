#!/bin/bash

# Safe function body extraction that avoids segfaults
# Uses procedural approach instead of complex JOINs

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DUCKDB="$PROJECT_DIR/build/release/duckdb"

if [ $# -lt 2 ]; then
    echo "Usage: $0 <file_path> <function_name>"
    echo "Example: $0 src/unified_ast_backend.cpp ParseToASTResult"
    exit 1
fi

FILE_PATH="$1"
FUNCTION_NAME="$2"

echo "=== Extracting function: $FUNCTION_NAME ==="

# Step 1: Find qualified_identifier nodes matching the function name
QUALIFIED_NODE=$("$DUCKDB" -json -s "
WITH full_ast AS (FROM read_ast('$FILE_PATH'))
SELECT node_id, parent_id, name
FROM full_ast
WHERE type = 'qualified_identifier' 
  AND name ILIKE '%$FUNCTION_NAME%'
ORDER BY node_id
LIMIT 1;
" | jq -r '.[0]' 2>/dev/null)

if [ -z "$QUALIFIED_NODE" ] || [ "$QUALIFIED_NODE" = "null" ]; then
    echo "Error: Function '$FUNCTION_NAME' not found"
    exit 1
fi

# Extract node_id and parent_id
NODE_ID=$(echo "$QUALIFIED_NODE" | jq -r '.node_id')
PARENT_ID=$(echo "$QUALIFIED_NODE" | jq -r '.parent_id')
FULL_NAME=$(echo "$QUALIFIED_NODE" | jq -r '.name')

echo "Found: $FULL_NAME"

# Step 2: Walk up the tree to find the function_definition
# Need to find the nearest ancestor that is a function_definition
FUNCTION_DEF=$("$DUCKDB" -json -s "
WITH full_ast AS (FROM read_ast('$FILE_PATH')),
-- Get all ancestors of the qualified_identifier node
ancestors AS (
    SELECT a1.node_id, a1.parent_id, a1.type, a1.start_line, a1.end_line, 1 as level
    FROM full_ast a1 WHERE a1.node_id = $PARENT_ID
    UNION ALL
    SELECT a2.node_id, a2.parent_id, a2.type, a2.start_line, a2.end_line, 2 as level
    FROM full_ast a2 WHERE a2.node_id = (SELECT parent_id FROM full_ast WHERE node_id = $PARENT_ID)
    UNION ALL
    SELECT a3.node_id, a3.parent_id, a3.type, a3.start_line, a3.end_line, 3 as level
    FROM full_ast a3 WHERE a3.node_id = (
        SELECT parent_id FROM full_ast WHERE node_id = (
            SELECT parent_id FROM full_ast WHERE node_id = $PARENT_ID
        )
    )
)
SELECT node_id, start_line, end_line, type
FROM ancestors
WHERE type = 'function_definition'
ORDER BY level
LIMIT 1;
" | jq -r '.[0]' 2>/dev/null)

if [ -z "$FUNCTION_DEF" ] || [ "$FUNCTION_DEF" = "null" ]; then
    echo "Error: Could not find function_definition for $FULL_NAME"
    exit 1
fi

# Extract line numbers
START_LINE=$(echo "$FUNCTION_DEF" | jq -r '.start_line')
END_LINE=$(echo "$FUNCTION_DEF" | jq -r '.end_line')

echo "Lines: $START_LINE-$END_LINE"
echo "Location: $FILE_PATH:$START_LINE"
echo ""

# Extract the actual function body
sed -n "${START_LINE},${END_LINE}p" "$FILE_PATH"