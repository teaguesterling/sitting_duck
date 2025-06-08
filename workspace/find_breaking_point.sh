#!/bin/bash

# Find the exact breaking point for read_ast segfaults
# This incrementally adds complexity until we hit the segfault

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DUCKDB="$PROJECT_DIR/build/release/duckdb"

echo "=== FINDING EXACT SEGFAULT BREAKING POINT ==="
echo ""

# Helper function to test a query
test_query() {
    local test_name="$1"
    local query="$2"
    echo -n "$test_name: "
    
    timeout 3s "$DUCKDB" -s "$query" >/dev/null 2>&1
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo "✅ WORKS"
        return 0
    elif [ $exit_code -eq 124 ]; then
        echo "⏰ TIMEOUT"
        return 1
    else
        echo "❌ SEGFAULT"
        return 1
    fi
}

# Test 1: Basic CTE + JOIN
test_query "CTE + Single JOIN" "
WITH cte AS (SELECT node_id FROM read_ast('src/unified_ast_backend.cpp') LIMIT 5)
SELECT COUNT(*) FROM cte c1 JOIN cte c2 ON c1.node_id = c2.node_id;
"

# Test 2: CTE + External JOIN
test_query "CTE + External JOIN" "
WITH cte AS (SELECT node_id FROM read_ast('src/unified_ast_backend.cpp') LIMIT 5)
SELECT COUNT(*) FROM cte c 
JOIN read_ast('src/unified_ast_backend.cpp') r ON c.node_id = r.parent_id;
"

# Test 3: Two External JOINs
test_query "Two External JOINs" "
SELECT COUNT(*) FROM 
read_ast('src/unified_ast_backend.cpp') r1 
JOIN read_ast('src/unified_ast_backend.cpp') r2 ON r1.node_id = r2.parent_id 
LIMIT 5;
"

# Test 4: Add LEFT JOIN
test_query "Add LEFT JOIN (suspected culprit)" "
SELECT COUNT(*) FROM 
read_ast('src/unified_ast_backend.cpp') r1 
JOIN read_ast('src/unified_ast_backend.cpp') r2 ON r1.node_id = r2.parent_id
LEFT JOIN read_ast('src/unified_ast_backend.cpp') r3 ON r2.node_id = r3.parent_id
LIMIT 5;
"

# Test 5: Three JOINs
test_query "Three JOINs" "
SELECT COUNT(*) FROM 
read_ast('src/unified_ast_backend.cpp') r1 
JOIN read_ast('src/unified_ast_backend.cpp') r2 ON r1.node_id = r2.parent_id
JOIN read_ast('src/unified_ast_backend.cpp') r3 ON r2.node_id = r3.parent_id
LIMIT 5;
"

# Test 6: Different JOIN conditions
test_query "Different JOIN conditions" "
SELECT COUNT(*) FROM 
read_ast('src/unified_ast_backend.cpp') r1 
JOIN read_ast('src/unified_ast_backend.cpp') r2 ON r1.node_id = r2.node_id
LEFT JOIN read_ast('src/unified_ast_backend.cpp') r3 ON r1.start_line = r3.start_line
LIMIT 5;
"

# Test 7: UNION instead of JOIN
test_query "UNION (alternative approach)" "
SELECT COUNT(*) FROM (
    SELECT node_id FROM read_ast('src/unified_ast_backend.cpp') LIMIT 2
    UNION ALL
    SELECT node_id FROM read_ast('src/unified_ast_backend.cpp') LIMIT 2
);
"

echo ""
echo "=== ANALYSIS ==="
echo "The breaking point helps identify:"
echo "1. Is it the number of read_ast() calls?"
echo "2. Is it specific JOIN types (LEFT vs INNER)?"  
echo "3. Is it the combination of CTE + multiple external calls?"
echo "4. Is it the complexity of JOIN conditions?"
echo ""
echo "Next: Examine the table function state management in C++ code"