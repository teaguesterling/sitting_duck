-- File Utility Macros
-- Scalar functions for extracting source code from files
--
-- For full-featured line reading (globs, line specs, context windows, lateral joins),
-- use the duckdb_read_lines extension: https://github.com/teaguesterling/duckdb_read_lines

-- =============================================================================
-- Source Extraction - Scalar Macros (return single value)
-- =============================================================================

-- Extract source code for an AST node given file_path, start_line, end_line
-- Returns lines as a single newline-joined string
CREATE OR REPLACE MACRO ast_get_source(file_path, start_line, end_line) AS (
    WITH file_content AS (
        SELECT string_split(content, E'\n') as lines
        FROM read_text(file_path)
    ),
    line_nums AS (
        SELECT lines, generate_series(1, len(lines)) as line_numbers
        FROM file_content
    ),
    all_lines AS (
        SELECT
            UNNEST(line_numbers) AS line_number,
            lines[UNNEST(line_numbers)] AS line
        FROM line_nums
    )
    SELECT string_agg(line, E'\n' ORDER BY line_number)
    FROM all_lines
    WHERE line_number >= start_line AND line_number <= end_line
);

-- Get source with line numbers prefixed (useful for display)
CREATE OR REPLACE MACRO ast_get_source_numbered(file_path, start_line, end_line) AS (
    WITH file_content AS (
        SELECT string_split(content, E'\n') as lines
        FROM read_text(file_path)
    ),
    line_nums AS (
        SELECT lines, generate_series(1, len(lines)) as line_numbers
        FROM file_content
    ),
    all_lines AS (
        SELECT
            UNNEST(line_numbers) AS line_number,
            lines[UNNEST(line_numbers)] AS line
        FROM line_nums
    )
    SELECT string_agg(
        printf('%4d: %s', line_number, line),
        E'\n' ORDER BY line_number
    )
    FROM all_lines
    WHERE line_number >= start_line AND line_number <= end_line
);

-- =============================================================================
-- Source Lookup - Table Macro (find definition by name, return numbered source)
-- =============================================================================

-- Look up a definition by name and return its numbered source code
-- Usage:
--   SELECT * FROM ast_source_of('src/**/*.py', 'process_payment');
--   SELECT * FROM ast_source_of('src/**/*.py', 'MyClass', kind := 'class');
--   SELECT * FROM ast_source_of('src/**/*.py', 'handler', language := 'python', kind := 'function');
CREATE OR REPLACE MACRO ast_source_of(
    file_patterns,
    target_name,
    language := NULL,
    kind := NULL
) AS TABLE
    WITH
        ast AS (
            SELECT *
            FROM read_ast(file_patterns, language)
        ),
        matches AS (
            SELECT
                file_path,
                name,
                CASE
                    WHEN is_function_definition(semantic_type) THEN 'function'
                    WHEN is_class_definition(semantic_type) THEN 'class'
                    WHEN is_variable_definition(semantic_type) THEN 'variable'
                    WHEN is_module_definition(semantic_type) THEN 'module'
                    WHEN is_type_definition(semantic_type) THEN 'type'
                    ELSE 'other'
                END AS definition_kind,
                type,
                start_line,
                end_line
            FROM ast
            WHERE is_definition(semantic_type)
              AND is_construct(flags)
              AND name = target_name
              AND (kind IS NULL
                   OR (kind = 'function' AND is_function_definition(semantic_type))
                   OR (kind = 'class' AND is_class_definition(semantic_type))
                   OR (kind = 'variable' AND is_variable_definition(semantic_type))
                   OR (kind = 'module' AND is_module_definition(semantic_type))
                   OR (kind = 'type' AND is_type_definition(semantic_type))
              )
        ),
        -- Read file contents for all matched files
        file_contents AS (
            SELECT DISTINCT ON (m.file_path)
                m.file_path,
                string_split(fc.content, E'\n') AS lines
            FROM matches m
            JOIN read_text(file_patterns) fc ON fc.filename = m.file_path
        ),
        -- Build numbered source for each match
        source_output AS (
            SELECT
                m.file_path,
                m.name,
                m.definition_kind,
                m.start_line,
                m.end_line,
                (SELECT string_agg(
                    printf('%4d: %s', line_num, fc.lines[line_num]),
                    E'\n' ORDER BY line_num
                )
                FROM generate_series(m.start_line, m.end_line) AS t(line_num)
                ) AS source
            FROM matches m
            JOIN file_contents fc ON fc.file_path = m.file_path
        )
    SELECT
        file_path,
        name,
        definition_kind,
        start_line,
        end_line,
        source
    FROM source_output
    ORDER BY file_path, start_line;

-- =============================================================================
-- Source Extraction - Single Line
-- =============================================================================

-- Get a single line from a file
CREATE OR REPLACE MACRO ast_get_source_line(file_path, line_num) AS (
    WITH file_content AS (
        SELECT string_split(content, E'\n') as lines
        FROM read_text(file_path)
    )
    SELECT lines[line_num]
    FROM file_content
);
