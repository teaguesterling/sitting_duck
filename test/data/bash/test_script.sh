#!/bin/bash

# Function definition
function greet() {
    local name="$1"
    echo "Hello, ${name}!"
}

# Variables
COUNT=0
FILES=(*.txt)

# Control flow
for file in "${FILES[@]}"; do
    if [[ -f "$file" ]]; then
        echo "Processing $file"
        COUNT=$((COUNT + 1))
    fi
done

# Conditional
if [ $COUNT -gt 0 ]; then
    echo "Processed $COUNT files"
else
    echo "No files found"
fi

greet "World"