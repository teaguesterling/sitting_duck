-- AST Get Functions
-- Tree-preserving operations that return valid ASTs with complete subtrees

-- ===================================
-- GENERIC GET FUNCTIONS
-- ===================================

-- Get nodes by type(s)
CREATE OR REPLACE MACRO ast_get_types(ast, types) AS (
    ast_get_branches(ast, node -> list_contains(types, node.type))
);

-- Get nodes by single type
CREATE OR REPLACE MACRO ast_get_type(ast, type) AS (
    ast_get_branches(ast, node -> node.type = type)
);

-- Get nodes where property matches value(s)
CREATE OR REPLACE MACRO ast_get_property_in(ast, property, values) AS (
    ast_get_branches(ast, node -> list_contains(values, node[property]))
);

-- Get nodes where property equals value
CREATE OR REPLACE MACRO ast_get_property_eq(ast, property, value) AS (
    ast_get_branches(ast, node -> node[property] = value)
);

-- Get nodes at specific depth
CREATE OR REPLACE MACRO ast_get_depth(ast, depth) AS (
    ast_get_branches(ast, node -> node.depth = depth)
);

-- Get nodes with non-empty names
CREATE OR REPLACE MACRO ast_get_named(ast) AS (
    ast_get_branches(ast, node -> node.name IS NOT NULL AND node.name != '')
);

-- ===================================
-- LANGUAGE-AGNOSTIC SEMANTIC FUNCTIONS
-- ===================================

-- Get all functions (works across languages)
CREATE OR REPLACE MACRO ast_get_functions(ast) AS (
    ast_get_types(ast, [
        'function_declaration', 
        'function_definition', 
        'method_definition',
        'arrow_function',
        'function_expression'
    ])
);

-- Get all classes (works across languages)
CREATE OR REPLACE MACRO ast_get_classes(ast) AS (
    ast_get_types(ast, [
        'class_declaration',
        'class_definition',
        'class_specifier'
    ])
);

-- Get all imports/includes (works across languages)
CREATE OR REPLACE MACRO ast_get_imports(ast) AS (
    ast_get_types(ast, [
        'import_statement',
        'import_from_statement',
        'preproc_include',
        'use_declaration'
    ])
);

-- Get all comments
CREATE OR REPLACE MACRO ast_get_comments(ast) AS (
    ast_get_branches(ast, node -> node.type LIKE '%comment%')
);

-- ===================================
-- COMPLEX QUERIES
-- ===================================

-- Get nodes matching a name pattern
CREATE OR REPLACE MACRO ast_get_pattern(ast, pattern) AS (
    ast_get_branches(ast, node -> node.name LIKE pattern)
);

-- Get nodes within a line range
CREATE OR REPLACE MACRO ast_get_lines(ast, start_line, end_line) AS (
    ast_get_branches(ast, node -> 
        node.start_line >= start_line AND node.end_line <= end_line
    )
);

-- Get nodes with specific complexity (using descendant count as proxy)
CREATE OR REPLACE MACRO ast_get_complex(ast, min_descendants) AS (
    ast_get_branches(ast, node -> node.descendant_count >= min_descendants)
);

-- Get top-level nodes (direct children of root)
CREATE OR REPLACE MACRO ast_get_top_level(ast) AS (
    ast_get_branches(ast, node -> node.depth = 1)
);