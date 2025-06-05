LOAD 'build/debug/extension/duckdb_ast/duckdb_ast.duckdb_extension';
SELECT COUNT(*) FROM parse_ast('def hello(): pass', 'python');