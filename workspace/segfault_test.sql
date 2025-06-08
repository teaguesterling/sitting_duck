-- Minimal reproducer for segfault
-- Let's start with the simplest possible queries and build up

-- Test 1: Basic extension loading (this should work)
SELECT 'Extension test' as test_name;

-- Test 2: Simple read_ast call
SELECT 'Simple read_ast test' as test_name;
SELECT COUNT(*) FROM read_ast('src/unified_ast_backend.cpp');

-- Test 3: Basic WHERE clause
SELECT 'Basic WHERE test' as test_name;
SELECT COUNT(*) FROM read_ast('src/unified_ast_backend.cpp')
WHERE type = 'function_definition';

-- Test 4: Simple JOIN (potential issue)
SELECT 'Simple JOIN test' as test_name;
WITH t1 AS (
    SELECT node_id, type FROM read_ast('src/unified_ast_backend.cpp') LIMIT 10
),
t2 AS (
    SELECT node_id, parent_id FROM read_ast('src/unified_ast_backend.cpp') LIMIT 10
)
SELECT COUNT(*) FROM t1 JOIN t2 ON t1.node_id = t2.parent_id;

-- Test 5: Multiple read_ast calls (potential issue)
SELECT 'Multiple read_ast test' as test_name;
SELECT COUNT(*) FROM (
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
    UNION ALL
    SELECT * FROM read_ast('src/unified_ast_backend.cpp')
) LIMIT 5;

-- Test 6: Complex aggregation
SELECT 'Complex aggregation test' as test_name;
SELECT 
    type,
    COUNT(*) as count
FROM read_ast('src/unified_ast_backend.cpp')
GROUP BY type
LIMIT 5;

-- Test 7: Self-join (this was the segfault pattern)
SELECT 'Self-join test' as test_name;
SELECT COUNT(*) FROM (
    SELECT f.node_id, c.type
    FROM read_ast('src/unified_ast_backend.cpp') f
    JOIN read_ast('src/unified_ast_backend.cpp') c ON c.parent_id = f.node_id
    LIMIT 10
);