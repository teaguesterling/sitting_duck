-- File Utility Macros
-- Functions for reading and extracting portions of files

-- =============================================================================
-- Line Reading - Table Macros (return rows)
-- =============================================================================

-- Read all lines from a file as rows with line numbers
-- Returns: line_number (BIGINT), line (VARCHAR)
CREATE OR REPLACE MACRO read_lines(file_path) AS TABLE
    SELECT
        ROW_NUMBER() OVER () AS line_number,
        line
    FROM (
        SELECT UNNEST(string_split(content, E'\n')) AS line
        FROM read_text(file_path)
    );

-- Read specific line range from a file as rows
-- Returns: line_number (BIGINT), line (VARCHAR)
CREATE OR REPLACE MACRO read_lines_range(file_path, start_line, end_line) AS TABLE
    WITH numbered AS (
        SELECT
            ROW_NUMBER() OVER () AS line_number,
            line
        FROM (
            SELECT UNNEST(string_split(content, E'\n')) AS line
            FROM read_text(file_path)
        )
    )
    SELECT line_number, line
    FROM numbered
    WHERE line_number >= start_line AND line_number <= end_line;

-- Read lines around a specific line (context window)
-- Useful for showing code context around a specific location
CREATE OR REPLACE MACRO read_lines_context(file_path, center_line, context_lines) AS TABLE
    WITH numbered AS (
        SELECT
            ROW_NUMBER() OVER () AS line_number,
            line
        FROM (
            SELECT UNNEST(string_split(content, E'\n')) AS line
            FROM read_text(file_path)
        )
    )
    SELECT line_number, line
    FROM numbered
    WHERE line_number >= (center_line - context_lines)
      AND line_number <= (center_line + context_lines);

-- =============================================================================
-- Line Reading - Scalar Macros (return single string)
-- =============================================================================

-- Get a specific line range as a single string (newline-joined)
CREATE OR REPLACE MACRO get_lines_text(file_path, start_line, end_line) AS (
    SELECT string_agg(line, E'\n' ORDER BY line_number)
    FROM read_lines_range(file_path, start_line, end_line)
);

-- Get a single line from a file
CREATE OR REPLACE MACRO get_line(file_path, line_num) AS (
    SELECT line
    FROM read_lines_range(file_path, line_num, line_num)
    LIMIT 1
);

-- =============================================================================
-- Source Extraction Helpers (for use with read_ast results)
-- =============================================================================

-- Extract source code for an AST node given file_path, start_line, end_line
-- This is useful when you've already parsed and want to get the source
CREATE OR REPLACE MACRO ast_get_source(file_path, start_line, end_line) AS
    get_lines_text(file_path, start_line, end_line);

-- Get source with line numbers prefixed (useful for display)
CREATE OR REPLACE MACRO ast_get_source_numbered(file_path, start_line, end_line) AS (
    SELECT string_agg(
        printf('%4d: %s', line_number, line),
        E'\n' ORDER BY line_number
    )
    FROM read_lines_range(file_path, start_line, end_line)
);
