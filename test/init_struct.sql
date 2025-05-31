-- Struct approach initialization  
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Convert JSON to struct format
CREATE TEMP TABLE ast_data AS
SELECT 
    file_path,
    -- Convert to struct array
    (SELECT ARRAY_AGG(STRUCT_PACK(
        node_id := CAST(json_extract(node, '$.id') AS INTEGER),
        type := json_extract_string(node, '$.type'),
        name := json_extract_string(node, '$.name'),
        depth := CAST(json_extract(node, '$.depth') AS INTEGER),
        file_path := json_extract_string(node, '$.file_path'),
        start_line := CAST(json_extract(node, '$.start.line') AS INTEGER),
        start_column := CAST(json_extract(node, '$.start.column') AS INTEGER),
        end_line := CAST(json_extract(node, '$.end.line') AS INTEGER),
        end_column := CAST(json_extract(node, '$.end.column') AS INTEGER)
    ) ORDER BY CAST(json_extract(node, '$.id') AS INTEGER))
    FROM json_each(nodes::JSON) AS t(node)
    ) as struct_nodes
FROM read_ast_objects('test/data/python/simple.py', 'python');

-- Helper macro for struct filtering
CREATE MACRO filter_struct_by_type(type_name) AS (
    SELECT COUNT(*) as count
    FROM ast_data,
         UNNEST(struct_nodes)
    WHERE unnest.type = type_name
);

.print "Struct setup complete. Test with: SELECT filter_struct_by_type('function_definition');"