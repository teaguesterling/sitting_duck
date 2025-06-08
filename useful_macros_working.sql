-- Working macros for DuckDB AST extension
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Simple file metrics that works reliably  
CREATE OR REPLACE MACRO analyze_file_simple(file_path) AS TABLE
    SELECT 
        file_path,
        COUNT(*) as total_nodes,
        MAX(depth) as max_depth,
        COUNT(DISTINCT type) as unique_types,
        COUNT(*) FILTER (WHERE (semantic_type & 3840) = 512) as declarations,
        COUNT(*) FILTER (WHERE (semantic_type & 240) = 48) as functions
    FROM read_ast(file_path);

-- Get functions from a file using semantic types
CREATE OR REPLACE MACRO get_functions_simple(file_path) AS TABLE
    SELECT 
        name,
        start_line,
        end_line - start_line + 1 as line_count,
        descendant_count
    FROM read_ast(file_path)
    WHERE (semantic_type & 3840) = 512  -- DECLARATION
      AND (semantic_type & 240) IN (48, 64)  -- FUNCTION or METHOD
      AND name IS NOT NULL
    ORDER BY start_line;

-- Find comments with TODO/FIXME
CREATE OR REPLACE MACRO find_todos_simple(file_path) AS TABLE
    SELECT 
        start_line,
        LEFT(peek, 80) as comment_text
    FROM read_ast(file_path)
    WHERE type = 'comment'
      AND REGEXP_MATCHES(UPPER(peek), 'TODO|FIXME|HACK|XXX')
    ORDER BY start_line;