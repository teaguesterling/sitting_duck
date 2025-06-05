LOAD 'build/debug/extension/duckdb_ast/duckdb_ast.duckdb_extension';
SELECT function_name FROM duckdb_functions() WHERE function_name LIKE '%parse%' OR function_name LIKE '%ast%';