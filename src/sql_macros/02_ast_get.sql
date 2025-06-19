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