-- Semantic Type Functions for DuckDB AST Extension
-- These functions work with the 8-bit semantic_type field that combines kind and super_type

-- ===================================
-- SEMANTIC TYPE PREDICATES
-- ===================================

-- Check if a node is a definition (COMPUTATION|DEFINITION = bits 7-4 = 0111 = 112)
CREATE OR REPLACE MACRO ast_is_definition(semantic_type) AS (
    (semantic_type & 240) = 112
);

-- Check if a node is an execution statement (CONTROL_EFFECTS|EXECUTION = bits 7-4 = 1000 = 128)
CREATE OR REPLACE MACRO ast_is_execution(semantic_type) AS (
    (semantic_type & 240) = 128
);

-- Check if a node is flow control (CONTROL_EFFECTS|FLOW_CONTROL = bits 7-4 = 1001 = 144)
CREATE OR REPLACE MACRO ast_is_flow_control(semantic_type) AS (
    (semantic_type & 240) = 144
);

-- Check if a node is a literal (super_type bit 4 = 16)
CREATE OR REPLACE MACRO ast_is_literal(semantic_type) AS (
    (semantic_type & 16) = 16
);

-- Check if a node is a type reference (super_type bit 3 = 8)
CREATE OR REPLACE MACRO ast_is_type(semantic_type) AS (
    (semantic_type & 8) = 8
);

-- Check if a node is a pattern (super_type bit 2 = 4)
CREATE OR REPLACE MACRO ast_is_pattern(semantic_type) AS (
    (semantic_type & 4) = 4
);

-- Check if a node is a modifier (super_type bit 1 = 2)
CREATE OR REPLACE MACRO ast_is_modifier(semantic_type) AS (
    (semantic_type & 2) = 2
);

-- Check if a node is a comment (super_type bit 0 = 1)
CREATE OR REPLACE MACRO ast_is_comment(semantic_type) AS (
    (semantic_type & 1) = 1
);

-- ===================================
-- TABLE FUNCTION FILTERS
-- ===================================

-- Filter AST table to only declarations
CREATE OR REPLACE MACRO ast_filter_declarations(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_declaration(semantic_type);

-- Filter AST table to only statements
CREATE OR REPLACE MACRO ast_filter_statements(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_statement(semantic_type);

-- Filter AST table to only expressions
CREATE OR REPLACE MACRO ast_filter_expressions(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_expression(semantic_type);

-- Filter AST table to only literals
CREATE OR REPLACE MACRO ast_filter_literals(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_literal(semantic_type);

-- ===================================
-- COMBINED FILTERS
-- ===================================

-- Get all executable code (statements + expressions)
CREATE OR REPLACE MACRO ast_filter_executable(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_statement(semantic_type) OR ast_is_expression(semantic_type);

-- Get all definitions (declarations that define something)
CREATE OR REPLACE MACRO ast_filter_definitions(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_declaration(semantic_type)
    AND normalized_type IN ('function_declaration', 'class_declaration', 'variable_declaration');

-- ===================================
-- SEMANTIC TYPE ANALYSIS
-- ===================================

-- Count nodes by semantic type
CREATE OR REPLACE MACRO ast_count_by_semantic_type(ast_table) AS TABLE
    SELECT 
        CASE 
            WHEN ast_is_declaration(semantic_type) THEN 'declaration'
            WHEN ast_is_statement(semantic_type) THEN 'statement'
            WHEN ast_is_expression(semantic_type) THEN 'expression'
            WHEN ast_is_literal(semantic_type) THEN 'literal'
            WHEN ast_is_type(semantic_type) THEN 'type'
            WHEN ast_is_pattern(semantic_type) THEN 'pattern'
            WHEN ast_is_modifier(semantic_type) THEN 'modifier'
            WHEN ast_is_comment(semantic_type) THEN 'comment'
            ELSE 'other'
        END as semantic_category,
        COUNT(*) as count
    FROM ast_table
    GROUP BY semantic_category
    ORDER BY count DESC;

-- Get semantic type breakdown for a specific node type
CREATE OR REPLACE MACRO ast_semantic_breakdown(ast_table, node_type) AS TABLE
    SELECT 
        type,
        semantic_type,
        ast_is_declaration(semantic_type) as is_declaration,
        ast_is_statement(semantic_type) as is_statement,
        ast_is_expression(semantic_type) as is_expression,
        ast_is_literal(semantic_type) as is_literal,
        COUNT(*) as count
    FROM ast_table
    WHERE type = node_type
    GROUP BY type, semantic_type, is_declaration, is_statement, is_expression, is_literal;