-- Core AST API
-- Minimal, focused set of functions for AST querying

-- ===================================
-- INTERNAL HELPERS (not part of public API)
-- ===================================

-- Internal: Ensure a value is a VARCHAR array
CREATE OR REPLACE MACRO _ast_internal_ensure_varchar_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::VARCHAR[]
        WHEN typeof(val) = 'VARCHAR[]' THEN val
        ELSE [val]::VARCHAR[]
    END
);

-- Internal: Ensure a value is an INTEGER array  
CREATE OR REPLACE MACRO _ast_internal_ensure_integer_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::INTEGER[]
        WHEN typeof(val) = 'INTEGER[]' THEN val::INTEGER[]
        WHEN typeof(val) = 'BIGINT[]' THEN val::INTEGER[]
        WHEN typeof(val) = '"NULL"[]' THEN val::INTEGER[]
        WHEN typeof(val) = 'INTEGER' THEN [val::INTEGER]::INTEGER[]
        WHEN typeof(val) = 'BIGINT' THEN [val::INTEGER]::INTEGER[]
        ELSE []::INTEGER[]
    END
);

-- ===================================
-- CORE EXTRACTION FUNCTIONS
-- ===================================

-- Find nodes by type(s) - accepts string or array
CREATE OR REPLACE MACRO ast_get_type(nodes, types) AS (
    [
        node
        for node in COALESCE(nodes, [])
        if list_contains(_ast_internal_ensure_varchar_array(types), node.type)
    ]
);

-- Extract names from nodes, optionally filtered by type
CREATE OR REPLACE MACRO ast_get_names(nodes, node_type := NULL) AS (
    [
        node.name
        for node in COALESCE(nodes, [])
        if node.name IS NOT NULL AND node.name != ''
           AND (node_type IS NULL OR node.type = node_type)
    ]
);

-- Find nodes at specific depth(s) - accepts integer or array
CREATE OR REPLACE MACRO ast_get_depth(nodes, depths) AS (
    [
        node
        for node in COALESCE(nodes, [])
        if list_contains(_ast_internal_ensure_integer_array(depths), node.depth)
    ]
);

-- ===================================
-- FILTERING FUNCTIONS
-- ===================================

-- Filter nodes by name pattern (SQL LIKE pattern)
CREATE OR REPLACE MACRO ast_filter_pattern(nodes, pattern, field := 'name') AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.' || COALESCE(field, 'name')) LIKE pattern),
        '[]'::JSON
    )
);

-- Filter to nodes that have a name field
CREATE OR REPLACE MACRO ast_filter_has_name(nodes) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.name') IS NOT NULL),
        '[]'::JSON
    )
);

-- ===================================
-- NAVIGATION FUNCTIONS  
-- ===================================

-- Get direct children of a node
CREATE OR REPLACE MACRO ast_nav_children(nodes, parent_id) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract(je.value, '$.parent_id') = parent_id),
        '[]'::JSON
    )
);

-- Get parent of a node
CREATE OR REPLACE MACRO ast_nav_parent(nodes, child_id) AS (
    COALESCE(
        (SELECT json_group_array(parent.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS child,
              json_each(COALESCE(nodes, '[]'::JSON)) AS parent
         WHERE json_extract(child.value, '$.id') = child_id
           AND json_extract(parent.value, '$.id') = json_extract(child.value, '$.parent_id')
         LIMIT 1),
        '[]'::JSON
    )
);

-- ===================================
-- ANALYSIS FUNCTIONS
-- ===================================

-- Get summary statistics about the AST
CREATE OR REPLACE MACRO ast_summary(nodes) AS (
    json_object(
        'total_nodes', json_array_length(COALESCE(nodes, '[]'::JSON)),
        'types', (SELECT json_group_object(node_type, cnt) FROM (
            SELECT json_extract_string(je.value, '$.type') as node_type, COUNT(*) as cnt
            FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
            GROUP BY node_type
        )),
        'max_depth', COALESCE((SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
                               FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je), 0),
        'functions', json_array_length(ast_get_names(nodes, node_type:='function_definition')),
        'classes', json_array_length(ast_get_names(nodes, node_type:='class_definition'))
    )
);