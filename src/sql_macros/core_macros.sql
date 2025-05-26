-- SQL Macros for AST querying with proper JSON type handling
-- These handle JSON type casting internally for clean user experience

-- Utility macro to ensure a value is a VARCHAR array
CREATE OR REPLACE MACRO ensure_varchar_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::VARCHAR[]
        WHEN typeof(val) = 'VARCHAR[]' THEN val
        ELSE [val]::VARCHAR[]
    END
);

-- Utility macro to ensure a value is an INTEGER array
CREATE OR REPLACE MACRO ensure_integer_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::INTEGER[]
        WHEN typeof(val) = 'INTEGER[]' THEN val::INTEGER[]
        WHEN typeof(val) = 'BIGINT[]' THEN val::INTEGER[]
        WHEN typeof(val) = 'INTEGER' THEN [val::INTEGER]::INTEGER[]
        WHEN typeof(val) = 'BIGINT' THEN [val::INTEGER]::INTEGER[]
        ELSE []::INTEGER[]
    END
);

-- Find all nodes of specific type(s) - accepts single value or array
CREATE OR REPLACE MACRO ast_find_type(nodes, types) AS (
    COALESCE(
        (SELECT json_group_array(je.value) 
         FROM json_each(nodes) AS je
         WHERE list_contains(ensure_varchar_array(types), json_extract_string(je.value, '$.type'))),
        '[]'::JSON
    )
);

-- Get all function names
CREATE OR REPLACE MACRO ast_function_names(nodes) AS (
    COALESCE(
        (SELECT json_group_array(json_extract_string(je.value, '$.name'))
         FROM json_each(nodes) AS je
         WHERE json_extract_string(je.value, '$.type') = 'function_definition'
           AND json_extract_string(je.value, '$.name') IS NOT NULL),
        '[]'::JSON
    )
);

-- Get all class names
CREATE OR REPLACE MACRO ast_class_names(nodes) AS (
    COALESCE(
        (SELECT json_group_array(json_extract_string(je.value, '$.name'))
         FROM json_each(nodes) AS je
         WHERE json_extract_string(je.value, '$.type') = 'class_definition'
           AND json_extract_string(je.value, '$.name') IS NOT NULL),
        '[]'::JSON
    )
);

-- Count nodes by type
CREATE OR REPLACE MACRO ast_type_counts(nodes) AS (
    (SELECT json_group_object(node_type, cnt)
     FROM (
         SELECT json_extract_string(je.value, '$.type') as node_type, COUNT(*) as cnt
         FROM json_each(nodes) AS je
         GROUP BY node_type
     ))
);

-- Get nodes at specific depth(s) - accepts single value or array
CREATE OR REPLACE MACRO ast_at_depth(nodes, depths) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(nodes) AS je
         WHERE list_contains(ensure_integer_array(depths), json_extract(je.value, '$.depth')::INTEGER)),
        '[]'::JSON
    )
);

-- Get direct children of a node by ID
CREATE OR REPLACE MACRO ast_children_of(nodes, parent_id) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(nodes) AS je
         WHERE json_extract(je.value, '$.parent_id')::INTEGER = parent_id),
        '[]'::JSON
    )
);

-- Get all identifiers (handles proper field name)
CREATE OR REPLACE MACRO ast_identifiers(nodes) AS (
    COALESCE(
        (SELECT json_group_array(json_extract_string(je.value, '$.name'))
         FROM json_each(nodes) AS je
         WHERE json_extract_string(je.value, '$.type') = 'identifier'
           AND json_extract_string(je.value, '$.name') IS NOT NULL),
        '[]'::JSON
    )
);

-- Get all string literals
CREATE OR REPLACE MACRO ast_strings(nodes) AS (
    COALESCE(
        (SELECT json_group_array(json_extract_string(je.value, '$.content'))
         FROM json_each(nodes) AS je
         WHERE json_extract_string(je.value, '$.type') = 'string'
           AND json_extract_string(je.value, '$.content') IS NOT NULL),
        '[]'::JSON
    )
);

-- Get function details with proper type casting for line numbers
CREATE OR REPLACE MACRO ast_function_details(nodes) AS (
    COALESCE(
        (SELECT json_group_array(
            json_object(
                'name', json_extract_string(je.value, '$.name'),
                'start_line', json_extract(je.value, '$.start.line')::INTEGER,
                'end_line', json_extract(je.value, '$.end.line')::INTEGER,
                'depth', json_extract(je.value, '$.depth')::INTEGER,
                'id', json_extract(je.value, '$.id')::INTEGER
            ))
         FROM json_each(nodes) AS je
         WHERE json_extract_string(je.value, '$.type') = 'function_definition'
           AND json_extract_string(je.value, '$.name') IS NOT NULL),
        '[]'::JSON
    )
);

-- Get a summary of the AST structure (handles all type casting internally)
CREATE OR REPLACE MACRO ast_summary(nodes) AS (
    json_object(
        'total_nodes', json_array_length(nodes),
        'node_types', ast_type_counts(nodes),
        'functions', ast_function_names(nodes),
        'classes', ast_class_names(nodes),
        'max_depth', (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
                      FROM json_each(nodes) AS je),
        'function_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                          WHERE json_extract_string(je.value, '$.type') = 'function_definition'),
        'class_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                       WHERE json_extract_string(je.value, '$.type') = 'class_definition')
    )
);

-- Handle empty results gracefully - return empty array instead of NULL
CREATE OR REPLACE MACRO ast_safe_find_type(nodes, node_type) AS (
    COALESCE(ast_find_type(nodes, node_type), '[]'::JSON)
);

-- Handle empty function lists gracefully
CREATE OR REPLACE MACRO ast_safe_function_names(nodes) AS (
    COALESCE(ast_function_names(nodes), '[]'::JSON)
);

-- Find nodes at specific line(s) - accepts single value or array
CREATE OR REPLACE MACRO ast_at_line(nodes, lines) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE EXISTS (
         SELECT 1 FROM unnest(ensure_integer_array(lines)) as line
         WHERE line BETWEEN json_extract(je.value, '$.start.line')::INTEGER 
                       AND json_extract(je.value, '$.end.line')::INTEGER
     ))
);

-- Get nodes in line number range
CREATE OR REPLACE MACRO ast_in_line_range(nodes, start_line, end_line) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE json_extract(je.value, '$.start.line')::INTEGER >= start_line
       AND json_extract(je.value, '$.end.line')::INTEGER <= end_line)
);

-- Extract all nodes with their essential info (proper type casting)
CREATE OR REPLACE MACRO ast_node_info(nodes) AS (
    (SELECT json_group_array(
        json_object(
            'id', json_extract(je.value, '$.id')::INTEGER,
            'type', json_extract_string(je.value, '$.type'),
            'name', json_extract_string(je.value, '$.name'),
            'start_line', json_extract(je.value, '$.start.line')::INTEGER,
            'end_line', json_extract(je.value, '$.end.line')::INTEGER,
            'depth', json_extract(je.value, '$.depth')::INTEGER
        ))
     FROM json_each(nodes) AS je)
);

-- Find nodes containing specific text (searches name, content fields)
CREATE OR REPLACE MACRO ast_contains_text(nodes, search_text) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE COALESCE(json_extract_string(je.value, '$.name'), '') LIKE '%' || search_text || '%'
        OR COALESCE(json_extract_string(je.value, '$.content'), '') LIKE '%' || search_text || '%')
);

-- Get complexity metrics (with proper numeric handling)
CREATE OR REPLACE MACRO ast_complexity(nodes) AS (
    json_object(
        'total_nodes', json_array_length(nodes),
        'avg_depth', (SELECT AVG(json_extract(je.value, '$.depth')::INTEGER) 
                      FROM json_each(nodes) AS je),
        'max_depth', (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
                      FROM json_each(nodes) AS je),
        'function_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                          WHERE json_extract_string(je.value, '$.type') = 'function_definition'),
        'lines_of_code', (SELECT MAX(json_extract(je.value, '$.end.line')::INTEGER) 
                         FROM json_each(nodes) AS je)
    )
);

-- ================================
-- Safe Variants (return empty instead of NULL)
-- ================================

CREATE OR REPLACE MACRO ast_safe_function_names(nodes) AS (
    COALESCE(ast_function_names(nodes), '[]'::JSON)
);

CREATE OR REPLACE MACRO ast_safe_class_names(nodes) AS (
    COALESCE(ast_class_names(nodes), '[]'::JSON)
);

CREATE OR REPLACE MACRO ast_safe_identifiers(nodes) AS (
    COALESCE(ast_identifiers(nodes), '[]'::JSON)
);

CREATE OR REPLACE MACRO ast_safe_strings(nodes) AS (
    COALESCE(ast_strings(nodes), '[]'::JSON)
);

-- ================================
-- Tree Navigation Macros
-- ================================

CREATE OR REPLACE MACRO ast_parent_of(nodes, child_id) AS (
    WITH child_node AS (
        SELECT json_extract(je.value, '$.parent_id')::BIGINT as parent_id
        FROM json_each(nodes) AS je
        WHERE json_extract(je.value, '$.id')::BIGINT = child_id
        LIMIT 1
    )
    SELECT json_group_array(je.value)
    FROM json_each(nodes) AS je, child_node
    WHERE json_extract(je.value, '$.id')::BIGINT = child_node.parent_id
);

CREATE OR REPLACE MACRO ast_siblings_of(nodes, node_id, include_self := false) AS (
    WITH node_info AS (
        SELECT json_extract(je.value, '$.parent_id')::BIGINT as parent_id
        FROM json_each(nodes) AS je
        WHERE json_extract(je.value, '$.id')::BIGINT = node_id
        LIMIT 1
    )
    SELECT json_group_array(je.value)
    FROM json_each(nodes) AS je, node_info
    WHERE json_extract(je.value, '$.parent_id')::BIGINT = node_info.parent_id
      AND (include_self OR json_extract(je.value, '$.id')::BIGINT != node_id)
);

CREATE OR REPLACE MACRO ast_ancestors_of(nodes, node_id, max_levels := NULL) AS (
    WITH RECURSIVE ancestors AS (
        -- Start with the parent of the given node
        SELECT 
            je.value as node,
            json_extract(je.value, '$.id')::BIGINT as id,
            json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
            1 as level
        FROM json_each(nodes) AS je
        WHERE json_extract(je.value, '$.id')::BIGINT = (
            SELECT json_extract(n.value, '$.parent_id')::BIGINT
            FROM json_each(nodes) AS n
            WHERE json_extract(n.value, '$.id')::BIGINT = node_id
            LIMIT 1
        )
        
        UNION ALL
        
        -- Recursively find ancestors
        SELECT 
            je.value as node,
            json_extract(je.value, '$.id')::BIGINT as id,
            json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
            a.level + 1 as level
        FROM ancestors a
        JOIN json_each(nodes) AS je 
          ON json_extract(je.value, '$.id')::BIGINT = a.parent_id
        WHERE a.parent_id IS NOT NULL
          AND (max_levels IS NULL OR a.level < max_levels)
    )
    SELECT json_group_array(node)
    FROM ancestors
);

CREATE OR REPLACE MACRO ast_descendants_of(nodes, node_id, max_levels := NULL) AS (
    WITH RECURSIVE descendants AS (
        -- Start with direct children
        SELECT 
            je.value as node,
            json_extract(je.value, '$.id')::BIGINT as id,
            json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
            1 as level
        FROM json_each(nodes) AS je
        WHERE json_extract(je.value, '$.parent_id')::BIGINT = node_id
        
        UNION ALL
        
        -- Recursively find descendants
        SELECT 
            je.value as node,
            json_extract(je.value, '$.id')::BIGINT as id,
            json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
            d.level + 1 as level
        FROM descendants d
        JOIN json_each(nodes) AS je 
          ON json_extract(je.value, '$.parent_id')::BIGINT = d.id
        WHERE (max_levels IS NULL OR d.level < max_levels)
    )
    SELECT json_group_array(node)
    FROM descendants
);

-- ================================
-- Enhanced Filtering with Pattern Support
-- ================================

CREATE OR REPLACE MACRO ast_filter_name_pattern(nodes, pattern, case_sensitive := false) AS (
    SELECT json_group_array(je.value)
    FROM json_each(nodes) AS je
    WHERE CASE 
        WHEN case_sensitive THEN
            json_extract_string(je.value, '$.name') LIKE pattern
        ELSE
            LOWER(json_extract_string(je.value, '$.name')) LIKE LOWER(pattern)
    END
);

-- Get nodes at depth range
CREATE OR REPLACE MACRO ast_at_depth_range(nodes, min_depth := 0, max_depth := NULL) AS (
    SELECT json_group_array(je.value)
    FROM json_each(nodes) AS je
    WHERE json_extract(je.value, '$.depth')::INTEGER >= min_depth
      AND (max_depth IS NULL OR json_extract(je.value, '$.depth')::INTEGER <= max_depth)
);