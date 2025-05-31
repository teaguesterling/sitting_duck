-- JSON approach initialization
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Create test data with JSON
CREATE TEMP TABLE ast_data AS
SELECT 
    file_path,
    nodes as json_nodes
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Helper macro for JSON filtering
CREATE MACRO filter_json_by_type(type_name) AS (
    SELECT COUNT(*) as count
    FROM ast_data,
         json_each(json_nodes::JSON) as je
    WHERE json_extract_string(je.value, '$.type') = type_name
);

.print "JSON setup complete. Test with: SELECT filter_json_by_type('function_definition');"