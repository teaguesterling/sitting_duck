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