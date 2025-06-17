.timer on

-- Test parsing performance on a larger file
SELECT COUNT(*) AS node_count, 
       AVG(children_count) AS avg_children,
       MAX(depth) AS max_depth
FROM read_ast('src/unified_ast_backend.cpp', 'cpp');

-- Test parsing multiple files
SELECT COUNT(*) AS total_nodes,
       COUNT(DISTINCT file_path) AS file_count
FROM read_ast('src/language_adapters/*.cpp', 'cpp');

-- Test different language parsers
SELECT language, COUNT(*) AS node_count
FROM (
    SELECT 'python' AS language, COUNT(*) AS cnt FROM read_ast('test/data/test_class_analysis.py', 'python')
    UNION ALL
    SELECT 'javascript', COUNT(*) FROM read_ast('test/data/test_ts_types.ts', 'typescript')
    UNION ALL
    SELECT 'cpp', COUNT(*) FROM read_ast('src/unified_ast_backend.cpp', 'cpp')
) GROUP BY language;