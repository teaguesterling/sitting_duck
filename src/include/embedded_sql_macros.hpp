// Auto-generated file - DO NOT EDIT
// Generated from SQL macro files in src/sql_macros/
// Run: python scripts/embed_sql_macros.py

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace duckdb {

// SQL macro definitions embedded at compile time
static const std::vector<std::pair<std::string, std::string>> EMBEDDED_SQL_MACROS = {
    {"core_macros.sql", R"SQLMACRO(
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
)SQLMACRO"},
    {"entrypoint_macros.sql", R"SQLMACRO(
-- AST Entrypoint Macro
-- Provides a single starting point for all AST operations with type normalization

-- Main entrypoint that normalizes different input types to a standard nodes array
CREATE OR REPLACE MACRO ast(input) AS (
    CASE 
        -- Input is NULL
        WHEN input IS NULL THEN 
            '[]'::JSON
            
        -- Input is already a JSON array (raw nodes)
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'ARRAY' THEN 
            input
            
        -- Input is a JSON object (single node), wrap in array
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'OBJECT' THEN 
            json_array(input)
            
        -- Input is a struct that might have a 'nodes' field
        WHEN typeof(input) = 'STRUCT' THEN
            COALESCE(
                TRY_CAST(json_extract(input::JSON, '$.nodes') AS JSON),
                '[]'::JSON
            )
            
        -- Input is a VARCHAR, try to parse as JSON
        WHEN typeof(input) = 'VARCHAR' THEN 
            CASE 
                WHEN TRY_CAST(input AS JSON) IS NULL THEN '[]'::JSON
                WHEN json_type(TRY_CAST(input AS JSON)) = 'ARRAY' THEN TRY_CAST(input AS JSON)
                WHEN json_type(TRY_CAST(input AS JSON)) = 'OBJECT' THEN json_array(TRY_CAST(input AS JSON))
                ELSE '[]'::JSON
            END
            
        -- Fallback: empty array
        ELSE '[]'::JSON
    END
);

-- Method-style macros that work with the ast() entrypoint
-- These provide the chainable interface

CREATE OR REPLACE MACRO find_type(nodes, types) AS (
    ast_find_type(nodes, types)
);

CREATE OR REPLACE MACRO function_names(nodes) AS (
    ast_function_names(nodes)
);

CREATE OR REPLACE MACRO class_names(nodes) AS (
    ast_class_names(nodes)
);

CREATE OR REPLACE MACRO identifiers(nodes) AS (
    ast_identifiers(nodes)
);

CREATE OR REPLACE MACRO at_depth(nodes, depths) AS (
    ast_at_depth(nodes, depths)
);

CREATE OR REPLACE MACRO children_of(nodes, parent_id) AS (
    ast_children_of(nodes, parent_id)
);

CREATE OR REPLACE MACRO summary(nodes) AS (
    ast_summary(nodes)
);

CREATE OR REPLACE MACRO function_details(nodes) AS (
    ast_function_details(nodes)
);

CREATE OR REPLACE MACRO type_counts(nodes) AS (
    ast_type_counts(nodes)
);

CREATE OR REPLACE MACRO strings(nodes) AS (
    ast_strings(nodes)
);

-- Utility methods for working with arrays/results
CREATE OR REPLACE MACRO count_elements(arr) AS (
    json_array_length(COALESCE(arr, '[]'::JSON))
);

CREATE OR REPLACE MACRO first_element(arr) AS (
    json_extract(COALESCE(arr, '[]'::JSON), '$[0]')
);

CREATE OR REPLACE MACRO last_element(arr) AS (
    json_extract(
        COALESCE(arr, '[]'::JSON), 
        '$[' || (json_array_length(COALESCE(arr, '[]'::JSON)) - 1) || ']'
    )
);

CREATE OR REPLACE MACRO extract_names(nodes) AS (
    COALESCE(
        (SELECT json_group_array(json_extract_string(je.value, '$.name'))
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.name') IS NOT NULL),
        '[]'::JSON
    )
);

-- Advanced chaining helpers
CREATE OR REPLACE MACRO where_type(nodes, type_name) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.type') = type_name),
        '[]'::JSON
    )
);

CREATE OR REPLACE MACRO where_depth(nodes, target_depth) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract(je.value, '$.depth')::INTEGER = target_depth),
        '[]'::JSON
    )
);
)SQLMACRO"},
    {"structure_macros.sql", R"SQLMACRO(
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
)SQLMACRO"},
    {"extract_macros.sql", R"SQLMACRO(
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
)SQLMACRO"},
    {"source_macros.sql", R"SQLMACRO(
-- SQL Macros for Source Code Integration - Final Version
-- Using list comprehension approach for efficient line numbering

-- Read file lines with line numbers (using list comprehension)
CREATE OR REPLACE MACRO read_file_lines(file_path, start_line := 1, end_line := 2147483647) AS (
    SELECT 
        filename,
        size AS filesize,
        last_modified AS file_last_modified,
        line_info.lineno AS line_number,
        line_info.content AS line,
        length(line_info.content) AS line_length
    FROM (
        SELECT 
            filename,
            size,
            last_modified,
            unnest([{
                'lineno': i, 
                'content': lines[i]
            } for i in generate_series(1, array_length(lines))]) AS line_info
        FROM (
            SELECT *, string_split(content, E'\n') AS lines
            FROM read_text(file_path)
        )
    )
    WHERE line_info.lineno BETWEEN start_line AND end_line
    ORDER BY line_info.lineno
);

-- Get source text with optional line numbers and formatting
CREATE OR REPLACE MACRO get_source_text(
    file_path, 
    start_line, 
    end_line, 
    include_line_numbers := true,
    line_format := '{:4d}: {}'
) AS (
    SELECT string_agg(
        CASE 
            WHEN include_line_numbers 
            THEN format(line_format, line_number, line)
            ELSE line
        END,
        E'\n'
        ORDER BY line_number
    ) AS source_text
    FROM read_file_lines(file_path, start_line, end_line)
);

-- Extract source code for AST nodes with context
CREATE OR REPLACE MACRO ast_extract_source(
    ast_obj,
    node_ids := NULL,
    pad_lines := 0,
    min_lines := 0,
    include_line_numbers := true,
    highlight_range := false
) AS (
    SELECT 
        ast_obj.file_path,
        json_extract(je.value, '$.id')::BIGINT AS node_id,
        json_extract_string(je.value, '$.type') AS node_type,
        json_extract_string(je.value, '$.name') AS node_name,
        json_extract(je.value, '$.start.line')::INTEGER AS start_line,
        json_extract(je.value, '$.end.line')::INTEGER AS end_line,
        json_extract(je.value, '$.start.column')::INTEGER AS start_column,
        json_extract(je.value, '$.end.column')::INTEGER AS end_column,
        json_extract(je.value, '$.end.line')::INTEGER - 
            json_extract(je.value, '$.start.line')::INTEGER + 1 AS line_count,
        GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines) AS context_start,
        json_extract(je.value, '$.end.line')::INTEGER + pad_lines AS context_end,
        -- Original source without padding
        get_source_text(
            ast_obj.file_path,
            json_extract(je.value, '$.start.line')::INTEGER,
            json_extract(je.value, '$.end.line')::INTEGER,
            include_line_numbers
        ) AS source_text,
        -- Source with context padding
        CASE 
            WHEN highlight_range THEN
                -- Add visual markers for the actual node range
                CONCAT(
                    CASE WHEN pad_lines > 0 THEN
                        get_source_text(
                            ast_obj.file_path,
                            GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines),
                            json_extract(je.value, '$.start.line')::INTEGER - 1,
                            include_line_numbers
                        ) || E'\n' || REPEAT('-', 40) || ' [START] ' || REPEAT('-', 40) || E'\n'
                    ELSE '' END,
                    get_source_text(
                        ast_obj.file_path,
                        json_extract(je.value, '$.start.line')::INTEGER,
                        json_extract(je.value, '$.end.line')::INTEGER,
                        include_line_numbers
                    ),
                    CASE WHEN pad_lines > 0 THEN
                        E'\n' || REPEAT('-', 40) || ' [END] ' || REPEAT('-', 40) || E'\n' ||
                        get_source_text(
                            ast_obj.file_path,
                            json_extract(je.value, '$.end.line')::INTEGER + 1,
                            json_extract(je.value, '$.end.line')::INTEGER + pad_lines,
                            include_line_numbers
                        )
                    ELSE '' END
                )
            ELSE
                get_source_text(
                    ast_obj.file_path,
                    GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines),
                    json_extract(je.value, '$.end.line')::INTEGER + pad_lines,
                    include_line_numbers
                )
        END AS source_with_context
    FROM json_each(ast_obj.nodes) AS je
    WHERE 
        -- Filter by line count if specified
        (min_lines = 0 OR 
         json_extract(je.value, '$.end.line')::INTEGER - 
         json_extract(je.value, '$.start.line')::INTEGER + 1 >= min_lines)
        -- Filter by node IDs if specified
        AND (node_ids IS NULL OR 
             CASE 
                WHEN pg_typeof(node_ids) = 'BIGINT[]' THEN
                    list_contains(node_ids, json_extract(je.value, '$.id')::BIGINT)
                WHEN pg_typeof(node_ids) = 'BIGINT' THEN
                    json_extract(je.value, '$.id')::BIGINT = node_ids
                ELSE false
             END)
);

-- Get file statistics with code metrics
CREATE OR REPLACE MACRO ast_file_stats(file_path) AS (
    WITH file_metrics AS (
        SELECT 
            filename,
            filesize,
            file_last_modified,
            COUNT(*) AS total_lines,
            COUNT(*) FILTER (WHERE trim(line) = '') AS empty_lines,
            COUNT(*) FILTER (WHERE line LIKE '%TODO%' OR line LIKE '%FIXME%') AS todo_lines,
            COUNT(*) FILTER (WHERE trim(line) LIKE '#%' OR trim(line) LIKE '//%') AS comment_lines,
            MAX(line_length) AS max_line_length,
            AVG(line_length)::INTEGER AS avg_line_length,
            SUM(line_length) AS total_chars
        FROM read_file_lines(file_path)
        GROUP BY filename, filesize, file_last_modified
    )
    SELECT 
        filename,
        filesize,
        file_last_modified,
        total_lines,
        total_lines - empty_lines AS non_empty_lines,
        empty_lines,
        comment_lines,
        todo_lines,
        ROUND(100.0 * empty_lines / total_lines, 2) AS empty_line_pct,
        ROUND(100.0 * comment_lines / total_lines, 2) AS comment_line_pct,
        max_line_length,
        avg_line_length,
        total_chars
    FROM file_metrics
);

-- Find and extract TODO/FIXME comments with context
CREATE OR REPLACE MACRO ast_extract_todos(ast_obj, context_lines := 2) AS (
    WITH todo_lines AS (
        SELECT 
            line_number,
            line,
            line LIKE '%TODO%' AS is_todo,
            line LIKE '%FIXME%' AS is_fixme
        FROM read_file_lines(ast_obj.file_path)
        WHERE line LIKE '%TODO%' OR line LIKE '%FIXME%'
    )
    SELECT 
        ast_obj.file_path,
        tl.line_number,
        CASE 
            WHEN tl.is_todo AND tl.is_fixme THEN 'TODO+FIXME'
            WHEN tl.is_todo THEN 'TODO'
            ELSE 'FIXME'
        END AS todo_type,
        trim(tl.line) AS todo_text,
        get_source_text(
            ast_obj.file_path,
            GREATEST(1, tl.line_number - context_lines),
            tl.line_number + context_lines,
            true
        ) AS context
    FROM todo_lines tl
    ORDER BY tl.line_number
);

-- Build code review table with source snippets
CREATE OR REPLACE MACRO ast_code_review_table(
    ast_obj,
    complexity_threshold := 10,
    length_threshold := 50
) AS (
    WITH complex_entities AS (
        SELECT *
        FROM ast_extract_entities(
            ast_obj,
            types := ['function_definition', 'class_definition', 'method_definition']
        )
        WHERE (end_line - start_line + 1) > length_threshold
    ),
    entity_metrics AS (
        SELECT 
            ce.*,
            ce.end_line - ce.start_line + 1 AS line_count,
            -- Calculate cyclomatic complexity estimate (simplified)
            (SELECT COUNT(*) 
             FROM json_each(ast_obj.nodes) AS je
             WHERE json_extract_string(je.value, '$.type') IN 
                   ('if_statement', 'while_statement', 'for_statement', 'except_clause')
               AND json_extract(je.value, '$.start.line')::INTEGER >= ce.start_line
               AND json_extract(je.value, '$.end.line')::INTEGER <= ce.end_line
            ) + 1 AS estimated_complexity
        FROM complex_entities ce
    )
    SELECT 
        em.*,
        src.source_with_context,
        fs.todo_lines,
        CASE 
            WHEN em.estimated_complexity > complexity_threshold THEN 'HIGH'
            WHEN em.estimated_complexity > complexity_threshold / 2 THEN 'MEDIUM'
            ELSE 'LOW'
        END AS complexity_rating
    FROM entity_metrics em
    JOIN ast_extract_source(
        ast_obj,
        node_ids := LIST(em.node_id),
        pad_lines := 3,
        highlight_range := true
    ) src ON em.node_id = src.node_id
    CROSS JOIN ast_file_stats(em.file_path) fs
    ORDER BY em.estimated_complexity DESC, em.line_count DESC
);

-- Example usage comments:
-- 
-- 1. Get source for a specific function:
-- SELECT * FROM read_ast_objects('app.py', 'python') AS ast,
--      TABLE(ast_extract_source(
--          ast_filter_type(ast, 'function_definition'),
--          pad_lines := 5
--      )) AS src
-- WHERE src.node_name = 'process_data';
--
-- 2. Find complex functions for review:
-- SELECT * FROM read_ast_objects('**/*.py', 'python') AS ast,
--      TABLE(ast_code_review_table(ast, complexity_threshold := 10))
-- WHERE complexity_rating = 'HIGH';

)SQLMACRO"},
    {"ai_macros.sql", R"SQLMACRO(
-- AI-Friendly Macros and Shortcuts
-- These provide semantic operations and discovery functions

-- Discover available node types in AST
CREATE OR REPLACE MACRO ast_available_types(ast_obj) AS (
    SELECT LIST(DISTINCT json_extract_string(je.value, '$.type') ORDER BY 1)
    FROM json_each(ast_obj.nodes) AS je
);

-- Find test functions (common patterns)
CREATE OR REPLACE MACRO ast_test_functions(
    ast_obj, 
    patterns := ['test_%', '%_test', 'Test%', '%Test']
) AS (
    WITH filtered_types AS (
        SELECT ast_filter_type(ast_obj, ['function_definition', 'method_definition']) as filtered
    ),
    pattern_array AS (
        SELECT ensure_varchar_array(patterns) as pattern_list
    ),
    test_nodes AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(filtered_types.filtered.nodes) AS je, pattern_array
        WHERE EXISTS (
            SELECT 1 FROM unnest(pattern_array.pattern_list) as pattern
            WHERE LOWER(json_extract_string(je.value, '$.name')) LIKE LOWER(pattern)
        )
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(test_nodes.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(test_nodes.nodes) as n), 
            0
        ),
        nodes := COALESCE(test_nodes.nodes, '[]'::JSON)
    )
    FROM filtered_types, test_nodes
);

-- Find public API (top-level non-private entities)
CREATE OR REPLACE MACRO ast_public_api(ast_obj, max_depth := 2) AS (
    WITH filtered AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.type') IN ('function_definition', 'class_definition')
          AND json_extract(je.value, '$.depth')::INTEGER <= max_depth
          AND NOT (json_extract_string(je.value, '$.name') LIKE '\_%' 
                   OR json_extract_string(je.value, '$.name') LIKE '_%')
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

-- Find methods (functions inside classes)
CREATE OR REPLACE MACRO ast_methods(ast_obj, include_private := true) AS (
    WITH class_ids AS (
        SELECT json_extract(je.value, '$.id')::BIGINT as class_id
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.type') = 'class_definition'
    ),
    method_nodes AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.type') IN ('function_definition', 'method_definition')
          AND EXISTS (
              SELECT 1 FROM class_ids
              WHERE ast_is_descendant_of(
                  ast_obj.nodes,
                  json_extract(je.value, '$.id')::BIGINT,
                  class_ids.class_id
              )
          )
          AND (include_private OR NOT json_extract_string(je.value, '$.name') LIKE '\_%')
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(method_nodes.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(method_nodes.nodes) as n), 
            0
        ),
        nodes := COALESCE(method_nodes.nodes, '[]'::JSON)
    )
    FROM method_nodes
);

-- Helper: Check if node is descendant of another
CREATE OR REPLACE MACRO ast_is_descendant_of(nodes, child_id, ancestor_id) AS (
    WITH RECURSIVE path AS (
        SELECT 
            json_extract(je.value, '$.id')::BIGINT as id,
            json_extract(je.value, '$.parent_id')::BIGINT as parent_id
        FROM json_each(nodes) AS je
        WHERE json_extract(je.value, '$.id')::BIGINT = child_id
        
        UNION ALL
        
        SELECT 
            p.id,
            json_extract(je.value, '$.parent_id')::BIGINT as parent_id
        FROM path p
        JOIN json_each(nodes) AS je 
          ON json_extract(je.value, '$.id')::BIGINT = p.parent_id
        WHERE p.parent_id IS NOT NULL
    )
    SELECT COUNT(*) > 0
    FROM path
    WHERE parent_id = ancestor_id OR id = ancestor_id
);

-- Find error handlers (try/except/catch blocks)
CREATE OR REPLACE MACRO ast_error_handlers(ast_obj) AS (
    ast_filter_type(
        ast_obj, 
        ['try_statement', 'except_clause', 'catch_clause', 'finally_clause']
    )
);

-- Find imports
CREATE OR REPLACE MACRO ast_imports(ast_obj) AS (
    ast_filter_type(
        ast_obj,
        ['import_statement', 'import_from', 'require', 'include']
    )
);

-- Find TODO/FIXME comments
CREATE OR REPLACE MACRO ast_todos(ast_obj) AS (
    WITH todo_nodes AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.type') IN ('comment', 'string')
          AND (UPPER(json_extract_string(je.value, '$.content')) LIKE '%TODO%'
               OR UPPER(json_extract_string(je.value, '$.content')) LIKE '%FIXME%')
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(todo_nodes.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(todo_nodes.nodes) as n), 
            0
        ),
        nodes := COALESCE(todo_nodes.nodes, '[]'::JSON)
    )
    FROM todo_nodes
);

-- Get code quality indicators
CREATE OR REPLACE MACRO ast_code_quality(ast_obj) AS (
    WITH metrics AS (
        SELECT 
            ast_extract_summary(ast_obj) as summary,
            ast_extract_summary(ast_test_functions(ast_obj)) as test_summary,
            ast_extract_summary(ast_todos(ast_obj)) as todo_summary,
            ast_extract_summary(ast_public_api(ast_obj)) as api_summary
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        total_functions := metrics.summary.function_count,
        test_functions := metrics.test_summary.function_count,
        test_coverage_pct := ROUND(
            100.0 * metrics.test_summary.function_count / 
            NULLIF(metrics.summary.function_count, 0), 1
        ),
        todos_count := metrics.todo_summary.total_nodes,
        public_api_count := metrics.api_summary.function_count + metrics.api_summary.class_count,
        max_depth := metrics.summary.max_depth,
        avg_depth := metrics.summary.avg_depth,
        lines_of_code := metrics.summary.lines_of_code,
        complexity_rating := CASE
            WHEN metrics.summary.max_depth > 10 THEN 'High'
            WHEN metrics.summary.max_depth > 6 THEN 'Medium'
            ELSE 'Low'
        END
    )
    FROM metrics
);

-- Suggest relevant macros based on intent (simplified)
CREATE OR REPLACE MACRO ast_suggest_macro(intent) AS (
    WITH suggestions AS (
        SELECT 
            CASE LOWER(intent)
                WHEN intent LIKE '%function%' THEN 
                    ['ast_filter_type', 'ast_extract_function_names', 'ast_methods']
                WHEN intent LIKE '%test%' THEN 
                    ['ast_test_functions', 'ast_filter_name_pattern']
                WHEN intent LIKE '%class%' THEN 
                    ['ast_filter_type', 'ast_extract_class_names', 'ast_methods']
                WHEN intent LIKE '%import%' THEN 
                    ['ast_imports', 'ast_filter_type']
                WHEN intent LIKE '%error%' OR intent LIKE '%exception%' THEN 
                    ['ast_error_handlers', 'ast_filter_type']
                WHEN intent LIKE '%source%' OR intent LIKE '%code%' THEN 
                    ['ast_extract_source', 'ast_extract_entities']
                WHEN intent LIKE '%quality%' OR intent LIKE '%metric%' THEN 
                    ['ast_code_quality', 'ast_extract_summary', 'ast_complexity']
                ELSE 
                    ['ast_filter_type', 'ast_extract_entities', 'ast_extract_summary']
            END as macros
    )
    SELECT macros FROM suggestions
);
)SQLMACRO"},
    {"utility_macros.sql", R"SQLMACRO(
-- Utility Macros for Common Operations
-- These combine multiple operations for convenience

-- Normalize integer value to integer array
CREATE OR REPLACE MACRO ast_normalize_to_int_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::INTEGER[]
        WHEN typeof(val) = 'INTEGER[]' THEN val
        WHEN typeof(val) = 'BIGINT[]' THEN val::INTEGER[]
        WHEN typeof(val) = 'INTEGER' THEN [val]::INTEGER[]
        WHEN typeof(val) = 'BIGINT' THEN [val::INTEGER]::INTEGER[]
        ELSE []::INTEGER[]
    END
);

-- Normalize varchar value to varchar array  
CREATE OR REPLACE MACRO ast_normalize_to_varchar_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::VARCHAR[]
        WHEN typeof(val) = 'VARCHAR[]' THEN val
        WHEN typeof(val) = 'VARCHAR' THEN [val]::VARCHAR[]
        ELSE []::VARCHAR[]
    END
);

-- Find functions and methods together
CREATE OR REPLACE MACRO ast_all_functions(nodes) AS (
    ast_find_type(nodes, ['function_definition', 'method_definition'])
);

-- Find all definition types (functions, classes, methods)
CREATE OR REPLACE MACRO ast_all_definitions(nodes) AS (
    ast_find_type(nodes, ['function_definition', 'class_definition', 'method_definition'])
);

-- Find all literal values
CREATE OR REPLACE MACRO ast_all_literals(nodes) AS (
    ast_find_type(nodes, ['string', 'integer', 'float', 'boolean', 'null'])
);

-- Find all control flow statements
CREATE OR REPLACE MACRO ast_control_flow(nodes) AS (
    ast_find_type(nodes, ['if_statement', 'while_statement', 'for_statement', 'try_statement', 'with_statement'])
);

-- Get nodes at top level (depths 0-2)
CREATE OR REPLACE MACRO ast_top_level(nodes) AS (
    ast_at_depth(nodes, [0, 1, 2])
);

-- Get nodes at mid level (depths 3-5)
CREATE OR REPLACE MACRO ast_mid_level(nodes) AS (
    ast_at_depth(nodes, [3, 4, 5])
);

-- Get deeply nested nodes (depth > 5)
CREATE OR REPLACE MACRO ast_deep_nodes(nodes) AS (
    (SELECT json_group_array(je.value)
     FROM json_each(nodes) AS je
     WHERE json_extract(je.value, '$.depth')::INTEGER > 5)
);
)SQLMACRO"},
};

} // namespace duckdb
