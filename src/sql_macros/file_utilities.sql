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
-- ----------------------------------------------------------------------------
-- ast_get_source: Raw source text for a line range of a file
-- ----------------------------------------------------------------------------
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
-- ----------------------------------------------------------------------------
-- ast_get_source_numbered: Source text for a line range, each line prefixed with its number
-- ----------------------------------------------------------------------------
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

-- For an AST table you already parsed. `ast_table` is a table NAME (a string),
-- resolved with query_table(): parse once with read_ast, query many times.
--
-- WHY THIS ONE TAKES A SECOND ARGUMENT, unlike the other `_from` variants of
-- the M2 stage 3 split. The result carries the SOURCE TEXT of each match, and a
-- node table holds no source text -- only line numbers. So the paths come along
-- as `files`, which is read with read_text() exactly as the path-taking twin
-- reads them, in the same way ast_unparse_exact_from(ast_table, files, ...)
-- takes both. There is no `language` parameter, because a parsed table carries
-- its own.
-- Usage:
--   CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
--   SELECT * FROM ast_source_of_from('my_ast', 'src/**/*.py', 'process_payment');
--   SELECT * FROM ast_source_of_from('my_ast', 'src/**/*.py', 'MyClass', kind := 'class');
--
-- KNOWN LIMITATION: `files` AND `ast_table` CAN DISAGREE, AND THE RESULT IS
-- NARROWED SILENTLY. The file_contents join below matches
-- read_text(files).filename against the table's file_path EXACTLY, so a match
-- whose file is absent from `files`, or spelled differently there, is dropped
-- with no error. Pass read_text the same pattern you passed read_ast.
--
-- Two ways to get this wrong, both MEASURED on this build:
--   * A NARROWER `files`. A table parsed from 'test/data/python/q*.py' (2
--     files, 4 matches for '__init__') with files := one of those two paths
--     returns 3 rows -- exactly the matches in that file, and the same 3 rows
--     the twin returns for that single path. Narrowing is therefore usable, not
--     broken, and it is pinned in test/sql/macro_from_parity_source_of.test.
--   * A MIXED PATH SPELLING, which is the trap. A GLOB is normalised by both
--     read_ast and read_text: './test/data/python/q*.py' and
--     'test/data/python/q*.py' both yield 'test/data/python/...', so mixing
--     those two spellings is harmless (4 rows). An EXACT path is NOT
--     normalised: './test/data/python/qualified_names.py' keeps its './' in
--     both functions. So pairing a './'-prefixed exact path with an unprefixed
--     glob -- in EITHER direction -- yields 0 ROWS AND NO ERROR.
--
-- THE SILENT 0 IS A DEFECT, NOT A DECISION, AND IT HAS AN OWNER:
-- tracker/bugs/058. It breaks the contract at
-- tracker/features/044-v2-m1-contract.md:16 ("no silent-empty (#89)") and this
-- stage's own principle at tracker/roadmap/m2-stage3-macro-split-plan.md:60
-- ("0 rows mean 'searched correctly, not there'"). 058 carries the measurement,
-- the intended loud-failure shape (normalise the join key, raise on a match
-- whose file is unreadable) and the reason it is not fixed in this stage: the
-- join above is now SHARED with the path-taking twin, which delegates here, so
-- changing it would change the twin's behaviour -- which M2 stage 3 forbids.
-- ast_unparse_exact_from is the PRECEDENT to follow, not the fix's home: it
-- normalises for matching (ast_unparse.sql:593) and raises on an unreadable
-- file (:737). #89 itself is CLOSED (tracker/features/046-cpp-selector-engine.md:4).
--
-- ast_unparse.sql:564-569 reports a third spelling, Windows native backslashes
-- from the globber; that is NOT measured here. Its neighbouring :562-563 says
-- the globber './'-prefixes relative paths, which DISAGREES with the glob
-- measurement above and was not reproduced on this build -- the pinned 0 in
-- column 3 of the test's five-count row decides it, because a './'-prefixing
-- globber would put './' on both sides and make that cell 3.
-- ----------------------------------------------------------------------------
-- ast_source_of_from: Source text for matched nodes in a parsed AST table, read from files
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_source_of_from(
    ast_table,
    files,
    target_name,
    kind := NULL
) AS TABLE
    WITH
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
            FROM query_table(ast_table)
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
            JOIN read_text(files) fc ON fc.filename = m.file_path
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

-- Look up a definition by name and return its numbered source code
-- Usage:
--   SELECT * FROM ast_source_of('src/**/*.py', 'process_payment');
--   SELECT * FROM ast_source_of('src/**/*.py', 'MyClass', kind := 'class');
--   SELECT * FROM ast_source_of('src/**/*.py', 'handler', language := 'python', kind := 'function');
-- ----------------------------------------------------------------------------
-- ast_source_of: Source text for matched nodes, keyed by file pattern
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_source_of(
    file_patterns,
    target_name,
    language := NULL,
    kind := NULL
) AS TABLE
    -- Parse, then delegate to ast_source_of_from, which holds the match filter,
    -- the read_text join, the projection and the ORDER BY. `file_patterns` is
    -- passed TWICE on purpose: once as the table the nodes come from, once as
    -- the `files` the source lines are read from. That is the coupling this
    -- path-taking form guarantees and the two-argument form makes optional --
    -- which is also why this form cannot reach the mismatch the `_from` banner
    -- warns about.
    WITH __ast_source_of_src AS (
        SELECT * FROM read_ast(file_patterns, language)
    )
    SELECT * FROM ast_source_of_from('__ast_source_of_src', file_patterns,
                                     target_name, kind := kind);

-- =============================================================================
-- Source Extraction - Single Line
-- =============================================================================

-- Get a single line from a file
-- ----------------------------------------------------------------------------
-- ast_get_source_line: A single line of source text from a file
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_get_source_line(file_path, line_num) AS (
    WITH file_content AS (
        SELECT string_split(content, E'\n') as lines
        FROM read_text(file_path)
    )
    SELECT lines[line_num]
    FROM file_content
);
