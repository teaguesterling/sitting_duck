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
-- `files` AND `ast_table` MUST NAME THE SAME FILES. Pass read_text the same
-- pattern you passed read_ast. The file_contents join below matches
-- read_text(files).filename against the table's file_path after NORMALISING
-- both sides (leading './' stripped, '\' folded to '/'), the rule of
-- ast_unparse_exact_from (ast_unparse.sql:593). The raw file_path the node table
-- carried is what is returned. Normalising makes './x.py' and 'x.py' agree,
-- where an exact-string join used to drop the match.
--
-- The macro RAISES when any matched file has no content row through `files`
-- (the __sof_guard CTE): an absent or mismatched `files` is a failure to locate
-- the source, not an empty search. It names the count, one example path and the
-- remedy, as ast_unparse_exact_from does (ast_unparse.sql:736-741). A table with
-- no matches at all still returns 0 rows. A NULL or empty file_path in the node
-- table raises with its own message (the empty one names tracker/bugs/061, the
-- duckdb adapter's empty file_path), never the generic remedy.
-- The guard is a join in the final SELECT, so a consumer that statically asks
-- for no rows (LIMIT 0, WHERE false) prunes it and sees 0 rows; ast_unparse_exact
-- behaves the same way for LIMIT 0. A `files` that covers only SOME of the
-- matched files raises too, so narrowing `files` to a subset is an error.
-- Record: tracker/bugs/058, which carries the measurement and the decision
-- (Teague: "Raise plus the normalising join"). It honours
-- tracker/features/044-v2-m1-contract.md:16 ("no silent-empty (#89)"). The
-- path-taking twin delegates here and passes one pattern twice, so both halves
-- are a no-op for it: MEASURED 0 unmatched rows across four path spellings.
-- #89 itself is CLOSED (tracker/features/046-cpp-selector-engine.md:4).
--
-- Spellings, MEASURED before the fix: a GLOB is normalised by both read_ast and
-- read_text ('./test/data/python/q*.py' and 'test/data/python/q*.py' both yield
-- 'test/data/python/...'); an EXACT path is returned verbatim, so
-- './test/data/python/qualified_names.py' keeps its './'.
--
-- ast_unparse.sql:564-569 reports a third spelling, Windows native backslashes
-- from the globber; that is NOT measured here. Its neighbouring :562-563 says
-- the globber './'-prefixes relative paths, which DISAGREES with the glob
-- measurement above and was not reproduced on this build -- the 0 that column 3
-- of the test's five-count row pinned before 058 was fixed decided it, because
-- a './'-prefixing globber would have put './' on both sides and made that cell 3.
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
        -- Read file contents for all matched files. The join key is normalised
        -- on BOTH sides (leading './' stripped, '\' folded to '/'), the rule
        -- of ast_unparse_exact_from (ast_unparse.sql:593); the raw m.file_path
        -- is what is kept and returned.
        file_contents AS (
            SELECT DISTINCT ON (m.file_path)
                m.file_path,
                string_split(fc.content, E'\n') AS lines
            FROM matches m
            JOIN read_text(files) fc
              ON replace(regexp_replace(fc.filename, '^\./', ''), '\', '/')
               = replace(regexp_replace(m.file_path, '^\./', ''), '\', '/')
        ),
        -- A matched file with no content row is a failure to locate the source,
        -- not an empty search (tracker/bugs/058). Raise when ANY matched file is
        -- unjoined, as ast_unparse_exact_from does (ast_unparse.sql:736-741).
        -- No match at all is a real, quiet 0.
        --
        -- Branch order is the precedent's (ast_unparse.sql:704-709): NULL
        -- file_path, then empty, then the generic unjoined path. The NULL branch
        -- has to come first and has to count ROWS: count(DISTINCT file_path)
        -- ignores NULL, and error() given a NULL message returns NULL instead of
        -- raising, so a guard that built its message from a NULL file_path would
        -- fail open into 0 rows. The empty branch names the real cause (the
        -- duckdb adapter always writes an empty file_path, tracker/bugs/061)
        -- because the generic remedy would tell the caller to do what they did.
        __sof_guard AS (
            SELECT
                CASE
                    WHEN count(*) FILTER (WHERE m.file_path IS NULL) > 0
                        THEN error('ast_source_of_from: ' ||
                                   (count(*) FILTER (WHERE m.file_path IS NULL))::VARCHAR ||
                                   ' matched row(s) have a NULL file_path, so their source ' ||
                                   'cannot be located.')
                    WHEN count(*) FILTER (WHERE m.file_path = '') > 0
                        THEN error('ast_source_of_from: ' ||
                                   (count(*) FILTER (WHERE m.file_path = ''))::VARCHAR ||
                                   ' matched row(s) have an empty file_path, so their source ' ||
                                   'cannot be located. The duckdb adapter always produces an ' ||
                                   'empty file_path (tracker/bugs/061); parse a tree-sitter ' ||
                                   'language, or read the source yourself.')
                    WHEN count(DISTINCT m.file_path)
                             FILTER (WHERE fc.file_path IS NULL) > 0
                        THEN error('ast_source_of_from: ' ||
                                   (count(DISTINCT m.file_path)
                                        FILTER (WHERE fc.file_path IS NULL))::VARCHAR ||
                                   ' of ' || count(DISTINCT m.file_path)::VARCHAR ||
                                   ' matched file(s) have no content row in read_text(files) (e.g. ''' ||
                                   (min(m.file_path) FILTER (WHERE fc.file_path IS NULL)) ||
                                   '''); pass the same path or glob used for read_ast.')
                    ELSE true
                END AS ok
            FROM matches m
            LEFT JOIN file_contents fc ON fc.file_path = m.file_path
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
            JOIN __sof_guard g ON g.ok
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
