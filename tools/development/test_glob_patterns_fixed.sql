-- Test glob pattern functionality for the AST extension
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test 1: Check available columns
.print "=== Available columns ==="
SELECT * FROM read_ast('src/ast_type.cpp') LIMIT 1;

-- Test 2: Single file (basic functionality)
.print "=== Test 2: Single file ==="
SELECT COUNT(*) as node_count, file_path
FROM read_ast('src/unified_ast_backend.cpp') 
LIMIT 5;

-- Test 3: Glob pattern for C++ files
.print "=== Test 3: Glob pattern for C++ files ==="
SELECT file_path, COUNT(*) as node_count
FROM read_ast('src/*.cpp')
GROUP BY file_path
ORDER BY file_path;

-- Test 4: File list
.print "=== Test 4: File list ==="
SELECT file_path, COUNT(*) as node_count
FROM read_ast(['src/ast_type.cpp', 'src/semantic_types.cpp'])
GROUP BY file_path;

-- Test 5: With ignore_errors parameter
.print "=== Test 5: With ignore_errors ==="
SELECT COUNT(*) as found_files
FROM read_ast('src/*.nonexistent', ignore_errors := true);

-- Test 6: read_ast_objects with glob pattern
.print "=== Test 6: read_ast_objects with glob ==="
SELECT LENGTH(ast.source.file_path) > 0 as has_file_path,
       ast.source.language,
       LENGTH(ast.nodes) as node_count
FROM read_ast_objects('src/ast_type.cpp');

-- Test 7: Test recursive glob pattern
.print "=== Test 7: Recursive glob pattern ==="
SELECT COUNT(*) as total_nodes
FROM read_ast('src/**/*.hpp');

.print "=== All tests completed successfully! ==="