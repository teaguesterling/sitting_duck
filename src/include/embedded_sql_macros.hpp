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
    {"01_core.sql", R"SQLMACRO(
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
    COALESCE(
        (SELECT json_group_array(je.value) 
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE list_contains(_ast_internal_ensure_varchar_array(types), json_extract_string(je.value, '$.type'))),
        '[]'::JSON
    )
);

-- Extract names from nodes, optionally filtered by type
CREATE OR REPLACE MACRO ast_get_names(nodes, node_type := NULL) AS (
    COALESCE(
        (SELECT json_group_array(json_extract_string(je.value, '$.name'))
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE json_extract_string(je.value, '$.name') IS NOT NULL
           AND (node_type IS NULL OR json_extract_string(je.value, '$.type') = node_type)),
        '[]'::JSON
    )
);

-- Find nodes at specific depth(s) - accepts integer or array
CREATE OR REPLACE MACRO ast_get_depth(nodes, depths) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
         WHERE list_contains(_ast_internal_ensure_integer_array(depths), json_extract(je.value, '$.depth')::INTEGER)),
        '[]'::JSON
    )
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
)SQLMACRO"},
    {"02a_entrypoint.sql", R"SQLMACRO(
-- AST Entrypoint and Chaining Support
-- Provides the ast() function for natural chaining syntax

-- ===================================
-- MAIN ENTRYPOINT
-- ===================================

-- Universal entrypoint that normalizes input to JSON array
CREATE OR REPLACE MACRO ast(input) AS (
    CASE 
        WHEN input IS NULL THEN '[]'::JSON
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'ARRAY' THEN input
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'OBJECT' THEN json_array(input)
        WHEN typeof(input) = 'STRUCT' THEN 
            COALESCE(TRY_CAST(json_extract(CAST(input AS JSON), '$.nodes') AS JSON), '[]'::JSON)
        WHEN typeof(input) = 'VARCHAR' THEN 
            CASE 
                WHEN TRY_CAST(input AS JSON) IS NULL THEN '[]'::JSON
                WHEN json_type(TRY_CAST(input AS JSON)) = 'ARRAY' THEN TRY_CAST(input AS JSON)
                WHEN json_type(TRY_CAST(input AS JSON)) = 'OBJECT' THEN json_array(TRY_CAST(input AS JSON))
                ELSE '[]'::JSON
            END
        ELSE '[]'::JSON
    END
);

-- ===================================
-- CHAIN METHODS
-- ===================================
-- Note: Chain methods (unprefixed names) are now created dynamically
-- by the duckdb_ast_register_short_names() C++ function
)SQLMACRO"},
    {"02b_chain_methods.sql", R"SQLMACRO(
-- Chain Methods for ast() entrypoint
-- These are activated by calling duckdb_ast_register_short_names()

-- ===================================
-- CHAIN METHODS (UNPREFIXED)
-- ===================================
-- These methods are designed to work with the ast() entrypoint
-- They take the normalized JSON array as their first parameter

-- Core extraction methods
CREATE OR REPLACE MACRO get_type(nodes, types) AS (
    ast_get_type(nodes, types)
);

CREATE OR REPLACE MACRO get_names(nodes, node_type := NULL) AS (
    ast_get_names(nodes, node_type := node_type)
);

CREATE OR REPLACE MACRO get_depth(nodes, depths) AS (
    ast_get_depth(nodes, depths)
);

-- Filtering methods
CREATE OR REPLACE MACRO filter_pattern(nodes, pattern, field := 'name') AS (
    ast_filter_pattern(nodes, pattern, field := field)
);

CREATE OR REPLACE MACRO filter_has_name(nodes) AS (
    ast_filter_has_name(nodes)
);

-- Navigation methods
CREATE OR REPLACE MACRO nav_children(nodes, parent_id) AS (
    ast_nav_children(nodes, parent_id)
);

CREATE OR REPLACE MACRO nav_parent(nodes, child_id) AS (
    ast_nav_parent(nodes, child_id)
);

-- Analysis methods
CREATE OR REPLACE MACRO summary(nodes) AS (
    ast_summary(nodes)
);

-- Chain-specific utility methods
CREATE OR REPLACE MACRO count_nodes(nodes) AS (
    json_array_length(nodes)
);

CREATE OR REPLACE MACRO first_node(nodes) AS (
    json_extract(nodes, '$[0]')
);

CREATE OR REPLACE MACRO last_node(nodes) AS (
    json_extract(nodes, '$[-1]')
);

-- Alternative names for better ergonomics
CREATE OR REPLACE MACRO find_type(nodes, types) AS (
    ast_get_type(nodes, types)
);

CREATE OR REPLACE MACRO find_depth(nodes, depths) AS (
    ast_get_depth(nodes, depths)
);

CREATE OR REPLACE MACRO extract_names(nodes, node_type := NULL) AS (
    ast_get_names(nodes, node_type := node_type)
);

CREATE OR REPLACE MACRO children(nodes, parent_id) AS (
    ast_nav_children(nodes, parent_id)
);

CREATE OR REPLACE MACRO parent(nodes, child_id) AS (
    ast_nav_parent(nodes, child_id)
);

CREATE OR REPLACE MACRO len(nodes) AS (
    json_array_length(nodes)
);

CREATE OR REPLACE MACRO size(nodes) AS (
    json_array_length(nodes)
);

-- ===================================
-- SHORT NAME ALIASES (WITHOUT ast_ PREFIX)
-- ===================================
-- These are standalone functions that can be used without ast()

CREATE OR REPLACE MACRO get_type_standalone(nodes, types) AS (
    ast_get_type(nodes, types)
);

CREATE OR REPLACE MACRO get_names_standalone(nodes, node_type := NULL) AS (
    ast_get_names(nodes, node_type := node_type)
);

CREATE OR REPLACE MACRO get_depth_standalone(nodes, depths) AS (
    ast_get_depth(nodes, depths)
);

CREATE OR REPLACE MACRO filter_pattern_standalone(nodes, pattern, field := 'name') AS (
    ast_filter_pattern(nodes, pattern, field := field)
);

CREATE OR REPLACE MACRO filter_has_name_standalone(nodes) AS (
    ast_filter_has_name(nodes)
);

CREATE OR REPLACE MACRO nav_children_standalone(nodes, parent_id) AS (
    ast_nav_children(nodes, parent_id)
);

CREATE OR REPLACE MACRO nav_parent_standalone(nodes, child_id) AS (
    ast_nav_parent(nodes, child_id)
);

CREATE OR REPLACE MACRO summary_standalone(nodes) AS (
    ast_summary(nodes)
);
)SQLMACRO"},
    {"03_legacy.sql", R"SQLMACRO(
-- Legacy API Support  
-- Note: Legacy functions have been removed to keep the API clean
-- Use the new ast_get_*, ast_filter_*, ast_nav_* functions instead

-- No legacy macros defined - keeping file for future use if needed
)SQLMACRO"},
    {"04_optional.sql", R"SQLMACRO(
-- Optional Features
-- Additional functionality that can be loaded separately

-- ===================================
-- SHORT NAME REGISTRATION
-- ===================================

-- Note: duckdb_ast_register_short_names() is now implemented as a C++ function
-- that loads the chain methods from 02b_chain_methods.sql

-- ===================================
-- HELP SYSTEM
-- ===================================

-- Interactive help
CREATE OR REPLACE MACRO ast_help() AS (
    json_object(
        'description', 'DuckDB AST Extension - Minimal API',
        'core_functions', json_array(
            'ast_get_type(nodes, types) - Find nodes by type(s)',
            'ast_get_names(nodes, type?) - Extract names',
            'ast_get_depth(nodes, depths) - Find nodes at depth(s)',
            'ast_filter_pattern(nodes, pattern) - Filter by pattern',
            'ast_filter_has_name(nodes) - Filter to named nodes',
            'ast_nav_children(nodes, id) - Get children',
            'ast_nav_parent(nodes, id) - Get parent',
            'ast_summary(nodes) - Get statistics'
        ),
        'chaining', 'Use ast(nodes).method().method() for chaining',
        'examples', json_array(
            'SELECT ast_get_type(nodes, ''function_definition'') FROM read_ast_objects(''file.py'', ''python'')',
            'SELECT ast(nodes).get_type(''function_definition'').count() FROM read_ast_objects(''file.py'', ''python'')'
        )
    )
);
)SQLMACRO"},
};

} // namespace duckdb
