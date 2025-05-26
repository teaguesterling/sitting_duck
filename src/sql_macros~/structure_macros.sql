-- Structure-Preserving Macros for AST
-- These return full AST objects with filtered nodes, preserving metadata

-- Filter by node type(s) - returns AST object
CREATE OR REPLACE MACRO ast_filter_type(ast_obj, types) AS (
    WITH filtered AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE list_contains(ensure_varchar_array(types), json_extract_string(je.value, '$.type'))
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(filtered.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(filtered.nodes) as n), 
            0
        ),
        nodes := COALESCE(filtered.nodes, '[]'::JSON)
    )
    FROM filtered
);

-- Filter by name pattern - returns AST object
CREATE OR REPLACE MACRO ast_filter_name(ast_obj, patterns, case_sensitive := false) AS (
    WITH filtered AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE EXISTS (
            SELECT 1 FROM unnest(ensure_varchar_array(patterns)) as pattern
            WHERE CASE 
                WHEN case_sensitive THEN
                    json_extract_string(je.value, '$.name') LIKE pattern
                ELSE
                    LOWER(json_extract_string(je.value, '$.name')) LIKE LOWER(pattern)
            END
        )
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(filtered.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(filtered.nodes) as n), 
            0
        ),
        nodes := COALESCE(filtered.nodes, '[]'::JSON)
    )
    FROM filtered
);

-- Filter by depth(s) - returns AST object
CREATE OR REPLACE MACRO ast_filter_depth(ast_obj, depths) AS (
    WITH depth_array AS (
        SELECT ensure_integer_array(depths) as depth_list
    ),
    filtered AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je, depth_array
        WHERE list_contains(depth_array.depth_list, json_extract(je.value, '$.depth')::INTEGER)
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(filtered.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(filtered.nodes) as n), 
            0
        ),
        nodes := COALESCE(filtered.nodes, '[]'::JSON)
    )
    FROM filtered
);

-- Filter by depth range - returns AST object
CREATE OR REPLACE MACRO ast_filter_depth_range(ast_obj, min_depth := 0, max_depth := NULL) AS (
    WITH filtered AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract(je.value, '$.depth')::INTEGER >= min_depth
          AND (max_depth IS NULL OR json_extract(je.value, '$.depth')::INTEGER <= max_depth)
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(filtered.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(filtered.nodes) as n), 
            0
        ),
        nodes := COALESCE(filtered.nodes, '[]'::JSON)
    )
    FROM filtered
);

-- Filter by line(s) - returns AST object
CREATE OR REPLACE MACRO ast_filter_line(ast_obj, lines, include_partial := true) AS (
    WITH line_array AS (
        SELECT ensure_integer_array(lines) as line_list
    ),
    filtered AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je, line_array
        WHERE EXISTS (
            SELECT 1 FROM unnest(line_array.line_list) as line
            WHERE 
                CASE 
                    WHEN include_partial THEN
                        line BETWEEN json_extract(je.value, '$.start.line')::INTEGER 
                                 AND json_extract(je.value, '$.end.line')::INTEGER
                    ELSE
                        json_extract(je.value, '$.start.line')::INTEGER = line
                        AND json_extract(je.value, '$.end.line')::INTEGER = line
                END
        )
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(filtered.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(filtered.nodes) as n), 
            0
        ),
        nodes := COALESCE(filtered.nodes, '[]'::JSON)
    )
    FROM filtered
);

-- Get descendants of a node - returns AST object
CREATE OR REPLACE MACRO ast_filter_descendants(ast_obj, node_id, max_levels := NULL) AS (
    WITH RECURSIVE descendants AS (
        -- Start with direct children
        SELECT 
            je.value as node,
            json_extract(je.value, '$.id')::BIGINT as id,
            1 as level
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract(je.value, '$.parent_id')::BIGINT = node_id
        
        UNION ALL
        
        -- Recursively find descendants
        SELECT 
            je.value as node,
            json_extract(je.value, '$.id')::BIGINT as id,
            d.level + 1 as level
        FROM descendants d
        JOIN json_each(ast_obj.nodes) AS je 
          ON json_extract(je.value, '$.parent_id')::BIGINT = d.id
        WHERE (max_levels IS NULL OR d.level < max_levels)
    ),
    filtered AS (
        SELECT json_group_array(node) as nodes
        FROM descendants
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(filtered.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(filtered.nodes) as n), 
            0
        ),
        nodes := COALESCE(filtered.nodes, '[]'::JSON)
    )
    FROM filtered
);

-- Safe variants that always return valid AST objects
CREATE OR REPLACE MACRO ast_safe_filter_type(ast_obj, types) AS (
    CASE 
        WHEN ast_obj IS NULL THEN 
            STRUCT_PACK(
                file_path := '',
                language := '',
                node_count := 0,
                max_depth := 0,
                nodes := '[]'::JSON
            )
        ELSE ast_filter_type(ast_obj, types)
    END
);