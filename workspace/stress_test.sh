#!/bin/bash

# Stress test to find intermittent segfaults
cd /mnt/aux-data/teague/Projects/duckdb_ast

echo "Running stress test for segfault detection..."

# Test 1: Multiple instances running the same query
echo "=== Test 1: Multiple sequential runs ==="
for i in {1..20}; do
    echo -n "Run $i: "
    timeout 10s ./build/release/duckdb -s "
    SELECT COUNT(*) FROM read_ast('src/unified_ast_backend.cpp');
    " > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo "OK"
    elif [ $? -eq 124 ]; then
        echo "TIMEOUT"
    else
        echo "FAILED (exit code $?)"
    fi
done

# Test 2: Complex join queries (the pattern that caused segfault)
echo "=== Test 2: Complex join patterns ==="
for i in {1..10}; do
    echo -n "Join test $i: "
    timeout 15s ./build/release/duckdb -s "
    WITH func_defs AS (
        SELECT node_id, start_line
        FROM read_ast('src/unified_ast_backend.cpp')
        WHERE type = 'function_definition'
        LIMIT 3
    )
    SELECT 
        fd.node_id,
        MAX(CASE WHEN c.sibling_index = 0 THEN c.name END) as return_type
    FROM func_defs fd
    LEFT JOIN read_ast('src/unified_ast_backend.cpp') c ON c.parent_id = fd.node_id
    GROUP BY fd.node_id;
    " > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo "OK"
    elif [ $? -eq 124 ]; then
        echo "TIMEOUT"
    else
        echo "FAILED (exit code $?)"
    fi
done

# Test 3: Rapid fire initialization (auto-loading issue)
echo "=== Test 3: Rapid initialization ==="
for i in {1..10}; do
    echo -n "Init test $i: "
    timeout 5s ./build/release/duckdb -s "SELECT 'initialized';" > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo "OK"
    elif [ $? -eq 124 ]; then
        echo "TIMEOUT"
    else
        echo "FAILED (exit code $?)"
    fi
done

echo "Stress test complete."