-- Benchmark the templated parsing performance
.timer on

-- Warmup
SELECT COUNT(*) FROM read_ast('src/unified_ast_backend.cpp', 'cpp') WHERE depth > 5;

-- Test 1: Parse a large C++ file multiple times
SELECT 'Test 1: Large C++ file' AS test;
SELECT COUNT(*) AS total_nodes,
       SUM(CASE WHEN semantic_type = 1 THEN 1 ELSE 0 END) AS functions,
       SUM(CASE WHEN semantic_type = 2 THEN 1 ELSE 0 END) AS classes
FROM read_ast('src/unified_ast_backend.cpp', 'cpp');

-- Test 2: Parse multiple Python files
SELECT 'Test 2: Multiple Python files' AS test;
SELECT COUNT(*) AS total_nodes,
       COUNT(DISTINCT file_path) AS files
FROM read_ast('src/language_adapters/python_adapter.cpp', 'cpp');

-- Test 3: Complex query with filtering
SELECT 'Test 3: Complex filtering' AS test;
SELECT type, COUNT(*) AS count
FROM read_ast('src/unified_ast_backend.cpp', 'cpp')
WHERE children_count > 3 AND depth BETWEEN 2 AND 10
GROUP BY type
ORDER BY count DESC
LIMIT 10;