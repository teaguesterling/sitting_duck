#!/bin/bash

# Systematic segfault debugging using our AST tools
# This script isolates the exact patterns that cause segfaults

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DUCKDB="$PROJECT_DIR/build/release/duckdb"

echo "=== SEGFAULT DEBUGGING: SYSTEMATIC APPROACH ==="
echo "Using file: src/unified_ast_backend.cpp"
echo ""

# Test 1: Single read_ast call (should work)
echo "1. Testing single read_ast call..."
"$DUCKDB" -column -s "SELECT COUNT(*) as total_nodes FROM read_ast('src/unified_ast_backend.cpp');" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "   ✅ Single read_ast call: WORKS"
else
    echo "   ❌ Single read_ast call: FAILS"
fi

# Test 2: Simple self-join (might work)
echo "2. Testing simple self-join..."
timeout 5s "$DUCKDB" -column -s "
WITH funcs AS (SELECT node_id FROM read_ast('src/unified_ast_backend.cpp') WHERE type = 'function_definition' LIMIT 2)
SELECT COUNT(*) FROM funcs f1 JOIN funcs f2 ON f1.node_id = f2.node_id;
" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "   ✅ Simple self-join: WORKS"
else
    echo "   ❌ Simple self-join: FAILS"
fi

# Test 3: Two read_ast calls (likely to fail)
echo "3. Testing two read_ast calls..."
timeout 5s "$DUCKDB" -column -s "
SELECT COUNT(*) FROM (
    SELECT node_id FROM read_ast('src/unified_ast_backend.cpp') LIMIT 1
) a JOIN (
    SELECT node_id FROM read_ast('src/unified_ast_backend.cpp') LIMIT 1  
) b ON a.node_id = b.node_id;
" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "   ✅ Two read_ast calls: WORKS"
else
    echo "   ❌ Two read_ast calls: FAILS"
fi

# Test 4: Complex join with multiple read_ast (very likely to fail)
echo "4. Testing complex multi-join..."
timeout 5s "$DUCKDB" -column -s "
WITH func_defs AS (SELECT node_id FROM read_ast('src/unified_ast_backend.cpp') WHERE type = 'function_definition' LIMIT 1)
SELECT COUNT(*) FROM func_defs fd
JOIN read_ast('src/unified_ast_backend.cpp') d ON d.parent_id = fd.node_id
LEFT JOIN read_ast('src/unified_ast_backend.cpp') c ON c.parent_id = d.node_id;
" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "   ✅ Complex multi-join: WORKS"
else
    echo "   ❌ Complex multi-join: FAILS"
fi

echo ""
echo "=== HYPOTHESIS TESTING ==="

# Test 5: File size impact
echo "5. Testing with smaller file..."
timeout 5s "$DUCKDB" -column -s "
WITH func_defs AS (SELECT node_id FROM read_ast('test/simple_setup.sql') WHERE type = 'function_definition' LIMIT 1)
SELECT COUNT(*) FROM func_defs fd
JOIN read_ast('test/simple_setup.sql') d ON d.parent_id = fd.node_id;
" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "   ✅ Smaller file multi-join: WORKS"
else
    echo "   ❌ Smaller file multi-join: FAILS"
fi

# Test 6: Memory pressure test
echo "6. Testing memory pressure (large result set)..."
timeout 10s "$DUCKDB" -column -s "
SELECT COUNT(*) FROM read_ast('src/unified_ast_backend.cpp') a 
CROSS JOIN (SELECT 1 as dummy UNION SELECT 2 UNION SELECT 3) b;
" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "   ✅ Memory pressure test: WORKS"
else
    echo "   ❌ Memory pressure test: FAILS"
fi

echo ""
echo "=== POTENTIAL ROOT CAUSES ==="
echo "Based on test results:"
echo "- If tests 1-2 work but 3-4 fail: Multiple read_ast() calls are the issue"
echo "- If test 5 works but 4 fails: Large file size exacerbates the problem"  
echo "- If test 6 fails: Memory management issues in table function"
echo ""
echo "Likely culprits to investigate:"
echo "1. Table function state management between multiple calls"
echo "2. Tree-sitter parser reuse conflicts"
echo "3. Smart pointer lifecycle in UnifiedASTBackend"
echo "4. Memory allocation patterns in large AST structures"