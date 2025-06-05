LOAD 'build/debug/extension/duckdb_ast/duckdb_ast.duckdb_extension';
LOAD 'build/debug/extension/json/json.duckdb_extension';
SELECT COUNT(*) FROM parse_ast('x = 1', 'python');