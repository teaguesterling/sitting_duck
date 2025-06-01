-- AST Find Functions
-- Node extraction operations that return detached nodes (breaks tree structure)

-- ===================================
-- BASIC FIND FUNCTIONS
-- ===================================

-- Find nodes by type(s) - returns detached nodes
CREATE OR REPLACE MACRO ast_find_types(ast, types) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if list_contains(types, node.type)]
    )
);

-- Find nodes by single type
CREATE OR REPLACE MACRO ast_find_type(ast, type) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.type = type]
    )
);

-- Find nodes at specific depth
CREATE OR REPLACE MACRO ast_find_depth(ast, depth) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.depth = depth]
    )
);

-- Find nodes by name
CREATE OR REPLACE MACRO ast_find_name(ast, name) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.name = name]
    )
);

-- Find all identifiers
CREATE OR REPLACE MACRO ast_find_identifiers(ast) AS (
    ast_find_type(ast, 'identifier')
);

-- Find all literals
CREATE OR REPLACE MACRO ast_find_literals(ast) AS (
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
CREATE OR REPLACE MACRO ast_find_calls(ast) AS (
    ast_find_types(ast, [
        'call_expression',
        'function_call',
        'method_call',
        'new_expression'
    ])
);

-- Find variable declarations
CREATE OR REPLACE MACRO ast_find_declarations(ast) AS (
    ast_find_types(ast, [
        'variable_declaration',
        'variable_declarator',
        'const_declaration',
        'let_declaration'
    ])
);

-- Find control flow statements
CREATE OR REPLACE MACRO ast_find_control_flow(ast) AS (
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
CREATE OR REPLACE MACRO ast_find_leaves(ast) AS (
    ast_update(
        ast,
        [node for node in ast.nodes if node.children_count = 0]
    )
);

-- Find parent nodes (have children)
CREATE OR REPLACE MACRO ast_find_parents(ast) AS (
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