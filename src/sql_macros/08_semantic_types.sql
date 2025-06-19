-- Semantic Type Functions for DuckDB AST Extension
-- These functions work with the 8-bit semantic_type field that combines kind and super_type

-- ===================================
-- SEMANTIC TYPE PREDICATES
-- ===================================

-- Check if a node is a definition (COMPUTATION|DEFINITION = 1111 xxxx = 240-255)
CREATE OR REPLACE TEMPORARY MACRO ast_is_definition(semantic_type) AS (
    (semantic_type & 240) = 240
);

-- Check if a node is an execution statement (CONTROL_EFFECTS|EXECUTION = 1000 xxxx = 128-143)
CREATE OR REPLACE TEMPORARY MACRO ast_is_execution(semantic_type) AS (
    (semantic_type & 240) = 128
);

-- Check if a node is flow control (CONTROL_EFFECTS|FLOW_CONTROL = 1001 xxxx = 144-159)  
CREATE OR REPLACE TEMPORARY MACRO ast_is_flow_control(semantic_type) AS (
    (semantic_type & 240) = 144
);

-- Check if a node is a literal (DATA_STRUCTURE|LITERAL = 0100 xxxx = 64-79)
CREATE OR REPLACE TEMPORARY MACRO ast_is_literal(semantic_type) AS (
    (semantic_type & 240) = 64
);

-- Check if a node is a type reference (DATA_STRUCTURE|TYPE = 0111 xxxx = 112-127)
CREATE OR REPLACE TEMPORARY MACRO ast_is_type(semantic_type) AS (
    (semantic_type & 240) = 112
);

-- Check if a node is a pattern (DATA_STRUCTURE|PATTERN = 0110 xxxx = 96-111)
CREATE OR REPLACE TEMPORARY MACRO ast_is_pattern(semantic_type) AS (
    (semantic_type & 240) = 96
);

-- Check if a node is metadata (META_EXTERNAL|METADATA = 0010 xxxx = 32-47)
CREATE OR REPLACE TEMPORARY MACRO ast_is_metadata(semantic_type) AS (
    (semantic_type & 240) = 32
);

-- Check if a node is a comment (specifically METADATA_COMMENT = 32)
CREATE OR REPLACE TEMPORARY MACRO ast_is_comment(semantic_type) AS (
    semantic_type = 32
);

-- Check if a node is a name (DATA_STRUCTURE|NAME = 0101 xxxx = 80-95)
CREATE OR REPLACE TEMPORARY MACRO ast_is_name(semantic_type) AS (
    (semantic_type & 240) = 80
);

-- Check if a node is an operator (COMPUTATION|OPERATOR = 1100 xxxx = 192-207)
CREATE OR REPLACE TEMPORARY MACRO ast_is_operator(semantic_type) AS (
    (semantic_type & 240) = 192
);

-- Check if a node is external (META_EXTERNAL|EXTERNAL = 0011 xxxx = 48-63)
CREATE OR REPLACE TEMPORARY MACRO ast_is_external(semantic_type) AS (
    (semantic_type & 240) = 48
);

-- ===================================
-- UNIVERSAL FLAGS PREDICATES  
-- ===================================

-- Check if a node is a keyword using universal_flags
CREATE OR REPLACE TEMPORARY MACRO ast_is_keyword(universal_flags) AS (
    (universal_flags & 1) = 1
);

-- Check if a node is public using universal_flags
CREATE OR REPLACE TEMPORARY MACRO ast_is_public(universal_flags) AS (
    (universal_flags & 2) = 2
);

-- Check if a node is unsafe using universal_flags
CREATE OR REPLACE TEMPORARY MACRO ast_is_unsafe(universal_flags) AS (
    (universal_flags & 4) = 4
);

-- ===================================
-- TABLE FUNCTION FILTERS
-- ===================================

-- Filter AST table to only definitions
CREATE OR REPLACE TEMPORARY MACRO ast_filter_definitions(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_definition(semantic_type);

-- Filter AST table to only execution statements
CREATE OR REPLACE TEMPORARY MACRO ast_filter_execution(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_execution(semantic_type);

-- Filter AST table to only flow control
CREATE OR REPLACE TEMPORARY MACRO ast_filter_flow_control(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_flow_control(semantic_type);

-- Filter AST table to only literals
CREATE OR REPLACE TEMPORARY MACRO ast_filter_literals(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_literal(semantic_type);

-- Filter AST table to only keywords
CREATE OR REPLACE TEMPORARY MACRO ast_filter_keywords(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_keyword(universal_flags);

-- Filter AST table to only public constructs
CREATE OR REPLACE TEMPORARY MACRO ast_filter_public(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_public(universal_flags);

-- Filter AST table to only unsafe constructs
CREATE OR REPLACE TEMPORARY MACRO ast_filter_unsafe(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_unsafe(universal_flags);

-- ===================================
-- COMBINED FILTERS
-- ===================================

-- Get all executable code (execution + flow control)
CREATE OR REPLACE TEMPORARY MACRO ast_filter_executable(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_execution(semantic_type) OR ast_is_flow_control(semantic_type);

-- Get all patterns (destructuring, collecting, templates, matching)
CREATE OR REPLACE TEMPORARY MACRO ast_filter_patterns(ast_table) AS TABLE
    SELECT * FROM ast_table
    WHERE ast_is_pattern(semantic_type);

-- ===================================
-- SEMANTIC TYPE ANALYSIS
-- ===================================

-- Count nodes by semantic type
CREATE OR REPLACE TEMPORARY MACRO ast_count_by_semantic_type(ast_table) AS TABLE
    SELECT 
        CASE 
            WHEN ast_is_definition(semantic_type) THEN 'definition'
            WHEN ast_is_execution(semantic_type) THEN 'execution'
            WHEN ast_is_flow_control(semantic_type) THEN 'flow_control'
            WHEN ast_is_operator(semantic_type) THEN 'operator'
            WHEN ast_is_literal(semantic_type) THEN 'literal'
            WHEN ast_is_name(semantic_type) THEN 'name'
            WHEN ast_is_type(semantic_type) THEN 'type'
            WHEN ast_is_pattern(semantic_type) THEN 'pattern'
            WHEN ast_is_metadata(semantic_type) THEN 'metadata'
            WHEN ast_is_external(semantic_type) THEN 'external'
            ELSE 'other'
        END as semantic_category,
        COUNT(*) as count
    FROM ast_table
    GROUP BY semantic_category
    ORDER BY count DESC;

-- Get semantic type breakdown for a specific node type
CREATE OR REPLACE TEMPORARY MACRO ast_semantic_breakdown(ast_table, node_type) AS TABLE
    SELECT 
        type,
        semantic_type,
        ast_is_definition(semantic_type) as is_definition,
        ast_is_execution(semantic_type) as is_execution,
        ast_is_flow_control(semantic_type) as is_flow_control,
        ast_is_literal(semantic_type) as is_literal,
        ast_is_pattern(semantic_type) as is_pattern,
        COUNT(*) as count
    FROM ast_table
    WHERE type = node_type
    GROUP BY type, semantic_type, is_definition, is_execution, is_flow_control, is_literal, is_pattern;