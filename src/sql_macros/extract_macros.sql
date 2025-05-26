-- Extraction Macros for AST
-- These return proper SQL types (VARCHAR[], TABLE, etc.) instead of JSON

-- Extract names as VARCHAR[] instead of JSON
CREATE OR REPLACE MACRO ast_extract_names(ast_obj, types := NULL, pattern := NULL) AS (
    SELECT LIST(DISTINCT name ORDER BY name)
    FROM (
        SELECT json_extract_string(je.value, '$.name') as name
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.name') IS NOT NULL
          AND (types IS NULL OR 
               list_contains(
                   ensure_varchar_array(types),
                   json_extract_string(je.value, '$.type')
               ))
          AND (pattern IS NULL OR
               json_extract_string(je.value, '$.name') LIKE pattern)
    )
);

-- Extract function names as VARCHAR[]
CREATE OR REPLACE MACRO ast_extract_function_names(ast_obj) AS (
    ast_extract_names(ast_obj, types := 'function_definition')
);

-- Extract class names as VARCHAR[]
CREATE OR REPLACE MACRO ast_extract_class_names(ast_obj) AS (
    ast_extract_names(ast_obj, types := 'class_definition')
);

-- Extract all identifiers as VARCHAR[]
CREATE OR REPLACE MACRO ast_extract_identifiers(ast_obj) AS (
    ast_extract_names(ast_obj, types := 'identifier')
);

-- Extract entities as a table
CREATE OR REPLACE MACRO ast_extract_entities(
    ast_obj, 
    types := NULL,
    include_anonymous := false
) AS (
    SELECT 
        ast_obj.file_path as file_path,
        json_extract(je.value, '$.start.line')::INTEGER as start_line,
        json_extract(je.value, '$.end.line')::INTEGER as end_line,
        json_extract_string(je.value, '$.type') as entity_type,
        json_extract_string(je.value, '$.name') as entity_name,
        json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
        json_extract(je.value, '$.id')::BIGINT as node_id,
        json_extract(je.value, '$.depth')::INTEGER as depth,
        json_extract(je.value, '$.end.line')::INTEGER - 
            json_extract(je.value, '$.start.line')::INTEGER + 1 as line_count
    FROM json_each(ast_obj.nodes) AS je
    WHERE (types IS NULL OR 
           list_contains(
               ensure_varchar_array(types),
               json_extract_string(je.value, '$.type')
           ))
      AND (include_anonymous OR json_extract_string(je.value, '$.name') IS NOT NULL)
);

-- Extract summary as a structured record
CREATE OR REPLACE MACRO ast_extract_summary(ast_obj) AS (
    WITH stats AS (
        SELECT 
            COUNT(*) as total_nodes,
            COUNT(DISTINCT json_extract_string(je.value, '$.type')) as unique_types,
            MAX(json_extract(je.value, '$.depth')::INTEGER) as max_depth,
            AVG(json_extract(je.value, '$.depth')::INTEGER) as avg_depth,
            MAX(json_extract(je.value, '$.end.line')::INTEGER) as lines_of_code,
            COUNT(*) FILTER (WHERE json_extract_string(je.value, '$.type') = 'function_definition') as function_count,
            COUNT(*) FILTER (WHERE json_extract_string(je.value, '$.type') = 'class_definition') as class_count,
            COUNT(*) FILTER (WHERE json_extract_string(je.value, '$.type') = 'import_statement') as import_count
        FROM json_each(ast_obj.nodes) AS je
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        total_nodes := stats.total_nodes,
        unique_types := stats.unique_types,
        max_depth := COALESCE(stats.max_depth, 0),
        avg_depth := ROUND(COALESCE(stats.avg_depth, 0), 2),
        lines_of_code := COALESCE(stats.lines_of_code, 0),
        function_count := stats.function_count,
        class_count := stats.class_count,
        import_count := stats.import_count
    )
    FROM stats
);

-- Extract type counts as a MAP
CREATE OR REPLACE MACRO ast_extract_type_counts(ast_obj) AS (
    SELECT MAP(
        LIST(node_type ORDER BY node_type),
        LIST(cnt ORDER BY node_type)
    ) as type_counts
    FROM (
        SELECT 
            json_extract_string(je.value, '$.type') as node_type,
            COUNT(*) as cnt
        FROM json_each(ast_obj.nodes) AS je
        GROUP BY node_type
    )
);

-- Extract function details as a table
CREATE OR REPLACE MACRO ast_extract_functions(ast_obj, include_methods := true) AS (
    WITH func_types AS (
        SELECT unnest(
            CASE 
                WHEN include_methods THEN ['function_definition', 'method_definition']
                ELSE ['function_definition']
            END
        ) as func_type
    )
    SELECT 
        ast_obj.file_path,
        json_extract_string(je.value, '$.name') as name,
        json_extract(je.value, '$.start.line')::INTEGER as start_line,
        json_extract(je.value, '$.end.line')::INTEGER as end_line,
        json_extract(je.value, '$.end.line')::INTEGER - 
            json_extract(je.value, '$.start.line')::INTEGER + 1 as line_count,
        json_extract(je.value, '$.depth')::INTEGER as depth,
        json_extract_string(je.value, '$.type') as node_type,
        json_extract(je.value, '$.id')::BIGINT as node_id,
        json_extract(je.value, '$.parent_id')::BIGINT as parent_id
    FROM json_each(ast_obj.nodes) AS je, func_types
    WHERE json_extract_string(je.value, '$.type') = func_types.func_type
      AND json_extract_string(je.value, '$.name') IS NOT NULL
);

-- Extract nodes as a flat table (for SQL analysis)
CREATE OR REPLACE MACRO ast_extract_nodes(ast_obj) AS (
    SELECT 
        json_extract(je.value, '$.id')::BIGINT as node_id,
        json_extract_string(je.value, '$.type') as node_type,
        json_extract_string(je.value, '$.name') as node_name,
        json_extract(je.value, '$.start.line')::INTEGER as start_line,
        json_extract(je.value, '$.start.column')::INTEGER as start_column,
        json_extract(je.value, '$.end.line')::INTEGER as end_line,
        json_extract(je.value, '$.end.column')::INTEGER as end_column,
        json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
        json_extract(je.value, '$.depth')::INTEGER as depth,
        json_extract(je.value, '$.sibling_index')::INTEGER as sibling_index,
        ast_obj.file_path,
        ast_obj.language
    FROM json_each(ast_obj.nodes) AS je
);

-- Safe extraction variants
CREATE OR REPLACE MACRO ast_safe_extract_names(ast_obj, types := NULL) AS (
    COALESCE(ast_extract_names(ast_obj, types), [])
);

CREATE OR REPLACE MACRO ast_safe_extract_function_names(ast_obj) AS (
    COALESCE(ast_extract_function_names(ast_obj), [])
);

CREATE OR REPLACE MACRO ast_safe_extract_class_names(ast_obj) AS (
    COALESCE(ast_extract_class_names(ast_obj), [])
);