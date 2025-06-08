-- Test queries for DuckDB AST extension

-- Load the extension
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test basic functionality
SELECT * FROM read_ast('workspace/sandbox/test_ast.py', 'python') LIMIT 10;

-- Count nodes by type
SELECT type, COUNT(*) as count
FROM read_ast('workspace/sandbox/test_ast.py', 'python')
GROUP BY type
ORDER BY count DESC;

-- Find all functions
SELECT node_id, name, start_line, end_line, depth
FROM read_ast('workspace/sandbox/test_ast.py', 'python')
WHERE type = 'function_definition'
ORDER BY start_line;

-- Find all classes
SELECT node_id, name, start_line, end_line
FROM read_ast('workspace/sandbox/test_ast.py', 'python')  
WHERE type = 'class_definition';

-- Show tree structure for first 20 nodes
SELECT 
    REPEAT('  ', depth) || type AS tree_structure,
    name,
    start_line,
    LENGTH(source_text) as source_len
FROM read_ast('workspace/sandbox/test_ast.py', 'python')
LIMIT 20;