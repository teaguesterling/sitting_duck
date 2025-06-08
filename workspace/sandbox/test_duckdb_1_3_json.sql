-- Test new JSON functions in DuckDB 1.3

-- Test json_group_array (should be available in 1.3)
SELECT json_group_array(x) FROM (VALUES (1), (2), (3)) t(x);

-- Test json_group_object 
SELECT json_group_object(k, v) FROM (VALUES ('a', 1), ('b', 2)) t(k, v);

-- Test other JSON aggregates
SELECT json_object_agg(k, v) FROM (VALUES ('a', 1), ('b', 2)) t(k, v);

-- Test JSON path functions
SELECT json_extract('{"a": {"b": [1,2,3]}}', '$.a.b[1]');

-- Test JSON array functions
SELECT json_array_length('[1,2,3,4,5]');

-- List all available functions with 'json' in the name
SELECT DISTINCT function_name 
FROM duckdb_functions() 
WHERE function_name LIKE '%json%'
ORDER BY function_name;