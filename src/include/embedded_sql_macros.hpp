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

-- Peer review features (chain methods)
CREATE OR REPLACE MACRO get_locations(nodes) AS (
    ast_get_locations(nodes)
);

CREATE OR REPLACE MACRO get_calls(nodes, root_node_id := NULL) AS (
    ast_get_calls(nodes, root_node_id := root_node_id)
);

CREATE OR REPLACE MACRO get_parent_chain(nodes, target_node_id, max_depth := NULL) AS (
    ast_get_parent_chain(nodes, target_node_id, max_depth := max_depth)
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
    {"05_hybrid_json_fix.sql", R"SQLMACRO(
-- Macro to create properly escaped JSON from AST data
CREATE OR REPLACE MACRO ast_to_json(file_path, language) AS (
    SELECT json_group_array(
        json_object(
            'id', node_id,
            'type', type,
            'name', name,
            'start', json_object('line', start_line, 'column', start_column),
            'end', json_object('line', end_line, 'column', end_column),
            'parent_id', parent_id,
            'depth', depth,
            'sibling_index', sibling_index
        )
    ) as nodes
    FROM read_ast(file_path, language)
);

-- Test macro to verify JSON escaping works correctly
CREATE OR REPLACE MACRO test_json_escaping(text_value) AS (
    SELECT to_json(text_value) as escaped_json
);
)SQLMACRO"},
    {"06_peer_review_features.sql", R"SQLMACRO(
-- Peer Review Feature Implementations
-- These macros implement high-priority features from peer review feedback

-- ===================================
-- ast_get_source - Extract source code with context
-- ===================================
-- Extracts source code for a node with optional context lines before/after
CREATE OR REPLACE MACRO ast_get_source(
    source_text,
    start_line, 
    end_line,
    context_lines := 0
) AS (
    WITH 
    -- Split source into lines
    lines AS (
        SELECT 
            ROW_NUMBER() OVER () as line_num,
            line
        FROM (
            SELECT UNNEST(string_split(source_text, chr(10))) as line
        )
    ),
    -- Calculate line range with context
    line_range AS (
        SELECT 
            GREATEST(1, start_line - context_lines) as first_line,
            end_line + context_lines as last_line
    )
    -- Extract and join the lines
    (SELECT string_agg(line, chr(10) ORDER BY line_num) as source
     FROM lines, line_range
     WHERE line_num >= line_range.first_line 
       AND line_num <= line_range.last_line)
);

-- Simpler version for single nodes (uses position from node)
CREATE OR REPLACE MACRO ast_node_get_source(
    node,
    source_text,
    context_lines := 0
) AS (
    ast_get_source(
        source_text,
        (node->'position'->>'start_row')::INTEGER + 1,  -- Convert 0-based to 1-based
        (node->'position'->>'end_row')::INTEGER + 1,
        context_lines := context_lines
    )
);

-- ===================================
-- ast_get_locations - Extract location information
-- ===================================
-- Returns a JSON array of location objects for named nodes
CREATE OR REPLACE MACRO ast_get_locations(nodes) AS (
    COALESCE(
        (SELECT json_group_array(json_object(
            'name', json_extract_string(je.value, '$.name'),
            'type', json_extract_string(je.value, '$.type'), 
            'start_line', json_extract(je.value, '$.position.start_row') + 1,
            'end_line', json_extract(je.value, '$.position.end_row') + 1,
            'start_column', json_extract(je.value, '$.position.start_column'),
            'end_column', json_extract(je.value, '$.position.end_column')
        ))
        FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
        WHERE json_extract_string(je.value, '$.name') IS NOT NULL),
        '[]'::JSON
    )
);

-- Chain method version
CREATE OR REPLACE MACRO get_locations(nodes) AS (
    ast_get_locations(nodes)
);

-- ===================================
-- ast_get_parent_chain - Get ancestors of a node
-- ===================================
-- Note: This requires parent_id to be present in nodes
-- For now, this is a placeholder that will work once we add parent tracking
CREATE OR REPLACE MACRO ast_get_parent_chain(
    nodes,
    target_node_id,
    max_depth := NULL
) AS (
    WITH RECURSIVE parent_chain AS (
        -- Start with the target node
        SELECT 
            node,
            0 as depth
        FROM json_each(nodes) as t(node)
        WHERE node->>'id' = target_node_id
        
        UNION ALL
        
        -- Find parent recursively
        SELECT 
            parent.node,
            pc.depth + 1
        FROM parent_chain pc,
             json_each(nodes) as parent(node)
        WHERE parent.node->>'id' = pc.node->>'parent_id'
          AND (max_depth IS NULL OR pc.depth < max_depth)
    )
    SELECT json_agg(node ORDER BY depth DESC) as parent_chain
    FROM parent_chain
);

-- ===================================
-- ast_get_calls - Extract function calls from a node
-- ===================================
-- Returns a JSON array of function call objects
CREATE OR REPLACE MACRO ast_get_calls(nodes, root_node_id := NULL) AS (
    COALESCE(
        (SELECT json_group_array(json_object(
            'called_function', json_extract_string(je.value, '$.name'),
            'call_type', json_extract_string(je.value, '$.type'),
            'line', json_extract(je.value, '$.position.start_row') + 1
        ))
        FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
        WHERE (json_extract_string(je.value, '$.normalized_type') = 'function_call'
               OR json_extract_string(je.value, '$.type') LIKE '%call%')
          AND json_extract_string(je.value, '$.name') IS NOT NULL
          AND (root_node_id IS NULL 
               OR json_extract_string(je.value, '$.id') = root_node_id
               OR json_extract_string(je.value, '$.parent_id') = root_node_id)),
        '[]'::JSON
    )
);

-- Chain method version
CREATE OR REPLACE MACRO get_calls(nodes) AS (
    ast_get_calls(nodes)
);

-- ===================================
-- Helper: Check if file content is available
-- ===================================
CREATE OR REPLACE MACRO ast_has_source(node) AS (
    node->>'source_file' IS NOT NULL
);

-- ===================================
-- Convenience wrapper: Extract source from file
-- ===================================
CREATE OR REPLACE MACRO ast_extract_source(
    file_path,
    start_line,
    end_line,
    context_lines := 0
) AS (
    WITH file AS (
        SELECT 
            string_split(content, chr(10)) AS lines,
            generate_subscripts(lines, 1) AS lineno,
            UNNEST(lines) AS line
        FROM read_text(file_path)
    )
    SELECT string_agg(line, chr(10) ORDER BY lineno) AS source
    FROM file 
    WHERE lineno BETWEEN GREATEST(1, start_line - context_lines) 
                     AND end_line + context_lines
);

-- ===================================
-- Integration example: Get function with source
-- ===================================
CREATE OR REPLACE MACRO ast_get_functions_with_source(
    file_path,
    language,
    context_lines := 2
) AS (
    WITH parsed AS (
        SELECT 
            nodes,
            read_text(file_path) as source_text
        FROM read_ast_objects(file_path, language)
    ),
    functions AS (
        SELECT 
            node,
            source_text
        FROM parsed,
             json_each(parsed.nodes) as t(node)
        WHERE node->>'type' = 'function_definition'
           OR node->>'normalized_type' = 'function_declaration'
    )
    SELECT 
        node->>'name' as function_name,
        (node->'position'->>'start_row')::INTEGER + 1 as start_line,
        (node->'position'->>'end_row')::INTEGER + 1 as end_line,
        ast_node_get_source(node, source_text, context_lines) as source_code
    FROM functions
);
)SQLMACRO"},
};

} // namespace duckdb
