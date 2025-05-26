-- Clean API Core Functions
-- Implementation of the simplified, consistent AST API

-- ===================================
-- 1. ENTRYPOINT & UTILITIES  
-- ===================================

-- ast() function already exists in entrypoint_macros.sql - reuse as-is

-- Interactive help system
CREATE OR REPLACE MACRO ast_help() AS (
    json_object(
        'getting_started', json_array(
            'SELECT ast_get_type(nodes, ''function_definition'') FROM read_ast_objects(''file.py'', ''python'')',
            'SELECT ast_get_names(nodes) FROM read_ast_objects(''file.py'', ''python'')',
            'SELECT ast(nodes).get_type(''function_definition'').count() FROM read_ast_objects(''file.py'', ''python'')'
        ),
        'categories', json_object(
            'ast_get_*', 'Simple extraction returning JSON arrays',
            'ast_filter_*', 'Filtering operations returning filtered JSON arrays', 
            'ast_nav_*', 'Tree navigation (parent, children, siblings, etc.)',
            'ast_analyze_*', 'Analysis and metrics',
            'ast_source_*', 'Source code extraction (reads from files)'
        ),
        'core_functions', json_array(
            'ast_get_type(nodes, types) - Find nodes by type(s)',
            'ast_get_names(nodes, node_type?) - Extract names, optionally by type',
            'ast_get_depth(nodes, depths) - Find nodes at depth(s)',
            'ast_get_line(nodes, lines) - Find nodes at line(s)',
            'ast_nav_children(nodes, parent_id) - Get direct children',
            'ast_analyze_summary(nodes) - Overall statistics'
        ),
        'examples_url', 'Use ast_examples() for more detailed examples'
    )
);

-- ===================================
-- 2. BASIC EXTRACTION (ast_get_*)
-- ===================================

-- Find nodes by type(s) - accepts single type or array of types
CREATE OR REPLACE MACRO ast_get_type(nodes, types) AS (
    COALESCE(
        (SELECT json_group_array(je.value) 
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE list_contains(ensure_varchar_array(types), json_extract_string(je.value, '$.type'))),
        '[]'::JSON
    )
);

-- Extract names, optionally filtered by node type
CREATE OR REPLACE MACRO ast_get_names(nodes, node_type := NULL) AS (
    COALESCE(
        (SELECT json_group_array(json_extract_string(je.value, '$.name'))
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.name') IS NOT NULL
           AND (node_type IS NULL OR json_extract_string(je.value, '$.type') = node_type)),
        '[]'::JSON
    )
);

-- Find nodes at specific depth(s) - accepts single depth or array of depths  
CREATE OR REPLACE MACRO ast_get_depth(nodes, depths) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE list_contains(ensure_integer_array(depths), json_extract(je.value, '$.depth')::INTEGER)),
        '[]'::JSON
    )
);

-- Find nodes at specific line(s) - accepts single line or array of lines
CREATE OR REPLACE MACRO ast_get_line(nodes, lines) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE EXISTS (
             SELECT 1 FROM unnest(ensure_integer_array(lines)) as line
             WHERE line BETWEEN json_extract(je.value, '$.start.line')::INTEGER 
                           AND json_extract(je.value, '$.end.line')::INTEGER
         )),
        '[]'::JSON
    )
);

-- Find nodes in line range
CREATE OR REPLACE MACRO ast_get_range(nodes, start_line, end_line) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract(je.value, '$.start.line')::INTEGER >= start_line
           AND json_extract(je.value, '$.end.line')::INTEGER <= end_line),
        '[]'::JSON
    )
);

-- ===================================
-- 3. FILTERING (ast_filter_*)
-- ===================================

-- Filter by pattern using LIKE (field defaults to 'name')
CREATE OR REPLACE MACRO ast_filter_pattern(nodes, pattern, field := 'name') AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.' || field) LIKE pattern),
        '[]'::JSON
    )
);

-- Filter by depth range
CREATE OR REPLACE MACRO ast_filter_depth_range(nodes, min_depth := 0, max_depth := NULL) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract(je.value, '$.depth')::INTEGER >= min_depth
           AND (max_depth IS NULL OR json_extract(je.value, '$.depth')::INTEGER <= max_depth)),
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
-- 4. TREE NAVIGATION (ast_nav_*)
-- ===================================

-- Get direct children of a node by parent ID
CREATE OR REPLACE MACRO ast_nav_children(nodes, parent_id) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract(je.value, '$.parent_id')::INTEGER = parent_id),
        '[]'::JSON
    )
);

-- Get parent node by child ID
CREATE OR REPLACE MACRO ast_nav_parent(nodes, child_id) AS (
    COALESCE(
        (WITH child_node AS (
            SELECT json_extract(je.value, '$.parent_id')::BIGINT as parent_id
            FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
            WHERE json_extract(je.value, '$.id')::BIGINT = child_id
            LIMIT 1
        )
        SELECT json_group_array(je.value)
        FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je, child_node
        WHERE json_extract(je.value, '$.id')::BIGINT = child_node.parent_id),
        '[]'::JSON
    )
);

-- Get sibling nodes (excluding the node itself by default)
CREATE OR REPLACE MACRO ast_nav_siblings(nodes, node_id, include_self := false) AS (
    COALESCE(
        (WITH node_info AS (
            SELECT json_extract(je.value, '$.parent_id')::BIGINT as parent_id
            FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
            WHERE json_extract(je.value, '$.id')::BIGINT = node_id
            LIMIT 1
        )
        SELECT json_group_array(je.value)
        FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je, node_info
        WHERE json_extract(je.value, '$.parent_id')::BIGINT = node_info.parent_id
          AND (include_self OR json_extract(je.value, '$.id')::BIGINT != node_id)),
        '[]'::JSON
    )
);

-- Get ancestor nodes with optional level limit
CREATE OR REPLACE MACRO ast_nav_ancestors(nodes, node_id, max_levels := NULL) AS (
    COALESCE(
        (WITH RECURSIVE ancestors AS (
            -- Start with the parent of the given node
            SELECT 
                je.value as node,
                json_extract(je.value, '$.id')::BIGINT as id,
                json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
                1 as level
            FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
            WHERE json_extract(je.value, '$.id')::BIGINT = (
                SELECT json_extract(n.value, '$.parent_id')::BIGINT
                FROM json_each(COALESCE(nodes, '[]'::JSON)) AS n
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
            JOIN json_each(COALESCE(nodes, '[]'::JSON)) AS je 
              ON json_extract(je.value, '$.id')::BIGINT = a.parent_id
            WHERE a.parent_id IS NOT NULL
              AND (max_levels IS NULL OR a.level < max_levels)
        )
        SELECT json_group_array(node)
        FROM ancestors),
        '[]'::JSON
    )
);

-- Get descendant nodes with optional level limit
CREATE OR REPLACE MACRO ast_nav_descendants(nodes, node_id, max_levels := NULL) AS (
    COALESCE(
        (WITH RECURSIVE descendants AS (
            -- Start with direct children
            SELECT 
                je.value as node,
                json_extract(je.value, '$.id')::BIGINT as id,
                json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
                1 as level
            FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
            WHERE json_extract(je.value, '$.parent_id')::BIGINT = node_id
            
            UNION ALL
            
            -- Recursively find descendants
            SELECT 
                je.value as node,
                json_extract(je.value, '$.id')::BIGINT as id,
                json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
                d.level + 1 as level
            FROM descendants d
            JOIN json_each(COALESCE(nodes, '[]'::JSON)) AS je 
              ON json_extract(je.value, '$.parent_id')::BIGINT = d.id
            WHERE (max_levels IS NULL OR d.level < max_levels)
        )
        SELECT json_group_array(node)
        FROM descendants),
        '[]'::JSON
    )
);