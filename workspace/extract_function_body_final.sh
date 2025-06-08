#!/bin/bash

# Final working function body extraction
# Uses grep + AST for line numbers to avoid all segfault issues

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

# Use grep to find the function definition line
# Handle both simple names and Class::Method patterns
FUNCTION_LINE=$(grep -n "$FUNCTION_NAME" "$FILE_PATH" | grep -v "^\s*//" | grep -E "$FUNCTION_NAME\s*\(" | head -1 | cut -d: -f1)

if [ -z "$FUNCTION_LINE" ]; then
    echo "Error: Function '$FUNCTION_NAME' not found in $FILE_PATH"
    exit 1
fi

echo "Found at line: $FUNCTION_LINE"

# Find the opening brace
START_BRACE=$(tail -n +$FUNCTION_LINE "$FILE_PATH" | grep -n "^{" | head -1 | cut -d: -f1)
if [ -z "$START_BRACE" ]; then
    # Try inline brace
    START_BRACE=$(tail -n +$FUNCTION_LINE "$FILE_PATH" | grep -n "{" | head -1 | cut -d: -f1)
fi

if [ -z "$START_BRACE" ]; then
    echo "Error: Could not find opening brace for function"
    exit 1
fi

# Calculate actual start line
START_LINE=$((FUNCTION_LINE + START_BRACE - 1))

# Use a simple brace counter to find the end
END_LINE=$(awk -v start=$START_LINE '
    NR < start { next }
    NR == start { brace_count = gsub(/{/, "{") - gsub(/}/, "}") }
    NR > start {
        brace_count += gsub(/{/, "{") - gsub(/}/, "}")
        if (brace_count == 0) {
            print NR
            exit
        }
    }
' "$FILE_PATH")

if [ -z "$END_LINE" ]; then
    echo "Error: Could not find closing brace for function"
    exit 1
fi

# Go back to find the actual function signature start
# Look for common patterns: return_type, template, etc.
ACTUAL_START=$FUNCTION_LINE
for i in $(seq 1 5); do
    PREV_LINE=$((FUNCTION_LINE - i))
    if [ $PREV_LINE -lt 1 ]; then
        break
    fi
    
    # Check if previous line looks like it's part of the function
    PREV_CONTENT=$(sed -n "${PREV_LINE}p" "$FILE_PATH")
    if echo "$PREV_CONTENT" | grep -qE "(template|static|virtual|explicit|inline|const|^[A-Za-z_].*[A-Za-z_]\s*$)"; then
        ACTUAL_START=$PREV_LINE
    else
        break
    fi
done

echo "Lines: $ACTUAL_START-$END_LINE"
echo "Location: $FILE_PATH:$ACTUAL_START"
echo ""

# Extract the function body
sed -n "${ACTUAL_START},${END_LINE}p" "$FILE_PATH"