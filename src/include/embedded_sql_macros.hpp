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
    {"02_ast_get.sql", R"SQLMACRO(
-- AST Get Functions
-- Tree-preserving operations that return valid ASTs with complete subtrees

-- ===================================
-- BASIC GET OPERATIONS
-- ===================================

-- Get nodes by type(s) - reuse primitives
CREATE OR REPLACE TEMPORARY MACRO ast_get_types(ast, types) AS (
    ast_get_by_types(ast, types)
);

-- Get nodes by single type - reuse primitives  
CREATE OR REPLACE TEMPORARY MACRO ast_get_type(ast, type) AS (
    ast_get_by_type(ast, type)
);

-- Get nodes at specific depth
CREATE OR REPLACE TEMPORARY MACRO ast_get_depth(ast, depth) AS (
    ast_update(
        ast,
        ast_extract_subtrees(
            ast.nodes,
            ast_filter_by_depth(ast.nodes, depth)
        )
    )
);

-- Get nodes with specific name
CREATE OR REPLACE TEMPORARY MACRO ast_get_name(ast, name) AS (
    ast_update(
        ast,
        ast_extract_subtrees(
            ast.nodes,
            ast_filter_by_name(ast.nodes, name)
        )
    )
);

-- ===================================
-- COMPLEX GET OPERATIONS
-- For these, users should use list_filter with lambdas
-- ===================================

-- Example usage patterns for complex queries:
-- Get nodes with non-empty names:
-- SELECT ast_update(ast, ast_extract_subtrees(ast.nodes, 
--     list_filter(ast_with_indices(ast.nodes), x -> x.node.name IS NOT NULL AND x.node.name != '')
-- )) FROM ...

-- Get nodes where property matches values:
-- SELECT ast_update(ast, ast_extract_subtrees(ast.nodes,
--     list_filter(ast_with_indices(ast.nodes), x -> list_contains(values, x.node[property]))
-- )) FROM ...

-- ===================================
-- LANGUAGE-AGNOSTIC SEMANTIC FUNCTIONS
-- ===================================

-- Get all functions (works across languages)
CREATE OR REPLACE TEMPORARY MACRO ast_get_functions(ast) AS (
    ast_get_types(ast, [
        'function_declaration', 
        'function_definition', 
        'method_definition',
        'arrow_function',
        'function_expression'
    ])
);

-- Get all classes (works across languages)
CREATE OR REPLACE TEMPORARY MACRO ast_get_classes(ast) AS (
    ast_get_types(ast, [
        'class_declaration',
        'class_definition',
        'class_specifier'
    ])
);

-- Get all imports/includes (works across languages)
CREATE OR REPLACE TEMPORARY MACRO ast_get_imports(ast) AS (
    ast_get_types(ast, [
        'import_statement',
        'import_from_statement',
        'preproc_include',
        'use_declaration'
    ])
);

-- Get top-level nodes (direct children of root)
CREATE OR REPLACE TEMPORARY MACRO ast_get_top_level(ast) AS (
    ast_get_depth(ast, 1)
);

-- ===================================
-- ADVANCED USAGE PATTERNS
-- ===================================

-- For complex predicates, users should compose with list_filter:

-- Get all comments:
-- SELECT ast_update(ast, ast_extract_subtrees(ast.nodes,
--     list_filter(ast_with_indices(ast.nodes), x -> x.node.type LIKE '%comment%')
-- )) FROM ...

-- Get nodes matching name pattern:
-- SELECT ast_update(ast, ast_extract_subtrees(ast.nodes,
--     list_filter(ast_with_indices(ast.nodes), x -> x.node.name LIKE '%test%')
-- )) FROM ...

-- Get nodes within line range:
-- SELECT ast_update(ast, ast_extract_subtrees(ast.nodes,
--     list_filter(ast_with_indices(ast.nodes), 
--         x -> x.node.start_line >= 10 AND x.node.end_line <= 20)
-- )) FROM ...

-- Get complex nodes (many descendants):
-- SELECT ast_update(ast, ast_extract_subtrees(ast.nodes,
--     list_filter(ast_with_indices(ast.nodes), x -> x.node.descendant_count >= 50)
-- )) FROM ...
)SQLMACRO"},
    {"03_ast_find.sql", R"SQLMACRO(
-- AST Find Functions
-- Node extraction operations that return detached nodes (breaks tree structure)

-- ===================================
-- BASIC FIND FUNCTIONS
-- ===================================

-- Find nodes by type(s) - returns detached nodes
CREATE OR REPLACE TEMPORARY MACRO ast_find_types(ast, types) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if list_contains(types, node.type)]
    )
);

-- Find nodes by single type
CREATE OR REPLACE TEMPORARY MACRO ast_find_type(ast, type) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.type = type]
    )
);

-- Find nodes at specific depth
CREATE OR REPLACE TEMPORARY MACRO ast_find_depth(ast, depth) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.depth = depth]
    )
);

-- Find nodes by name
CREATE OR REPLACE TEMPORARY MACRO ast_find_name(ast, name) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.name = name]
    )
);

-- Find all identifiers
CREATE OR REPLACE TEMPORARY MACRO ast_find_identifiers(ast) AS (
    ast_find_type(ast, 'identifier')
);

-- Find all literals
CREATE OR REPLACE TEMPORARY MACRO ast_find_literals(ast) AS (
    ast_update(
        ast,
        [node for node in ast.nodes 
         if node.type LIKE '%literal%' OR node.type LIKE '%number%' OR node.type LIKE '%string%']
    )
);

-- ===================================
-- SEMANTIC FIND FUNCTIONS
-- ===================================

-- Find function/method calls
CREATE OR REPLACE TEMPORARY MACRO ast_find_calls(ast) AS (
    ast_find_types(ast, [
        'call_expression',
        'function_call',
        'method_call',
        'new_expression'
    ])
);

-- Find variable declarations
CREATE OR REPLACE TEMPORARY MACRO ast_find_declarations(ast) AS (
    ast_find_types(ast, [
        'variable_declaration',
        'variable_declarator',
        'const_declaration',
        'let_declaration'
    ])
);

-- Find control flow statements
CREATE OR REPLACE TEMPORARY MACRO ast_find_control_flow(ast) AS (
    ast_find_types(ast, [
        'if_statement',
        'while_statement',
        'for_statement',
        'switch_statement',
        'try_statement',
        'catch_clause'
    ])
);

-- ===================================
-- SPECIALIZED FIND FUNCTIONS
-- ===================================

-- Find leaf nodes (no children)
CREATE OR REPLACE TEMPORARY MACRO ast_find_leaves(ast) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.children_count = 0]
    )
);

-- Find parent nodes (have children)
CREATE OR REPLACE TEMPORARY MACRO ast_find_parents(ast) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.children_count > 0]
    )
);

-- ===================================
-- ADVANCED USAGE PATTERNS
-- ===================================

-- For complex predicates, use list comprehensions directly:

-- Find nodes matching name pattern:
-- SELECT ast_update(ast, 
--     [node for node in ast.nodes if node.name LIKE '%test%']
-- ) FROM ...

-- Find nodes with multiple conditions:
-- SELECT ast_update(ast,
--     [node for node in ast.nodes 
--      if node.type = 'function' AND node.descendant_count > 10]
-- ) FROM ...

-- Or use list_filter for more complex logic:
-- SELECT ast_update(ast,
--     list_filter(ast.nodes, x -> x.name LIKE pattern AND x.depth = 2)
-- ) FROM ...
)SQLMACRO"},
    {"04_ast_to.sql", R"SQLMACRO(
-- AST To Functions
-- Transform AST data to other formats (breaks out of AST monad)

-- ===================================
-- BASIC TRANSFORMATIONS
-- ===================================

-- Extract all names as array
CREATE OR REPLACE TEMPORARY MACRO ast_to_names(ast, type := NULL) AS (
    [
        node.name
        for node in ast.nodes
        if node.name IS NOT NULL 
           AND node.name != ''
           AND (type IS NULL OR node.type = type)
    ]
);

-- Extract all types as array
CREATE OR REPLACE TEMPORARY MACRO ast_to_types(ast) AS (
    list_distinct([node.type for node in ast.nodes])
);

-- Extract source text snippets
CREATE OR REPLACE TEMPORARY MACRO ast_to_source(ast, type := NULL) AS (
    [
        node.source_text
        for node in ast.nodes
        if node.source_text IS NOT NULL
           AND node.source_text != ''
           AND (type IS NULL OR node.type = type)
    ]
);

-- Convert to location table
CREATE OR REPLACE TEMPORARY MACRO ast_to_locations(ast) AS (
    [
        struct_pack(
            node_id := node.node_id,
            type := node.type,
            name := node.name,
            file := ast.source.file_path,
            start_line := node.start_line,
            end_line := node.end_line,
            start_column := node.start_column,
            end_column := node.end_column
        )
        for node in ast.nodes
    ]
);

-- ===================================
-- STATISTICAL TRANSFORMATIONS
-- ===================================

-- Type frequency table
CREATE OR REPLACE TEMPORARY MACRO ast_to_type_stats(ast) AS (
    WITH type_counts AS (
        SELECT 
            node.type,
            count(*) as count
        FROM (SELECT unnest(ast.nodes) as node)
        GROUP BY node.type
    )
    SELECT 
        type,
        count,
        round(count * 100.0 / sum(count) OVER (), 2) as percentage
    FROM type_counts
    ORDER BY count DESC
);

-- Depth distribution
CREATE OR REPLACE TEMPORARY MACRO ast_to_depth_stats(ast) AS (
    WITH depth_counts AS (
        SELECT 
            node.depth,
            count(*) as count
        FROM (SELECT unnest(ast.nodes) as node)
        GROUP BY node.depth
    )
    SELECT 
        depth,
        count,
        round(count * 100.0 / sum(count) OVER (), 2) as percentage
    FROM depth_counts
    ORDER BY depth
);

-- Complexity metrics
CREATE OR REPLACE TEMPORARY MACRO ast_to_complexity_metrics(ast) AS (
    [
        struct_pack(
            name := node.name,
            type := node.type,
            line := node.start_line,
            descendants := node.descendant_count,
            children := node.children_count,
            lines := node.end_line - node.start_line + 1,
            complexity_score := node.descendant_count * 1.0 / GREATEST(node.end_line - node.start_line + 1, 1)
        )
        for node in ast.nodes
        if node.name IS NOT NULL AND node.children_count > 0
    ]
);

-- ===================================
-- CODE ANALYSIS TRANSFORMATIONS
-- ===================================

-- Function signatures
CREATE OR REPLACE TEMPORARY MACRO ast_to_signatures(ast) AS (
    [
        struct_pack(
            name := node.name,
            type := node.type,
            line := node.start_line,
            signature := trim(split_part(node.source_text, chr(10), 1))
        )
        for node in ast.nodes
        if node.type IN ('function_declaration', 'function_definition', 'method_definition')
           AND node.name IS NOT NULL
    ]
);

-- Import/dependency list
CREATE OR REPLACE TEMPORARY MACRO ast_to_dependencies(ast) AS (
    [
        struct_pack(
            line := node.start_line,
            statement := trim(node.source_text),
            type := node.type
        )
        for node in ast.nodes
        if node.type IN ('import_statement', 'import_from_statement', 'preproc_include')
    ]
);

-- Call graph edges (caller -> callee relationships)
CREATE OR REPLACE TEMPORARY MACRO ast_to_call_edges(ast) AS (
    WITH function_scopes AS (
        SELECT 
            node.node_id,
            node.name as function_name,
            node.descendant_count,
            row_number() OVER (ORDER BY node.node_id) - 1 as pos
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')
          AND node.name IS NOT NULL
    ),
    indexed_nodes AS (
        SELECT 
            node.*,
            row_number() OVER (ORDER BY node.node_id) - 1 as pos
        FROM (SELECT unnest(ast.nodes) as node)
    )
    SELECT DISTINCT
        f.function_name as caller,
        c.name as callee
    FROM function_scopes f
    JOIN indexed_nodes c 
        ON c.pos BETWEEN f.pos AND f.pos + f.descendant_count
        AND c.type IN ('call_expression', 'function_call')
        AND c.name IS NOT NULL
);

-- ===================================
-- SUMMARY TRANSFORMATIONS
-- ===================================

-- Overall AST summary
CREATE OR REPLACE TEMPORARY MACRO ast_to_summary(ast) AS (
    SELECT 
        ast.file_path,
        ast.language,
        length(ast.nodes) as total_nodes,
        (SELECT max(node.depth) FROM unnest(ast.nodes) as node) as max_depth,
        (SELECT count(*) FROM unnest(ast.nodes) as node WHERE node.children_count = 0) as leaf_count,
        (SELECT count(DISTINCT node.type) FROM unnest(ast.nodes) as node) as unique_types,
        (SELECT count(*) FROM unnest(ast.nodes) as node WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')) as function_count,
        (SELECT count(*) FROM unnest(ast.nodes) as node WHERE node.type IN ('class_declaration', 'class_definition')) as class_count
);
)SQLMACRO"},
    {"05_taxonomy.sql", R"SQLMACRO(
-- AST Taxonomy Functions
-- Functions that use the KIND system and semantic classification

-- ===================================
-- KIND-BASED FILTERS
-- ===================================

-- Get nodes by KIND (semantic category)
CREATE OR REPLACE TEMPORARY MACRO ast_filter_by_kind(nodes, kind_value) AS (
    [indexed for indexed in ast_with_indices(nodes) if indexed.node.kind = kind_value]
);

-- Get nodes by multiple KINDs
CREATE OR REPLACE TEMPORARY MACRO ast_filter_by_kinds(nodes, kind_values) AS (
    [indexed for indexed in ast_with_indices(nodes) if list_contains(kind_values, indexed.node.kind)]
);

-- Get all function-like nodes across languages (using KIND)
CREATE OR REPLACE TEMPORARY MACRO ast_filter_functions_by_kind(nodes) AS (
    ast_filter_by_kinds(nodes, [4, 5])  -- FUNCTION_DEF=4, METHOD_DEF=5
);

-- Get all literal nodes (using KIND)
CREATE OR REPLACE TEMPORARY MACRO ast_filter_literals_by_kind(nodes) AS (
    ast_filter_by_kind(nodes, 0)  -- LITERAL=0
);

-- Get all name/identifier nodes (using KIND)
CREATE OR REPLACE TEMPORARY MACRO ast_filter_names_by_kind(nodes) AS (
    ast_filter_by_kind(nodes, 1)  -- NAME=1
);

-- ===================================
-- FLAG-BASED FILTERS
-- ===================================

-- Get all public nodes (using universal flags)
CREATE OR REPLACE TEMPORARY MACRO ast_filter_public(nodes) AS (
    [indexed for indexed in ast_with_indices(nodes) if (indexed.node.universal_flags & 8) != 0]  -- is_public flag
);

-- Get all builtin nodes
CREATE OR REPLACE TEMPORARY MACRO ast_filter_builtin(nodes) AS (
    [indexed for indexed in ast_with_indices(nodes) if (indexed.node.universal_flags & 4) != 0]  -- is_builtin flag
);

-- Get all keyword nodes  
CREATE OR REPLACE TEMPORARY MACRO ast_filter_keywords(nodes) AS (
    [indexed for indexed in ast_with_indices(nodes) if (indexed.node.universal_flags & 1) != 0]  -- is_keyword flag
);

-- ===================================
-- SEMANTIC ID OPERATIONS
-- ===================================

-- Group nodes by semantic similarity (same semantic_id)
CREATE OR REPLACE TEMPORARY MACRO ast_group_by_semantic_id(nodes) AS (
    [
        struct_pack(
            semantic_id := sid,
            nodes := [n for n in nodes if n.semantic_id = sid]
        )
        for sid in list_distinct([n.semantic_id for n in nodes])
    ]
);

-- Find nodes with matching semantic pattern
CREATE OR REPLACE TEMPORARY MACRO ast_find_semantic_pattern(nodes, pattern_id) AS (
    [indexed for indexed in ast_with_indices(nodes) if indexed.node.semantic_id = pattern_id]
);

-- ===================================
-- COMPLETE OPERATIONS WITH TAXONOMY
-- ===================================

-- Get complete subtrees for functions (KIND-based)
CREATE OR REPLACE TEMPORARY MACRO ast_get_functions_by_kind(ast) AS (
    ast_update(
        ast,
        ast_extract_subtrees(
            ast.nodes,
            ast_filter_functions_by_kind(ast.nodes)
        )
    )
);

-- Get public interface nodes only
CREATE OR REPLACE TEMPORARY MACRO ast_get_public_interface(ast) AS (
    ast_update(
        ast,
        ast_extract_subtrees(
            ast.nodes,
            ast_filter_public(ast.nodes)
        )
    )
);

-- Get all cross-language literals
CREATE OR REPLACE TEMPORARY MACRO ast_get_literals_by_kind(ast) AS (
    ast_update(
        ast,
        ast_extract_subtrees(
            ast.nodes,
            ast_filter_literals_by_kind(ast.nodes)
        )
    )
);

-- ===================================
-- SEMANTIC QUERIES
-- ===================================

-- Cross-language function finding (using semantic classification)
CREATE OR REPLACE TEMPORARY MACRO ast_find_semantic_functions(ast) AS (
    ast_update(
        ast,
        [node for node in ast.nodes 
         if node.kind IN (4, 5)  -- FUNCTION_DEF, METHOD_DEF
         AND (node.universal_flags & 8) != 0]  -- is_public
    )
);

-- Find all control flow constructs
CREATE OR REPLACE TEMPORARY MACRO ast_find_control_flow_by_kind(ast) AS (
    ast_update(
        ast,
        [node for node in ast.nodes 
         if node.kind IN (9, 10)]  -- CONTROL_FLOW, CONDITIONAL (example values)
    )
);
)SQLMACRO"},
};

} // namespace duckdb
