-- Core AST Primitives
-- Low-level building blocks for AST manipulation

-- ===================================
-- PRIMITIVE UTILITIES
-- ===================================

-- Filter nodes with index preservation
-- Returns array of (index, node) pairs
CREATE OR REPLACE MACRO ast_nodes_filter_with_index(nodes, predicate) AS (
    [
        struct_pack(idx := idx, node := node)
        for (idx, node) in enumerate(nodes)
        if predicate(node)
    ]
);

-- Get complete subtrees for nodes at given positions
-- Takes original nodes array and array of (idx, node) pairs
-- Returns flattened array of all nodes in the subtrees
CREATE OR REPLACE MACRO ast_nodes_get_branches(nodes, indexed_nodes) AS (
    [
        nodes[i]
        for indexed in indexed_nodes
        for i in range(indexed.idx, indexed.idx + indexed.node.descendant_count + 1)
    ]
);

-- Flatten nested arrays
CREATE OR REPLACE MACRO ast_flatten(nested_nodes) AS (
    list_reduce(nested_nodes, (acc, x) -> list_concat(acc, x), [])
);

-- Update AST struct fields
-- Since DuckDB lacks struct_update, we rebuild the struct
CREATE OR REPLACE MACRO ast_update(ast, nodes := NULL, source := NULL) AS (
    struct_pack(
        source := COALESCE(source, ast.source),
        nodes := COALESCE(nodes, ast.nodes)
    )
);

-- Create AST struct from components
CREATE OR REPLACE MACRO ast_pack(file_path, language, nodes) AS (
    struct_pack(
        source := struct_pack(
            file_path := file_path,
            language := language
        ),
        nodes := nodes
    )
);

-- Create AST struct from source struct and nodes
CREATE OR REPLACE MACRO ast_pack_with_source(source, nodes) AS (
    struct_pack(
        source := source,
        nodes := nodes
    )
);

-- Extract ranges from indexed nodes
-- Converts [(idx, node)] to [(start, end)] for subtree ranges
CREATE OR REPLACE MACRO ast_nodes_to_ranges(indexed_nodes) AS (
    [
        struct_pack(
            start := indexed.idx,
            end := indexed.idx + indexed.node.descendant_count + 1
        )
        for indexed in indexed_nodes
    ]
);

-- Slice nodes using multiple ranges
CREATE OR REPLACE MACRO ast_nodes_slice_ranges(nodes, ranges) AS (
    ast_flatten([
        [nodes[i] for i in range(r.start, r.end)]
        for r in ranges
    ])
);

-- ===================================
-- CORE PATTERN
-- ===================================

-- Get complete subtrees for nodes matching a predicate
-- This is the fundamental building block for ast_get_* functions
CREATE OR REPLACE MACRO ast_get_branches(ast, predicate) AS (
    ast_update(
        ast,
        nodes := ast_nodes_get_branches(
            ast.nodes,
            ast_nodes_filter_with_index(ast.nodes, predicate)
        )
    )
);