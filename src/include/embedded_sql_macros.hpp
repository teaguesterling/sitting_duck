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
    {"01_core_primitives.sql", R"SQLMACRO(
-- Core AST Primitives
-- Low-level building blocks for AST manipulation

-- ===================================
-- INDEXING PRIMITIVES
-- ===================================

-- Add indices to all nodes
-- Returns array of (idx, node) pairs for use with list_filter
CREATE OR REPLACE TEMPORARY MACRO ast_with_indices(nodes) AS (
    [struct_pack(idx := i, node := node) for node, i in nodes]
);

-- Extract complete subtrees for filtered indexed nodes
-- Takes original nodes array and filtered indexed results
-- Returns flattened array of all nodes in the subtrees
CREATE OR REPLACE TEMPORARY MACRO ast_extract_subtrees(nodes, filtered_indices) AS (
    flatten([
        [nodes[j] for j in range(fi.idx, fi.idx + fi.node.descendant_count + 1)]
        for fi in filtered_indices
    ])
);

-- ===================================
-- AST STRUCT OPERATIONS
-- ===================================

-- Update AST struct with new nodes
CREATE OR REPLACE TEMPORARY MACRO ast_update(ast, new_nodes) AS (
    struct_pack(
        source := ast.source,
        nodes := new_nodes
    )
);

-- Create AST struct from components
CREATE OR REPLACE TEMPORARY MACRO ast_pack(file_path, language, nodes) AS (
    struct_pack(
        source := struct_pack(
            file_path := file_path,
            language := language
        ),
        nodes := nodes
    )
);

-- ===================================
-- SPECIALIZED FILTERS (NO LAMBDAS)
-- ===================================

-- Get nodes by exact type match
CREATE OR REPLACE TEMPORARY MACRO ast_filter_by_type(nodes, type) AS (
    [indexed for indexed in ast_with_indices(nodes) if indexed.node.type = type]
);

-- Get nodes by type list
CREATE OR REPLACE TEMPORARY MACRO ast_filter_by_types(nodes, types) AS (
    [indexed for indexed in ast_with_indices(nodes) if list_contains(types, indexed.node.type)]
);

-- Get nodes by name
CREATE OR REPLACE TEMPORARY MACRO ast_filter_by_name(nodes, name) AS (
    [indexed for indexed in ast_with_indices(nodes) if indexed.node.name = name]
);

-- Get nodes at specific depth
CREATE OR REPLACE TEMPORARY MACRO ast_filter_by_depth(nodes, depth) AS (
    [indexed for indexed in ast_with_indices(nodes) if indexed.node.depth = depth]
);

-- ===================================
-- COMPLETE OPERATIONS
-- ===================================

-- Get complete subtrees for nodes of a specific type
CREATE OR REPLACE TEMPORARY MACRO ast_get_by_type(ast, type) AS (
    ast_update(
        ast,
        ast_extract_subtrees(
            ast.nodes,
            ast_filter_by_type(ast.nodes, type)
        )
    )
);

-- Get complete subtrees for nodes matching type list
CREATE OR REPLACE TEMPORARY MACRO ast_get_by_types(ast, types) AS (
    ast_update(
        ast,
        ast_extract_subtrees(
            ast.nodes,
            ast_filter_by_types(ast.nodes, types)
        )
    )
);
)SQLMACRO"},
};

} // namespace duckdb
