LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';
SELECT jt.key, json_extract_string(jt.value, '$.type') as type, jt.type as json_type 
FROM read_ast_objects('test/data/python/simple.py', 'python') AS ast,
     json_tree(json_extract(ast.nodes, '$[1]')) AS jt
WHERE jt.type != 'ARRAY' AND jt.type != 'OBJECT' AND jt.key IS NOT NULL
ORDER BY jt.id
LIMIT 10;