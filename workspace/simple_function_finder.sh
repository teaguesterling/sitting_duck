#!/bin/bash

# Simple function finder that doesn't trigger segfaults
# Uses basic grep + line numbers for now

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FILE_PATH="$1"
FUNCTION_NAME="$2"

if [ $# -lt 2 ]; then
    echo "Usage: $0 <file_path> <function_name>"
    echo "Example: $0 src/language_adapter.cpp GetInstance"
    exit 1
fi

echo "=== Searching for '$FUNCTION_NAME' in $FILE_PATH ==="

# Use grep to find function definitions
grep -n "$FUNCTION_NAME" "$FILE_PATH" | head -5

echo ""
echo "Note: This is a simple grep-based finder to avoid segfaults."
echo "Use it to get approximate line numbers, then examine the code manually."