-- This macro file provides useful analysis functions built on top of the DuckDB AST extension
-- Load this file with: duckdb -init useful_macros.sql

-- First load the extension
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- ========================================
-- FUNCTION ANALYSIS MACROS
-- ========================================

-- Get all functions from a file with semantic type filtering
CREATE OR REPLACE MACRO get_functions_from_file(file_path) AS TABLE
    SELECT 
        node_id,
        name,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        children_count,
        descendant_count
    FROM read_ast(file_path)
    WHERE (semantic_type & 240) = 112  -- Is DEFINITION (bits 7-4 = 0111)
      AND type IN ('function_definition', 'function_declarator', 'method_definition');

-- Summarize functions across multiple files - simplified version
CREATE OR REPLACE MACRO summarize_functions_from_files(file_pattern) AS TABLE
    SELECT 
        file_path,
        COUNT(*) as function_count,
        AVG(end_line - start_line + 1) as avg_lines,
        MAX(end_line - start_line + 1) as max_lines,
        SUM(end_line - start_line + 1) as total_lines,
        AVG(descendant_count) as avg_complexity
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 128) = 128  -- Is declaration (bit 7)
      AND type IN ('function_definition', 'function_declarator', 'method_definition')
    GROUP BY file_path
    ORDER BY function_count DESC;

-- Find complex functions (by line count or descendant count)
CREATE OR REPLACE MACRO find_complex_functions(file_pattern, min_lines, min_descendants) AS TABLE
    SELECT 
        file_path,
        name,
        start_line,
        end_line - start_line + 1 as line_count,
        descendant_count,
        ROUND(descendant_count::FLOAT / GREATEST(end_line - start_line + 1, 1), 2) as density
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 128) = 128  -- Is declaration (bit 7)
      AND type IN ('function_definition', 'function_declarator', 'method_definition')
      AND (end_line - start_line + 1 >= min_lines OR descendant_count >= min_descendants)
    ORDER BY descendant_count DESC;

-- ========================================
-- CLASS ANALYSIS MACROS
-- ========================================

-- Analyze classes in files - simplified version
CREATE OR REPLACE MACRO analyze_classes(file_pattern) AS TABLE
    WITH classes AS (
        SELECT 
            file_path,
            name as class_name,
            node_id,
            start_line,
            end_line,
            descendant_count
        FROM read_ast(file_pattern)
        WHERE (semantic_type & 128) = 128  -- Is declaration (bit 7)
          AND type IN ('class_declaration', 'class_definition')
    )
    SELECT 
        file_path,
        class_name,
        end_line - start_line + 1 as line_count,
        descendant_count
    FROM classes
    ORDER BY descendant_count DESC;

-- ========================================
-- IMPORT/DEPENDENCY ANALYSIS
-- ========================================

-- Extract all imports/includes from files
CREATE OR REPLACE MACRO get_imports(file_pattern) AS TABLE
    SELECT DISTINCT
        file_path,
        name as import_name,
        type,
        start_line
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 64) = 64  -- Is statement (bit 6)
      AND type IN ('include_directive', 'import_statement', 'import_declaration')
    ORDER BY file_path, start_line;

-- ========================================
-- CODE QUALITY METRICS
-- ========================================

-- Calculate file-level metrics for a single file
CREATE OR REPLACE MACRO analyze_file_metrics(file_path) AS TABLE
    WITH base_data AS (
        SELECT * FROM read_ast(file_path)
    ),
    node_stats AS (
        SELECT 
            COUNT(*) as total_nodes,
            MAX(depth) as max_depth,
            AVG(depth) as avg_depth,
            COUNT(DISTINCT type) as unique_node_types
        FROM base_data
    ),
    type_sample AS (
        SELECT STRING_AGG(type, ', ' ORDER BY type) as example_types
        FROM (
            SELECT DISTINCT type
            FROM base_data
            ORDER BY type
            LIMIT 5
        ) t
    ),
    semantic_stats AS (
        SELECT 
            COUNT(*) FILTER (WHERE (semantic_type & 128) = 128) as declarations,
            COUNT(*) FILTER (WHERE (semantic_type & 64) = 64) as statements,
            COUNT(*) FILTER (WHERE (semantic_type & 32) = 32) as expressions,
            COUNT(*) FILTER (WHERE type IN ('function_definition', 'function_declarator', 'method_definition')) as functions,
            COUNT(*) FILTER (WHERE type IN ('class_declaration', 'class_definition')) as classes
        FROM base_data
    )
    SELECT 
        file_path,
        total_nodes,
        max_depth,
        ROUND(avg_depth, 2) as avg_depth,
        unique_node_types,
        example_types,
        declarations,
        statements,
        expressions,
        functions,
        classes
    FROM node_stats, type_sample, semantic_stats;

-- ========================================
-- SEMANTIC TYPE ANALYSIS
-- ========================================

-- Show semantic type distribution across files
CREATE OR REPLACE MACRO analyze_semantic_types(file_pattern) AS TABLE
    WITH type_breakdown AS (
        SELECT 
            file_path,
            CASE 
                WHEN (semantic_type & 128) = 128 THEN 'DECLARATION'
                WHEN (semantic_type & 64) = 64 THEN 'STATEMENT'
                WHEN (semantic_type & 32) = 32 THEN 'EXPRESSION'
                WHEN (semantic_type & 16) = 16 THEN 'LITERAL'
                WHEN (semantic_type & 8) = 8 THEN 'TYPE'
                WHEN (semantic_type & 4) = 4 THEN 'PATTERN'
                WHEN (semantic_type & 2) = 2 THEN 'MODIFIER'
                WHEN (semantic_type & 1) = 1 THEN 'COMMENT'
                ELSE 'OTHER'
            END as semantic_category,
            COUNT(*) as node_count
        FROM read_ast(file_pattern)
        GROUP BY file_path, semantic_category
    )
    SELECT 
        file_path,
        semantic_category,
        node_count,
        ROUND(100.0 * node_count / SUM(node_count) OVER (PARTITION BY file_path), 2) as percentage
    FROM type_breakdown
    ORDER BY file_path, node_count DESC;

-- ========================================
-- FIND TODO/FIXME COMMENTS
-- ========================================

-- Find TODO/FIXME comments
CREATE OR REPLACE MACRO find_todos(file_pattern) AS TABLE
    SELECT 
        file_path,
        start_line,
        peek,
        CASE 
            WHEN UPPER(peek) LIKE '%TODO%' THEN 'TODO'
            WHEN UPPER(peek) LIKE '%FIXME%' THEN 'FIXME'
            WHEN UPPER(peek) LIKE '%HACK%' THEN 'HACK'
            WHEN UPPER(peek) LIKE '%XXX%' THEN 'XXX'
            ELSE 'OTHER'
        END as comment_type
    FROM read_ast(file_pattern)
    WHERE type = 'comment'
      AND REGEXP_MATCHES(UPPER(peek), 'TODO|FIXME|HACK|XXX')
    ORDER BY file_path, start_line;

-- ========================================
-- EXAMPLE USAGE
-- ========================================

-- Get function summary for all C++ files:
-- FROM summarize_functions_from_files('src/*.cpp');

-- Find complex functions needing refactoring:
-- FROM find_complex_functions('src/**/*.cpp', 30, 50);

-- Analyze class structure:
-- FROM analyze_classes('src/*.cpp');

-- Get import dependencies:
-- FROM get_imports('src/**/*.cpp');

-- Get code quality metrics:
-- FROM analyze_file_metrics('src/unified_ast_backend.cpp');

-- Find all TODOs:
-- FROM find_todos('src/**/*.*');