-- =============================================================================
-- AST-anchored source patching: ast_node_edit / ast_patch / ast_replace
-- =============================================================================
--
-- Edits-as-data: plain rows describe source edits anchored at AST node
-- positions; ast_patch re-reads the files and applies the edits as a pure
-- transformation (no file writing). This is the "patch tools first" step of
-- the v2 architecture RFC (docs/planning/v2-architecture.md, "Patch / unparse
-- / rewrite") — codemods in SQL before any unparser exists.
--
-- POSITION SEMANTICS (ground truth, verified empirically in
-- test/sql/ast_patch.test and against src/ast_type.cpp):
--
--   * start_line / end_line are 1-indexed and inclusive.
--   * start_column / end_column are 1-indexed BYTE offsets within their line
--     (tree-sitter points are byte-based; multi-byte UTF-8 characters advance
--     the column by their byte width, not 1).
--   * start_column is the offset of the node's FIRST byte.
--   * end_column is EXCLUSIVE: it is one past the node's last byte on
--     end_line (src: ts_node_end_point(node).column + 1, and tree-sitter end
--     points are exclusive). A single-line node's text is the byte range
--     [start_column, end_column) of its line, i.e. for ASCII content
--     substr(line, start_column, end_column - start_column).
--   * Location columns require parsing with source := 'full' (otherwise
--     start_column / end_column do not exist). ast_patch errors on NULL
--     positions with a re-parse hint.
--
-- WHY FILES ARE RE-READ (and why parse_ast output cannot be patched):
-- NO extraction configuration retains per-node source text — `source :=`
-- controls location columns only, and peek is presentation, never a
-- correctness substrate (v2 RFC, "Engine principles"). Exact source therefore
-- comes from re-reading the file at patch time. Consequences:
--   * Edits anchored to in-memory parse_ast() results have no file to read
--     (parse_ast emits file_path = '<inline>'): ast_patch errors clearly.
--     Parse from a file with read_ast instead.
--   * Patch positions are only valid for the file content that was parsed.
--     If the file changed since parsing, positions may be out of range —
--     ast_patch errors on any position outside the current file content,
--     which doubles as a cheap staleness guard (it cannot catch every
--     staleness case: a same-shape edit elsewhere in the file is invisible).
--     Parse and patch in one motion; do not cache edits across file changes.
--
-- EDIT ROWS: any relation with these columns works (build them by hand or
-- with ast_node_edit):
--
--   file_path    VARCHAR   -- file the edit applies to
--   start_line   (integer) -- 1-indexed, inclusive
--   start_column (integer) -- 1-indexed byte offset, inclusive
--   end_line     (integer) -- 1-indexed, inclusive
--   end_column   (integer) -- 1-indexed byte offset, EXCLUSIVE
--   edit_kind    VARCHAR   -- 'replace' | 'delete' | 'insert_before' | 'insert_after'
--   new_text     VARCHAR   -- replacement/inserted text (ignored for 'delete')
--
-- Kinds: 'replace' substitutes new_text for the anchored byte range;
-- 'delete' removes it; 'insert_before' inserts new_text at the range's start
-- (zero-width — nothing is removed); 'insert_after' inserts just past the
-- range's end.
--
-- VALIDATION (issue #89: no silent wrong output — every failure errors, no
-- partial results):
--   * unknown edit_kind — error
--   * NULL file_path / positions — error (parse_ast or missing source:='full')
--   * NULL new_text for a non-delete kind — error
--   * file not covered by the files argument, or unreadable — error
--   * position out of range for current file content — error (staleness guard)
--   * overlapping edits in one file — error; edits must target disjoint byte
--     ranges. Two zero-width inserts at the same point (or an insert at the
--     exact start of another edit) are rejected as ambiguous ordering.
--     Adjacent edits (one ends exactly where the next starts) are fine.
--
-- APPLICATION ORDER: all edit offsets are resolved against the pristine
-- file content, and the output is reassembled in a single pass (unchanged
-- gap + replacement, in position order). This is order-independent — the
-- same invariant bottom-up (descending-position) application provides —
-- so the row order of the edits table does not matter.
--
-- ast_patch does NOT write files. To write a patched result back:
--
--   COPY (SELECT patched_source FROM ast_patch('edits', 'src/main.py'))
--   TO 'src/main.py' (FORMAT csv, QUOTE '', ESCAPE '', HEADER false);
--
-- (or route through your VCS/tooling of choice — the macro stays pure).
--
-- Usage:
--   -- Build edits from matched nodes:
--   CREATE TABLE edits AS
--   SELECT unnest(ast_node_edit(r, 'replace', 'renamed_fn'))
--   FROM read_ast('src/main.py', source := 'full') r
--   WHERE type = 'identifier' AND name = 'old_fn';
--
--   -- Apply (pure — returns patched text, writes nothing):
--   SELECT * FROM ast_patch('edits', 'src/main.py');
--
--   -- One-shot selector-driven replacement:
--   SELECT * FROM ast_replace('src/main.py', 'identifier[name=old_fn]', 'new_fn');
-- =============================================================================

-- =============================================================================
-- ast_node_edit: build a well-formed edit STRUCT from an AST row's location
-- columns. `node` is any STRUCT/row with file_path, start_line, start_column,
-- end_line, end_column — pass the table alias of a read_ast(...) scan
-- directly. Expand to columns with unnest(...):
--
--   SELECT unnest(ast_node_edit(r, 'replace', 'new_text'))
--   FROM read_ast('f.py', source := 'full') r WHERE <predicate>;
-- =============================================================================
CREATE OR REPLACE MACRO ast_node_edit(node, edit_kind, new_text) AS (
    {
        file_path: node.file_path,
        start_line: node.start_line,
        start_column: node.start_column,
        end_line: node.end_line,
        end_column: node.end_column,
        edit_kind: edit_kind,
        new_text: new_text
    }
);

-- =============================================================================
-- ast_patch: apply edit rows to their files, returning patched text.
--
--   edits — table/CTE name (string), resolved via query_table()
--           (ast_select_from pattern). Must expose the edit columns above.
--   files — glob / path / list of paths covering every edited file, passed
--           to read_text(). Required because read_text only accepts literal
--           arguments (no per-row lateral paths); pass the same glob used
--           for read_ast. An edit whose file_path is not covered errors.
--
-- Returns (file_path, patched_source), one row per edited file, ordered by
-- file_path. Zero edit rows produce zero output rows. Pure — writes nothing.
--
-- Byte-exactness: file content is sliced as BLOB byte ranges (columns are
-- byte offsets), so multi-byte characters before an edit cannot shift its
-- anchor. Slice boundaries fall on node boundaries, hence on character
-- boundaries; a boundary that splits a UTF-8 sequence (possible only with a
-- stale or hand-built edit) fails decode() loudly rather than corrupting
-- output.
-- =============================================================================
CREATE OR REPLACE MACRO ast_patch(edits, files) AS TABLE
    WITH
        -- File paths are matched textually against read_text(files) output.
        -- DuckDB's file glob returns './'-prefixed relative paths while exact
        -- paths pass through verbatim, so a leading './' is stripped on BOTH
        -- sides (edits and file contents) to make the two spellings of one
        -- file a single join key — otherwise one file's edits could silently
        -- split across two patched rows. Output file_path is the normalized
        -- spelling.
        edit_rows AS (
            SELECT regexp_replace(file_path, '^\./', '') AS file_path,
                   start_line, start_column, end_line, end_column,
                   edit_kind, new_text
            FROM query_table(edits)
        ),

        -- ---------------------------------------------------------------
        -- Validation (#89: error loudly, never emit partial/wrong output)
        -- ---------------------------------------------------------------
        kind_validation AS (
            SELECT CASE
                WHEN EXISTS (SELECT 1 FROM edit_rows
                             WHERE edit_kind IS NULL OR edit_kind NOT IN
                                   ('replace', 'delete', 'insert_before', 'insert_after'))
                THEN error('ast_patch: unknown edit_kind ''' ||
                           (SELECT COALESCE(edit_kind, 'NULL') FROM edit_rows
                            WHERE edit_kind IS NULL OR edit_kind NOT IN
                                  ('replace', 'delete', 'insert_before', 'insert_after')
                            LIMIT 1) ||
                           '''. Valid kinds: replace, delete, insert_before, insert_after.')
                ELSE true
            END AS ok
        ),
        anchor_validation AS (
            SELECT CASE
                WHEN EXISTS (SELECT 1 FROM edit_rows
                             WHERE file_path IS NULL OR file_path = '<inline>')
                THEN error('ast_patch: edit anchored to in-memory parse_ast() output ' ||
                           '(file_path is NULL or ''<inline>'') — this is unsupported: ' ||
                           'no extraction config retains per-node source text, so exact ' ||
                           'source must be re-read from a file (see the v2 architecture ' ||
                           'RFC). Parse from a file with read_ast(..., source := ' ||
                           '''full'') instead.')
                WHEN EXISTS (SELECT 1 FROM edit_rows
                             WHERE start_line IS NULL OR start_column IS NULL
                                OR end_line IS NULL OR end_column IS NULL)
                THEN error('ast_patch: edit has NULL position columns — location ' ||
                           'columns require parsing with source := ''full'' ' ||
                           '(start_column/end_column are absent otherwise). ' ||
                           'Re-parse with read_ast(..., source := ''full'') and ' ||
                           'rebuild the edits.')
                WHEN EXISTS (SELECT 1 FROM edit_rows
                             WHERE new_text IS NULL AND edit_kind != 'delete')
                THEN error('ast_patch: NULL new_text for a non-delete edit — pass the ' ||
                           'replacement text, or use edit_kind := ''delete'' to remove ' ||
                           'the range.')
                ELSE true
            END AS ok
        ),

        -- Re-read the files (the only correctness substrate for source text —
        -- never peek). read_text itself errors on an unmatchable files glob.
        file_contents AS (
            SELECT DISTINCT ON (file_path)
                   regexp_replace(rt.filename, '^\./', '') AS file_path,
                   rt.content
            FROM read_text(files) rt
            WHERE regexp_replace(rt.filename, '^\./', '')
                  IN (SELECT DISTINCT file_path FROM edit_rows
                      WHERE file_path IS NOT NULL)
        ),
        -- (NULL / '<inline>' paths are excluded here: validation subqueries
        -- have no guaranteed evaluation order, and those cases must get
        -- anchor_validation's dedicated parse_ast message, not this one.)
        coverage_validation AS (
            SELECT CASE
                WHEN EXISTS (SELECT 1 FROM edit_rows er
                             WHERE er.file_path IS NOT NULL
                               AND er.file_path != '<inline>'
                               AND er.file_path NOT IN (SELECT file_path FROM file_contents))
                THEN error('ast_patch: file ''' ||
                           (SELECT er.file_path FROM edit_rows er
                            WHERE er.file_path IS NOT NULL
                              AND er.file_path != '<inline>'
                              AND er.file_path NOT IN (SELECT file_path FROM file_contents)
                            LIMIT 1) ||
                           ''' has edits but was not readable via the files argument — ' ||
                           'pass the same glob/path used for read_ast, covering every ' ||
                           'edited file.')
                ELSE true
            END AS ok
        ),

        -- ---------------------------------------------------------------
        -- Line geometry: byte offset of each line start (0-based), byte
        -- length of each line. strlen() counts bytes (columns are bytes).
        -- ---------------------------------------------------------------
        file_lines AS (
            SELECT file_path, content, string_split(content, E'\n') AS lines
            FROM file_contents
        ),
        line_table AS (
            SELECT file_path, t.i AS line_number,
                   strlen(lines[t.i]) AS line_bytes
            FROM file_lines, generate_series(1, len(lines)) t(i)
        ),
        line_starts AS (
            SELECT file_path, line_number, line_bytes,
                   COALESCE(sum(line_bytes + 1) OVER (
                       PARTITION BY file_path ORDER BY line_number
                       ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING), 0) AS line_start
            FROM line_table
        ),

        positioned AS (
            SELECT e.*,
                   ls.line_start AS s_line_start, ls.line_bytes AS s_line_bytes,
                   le.line_start AS e_line_start, le.line_bytes AS e_line_bytes
            FROM edit_rows e
            LEFT JOIN line_starts ls
                   ON ls.file_path = e.file_path AND ls.line_number = e.start_line
            LEFT JOIN line_starts le
                   ON le.file_path = e.file_path AND le.line_number = e.end_line
        ),

        -- Out-of-range positions error: with re-read as the substrate, a
        -- position that doesn't fit the CURRENT file content most often means
        -- the file changed since parsing (cheap staleness guard — it cannot
        -- catch same-shape drift). Column upper bound is line_bytes + 1: the
        -- one-past-the-last-byte position is valid (exclusive end_column /
        -- zero-width insert at end of line).
        range_problems AS (
            SELECT CASE
                WHEN s_line_start IS NULL THEN
                    'start_line ' || start_line::VARCHAR || ' is beyond EOF of ' || file_path
                WHEN e_line_start IS NULL THEN
                    'end_line ' || end_line::VARCHAR || ' is beyond EOF of ' || file_path
                WHEN start_column < 1 OR start_column > s_line_bytes + 1 THEN
                    'start_column ' || start_column::VARCHAR || ' is outside line ' ||
                    start_line::VARCHAR || ' of ' || file_path || ' (line is ' ||
                    s_line_bytes::VARCHAR || ' bytes)'
                WHEN end_column < 1 OR end_column > e_line_bytes + 1 THEN
                    'end_column ' || end_column::VARCHAR || ' is outside line ' ||
                    end_line::VARCHAR || ' of ' || file_path || ' (line is ' ||
                    e_line_bytes::VARCHAR || ' bytes)'
                WHEN (e_line_start + end_column) < (s_line_start + start_column) THEN
                    'edit range in ' || file_path || ' ends (line ' || end_line::VARCHAR ||
                    ', col ' || end_column::VARCHAR || ') before it starts (line ' ||
                    start_line::VARCHAR || ', col ' || start_column::VARCHAR || ')'
            END AS problem
            FROM positioned
            -- Only files that were actually read: an uncovered file must get
            -- coverage_validation's message, not a misleading 'beyond EOF'.
            WHERE file_path IN (SELECT file_path FROM file_contents)
        ),
        range_validation AS (
            SELECT CASE
                WHEN EXISTS (SELECT 1 FROM range_problems WHERE problem IS NOT NULL)
                THEN error('ast_patch: position out of range for current file content — ' ||
                           (SELECT problem FROM range_problems WHERE problem IS NOT NULL
                            LIMIT 1) ||
                           '. The file likely changed since it was parsed; re-parse and ' ||
                           'rebuild the edits.')
                ELSE true
            END AS ok
        ),

        -- ---------------------------------------------------------------
        -- Normalize to half-open 0-based byte ranges [s_off, e_off) against
        -- the pristine content; inserts are zero-width.
        -- ---------------------------------------------------------------
        ranges AS (
            SELECT file_path,
                   CASE WHEN edit_kind = 'insert_after'
                        THEN e_line_start + end_column - 1
                        ELSE s_line_start + start_column - 1 END AS s_off,
                   CASE WHEN edit_kind = 'insert_before'
                        THEN s_line_start + start_column - 1
                        ELSE e_line_start + end_column - 1 END AS e_off,
                   CASE WHEN edit_kind = 'delete' THEN '' ELSE new_text END AS replacement
            FROM positioned
        ),

        -- Overlap detection: after sorting by (s_off, e_off) per file, an
        -- edit conflicts with its predecessor iff it starts inside the
        -- predecessor's range, or starts at exactly the same offset
        -- (ambiguous ordering — includes two inserts at one point).
        ordered AS (
            SELECT *,
                   lag(e_off) OVER w AS prev_e,
                   lag(s_off) OVER w AS prev_s
            FROM ranges
            WINDOW w AS (PARTITION BY file_path ORDER BY s_off, e_off)
        ),
        overlap_validation AS (
            SELECT CASE
                WHEN EXISTS (SELECT 1 FROM ordered
                             WHERE prev_e IS NOT NULL
                               AND (s_off < prev_e OR s_off = prev_s))
                THEN error('ast_patch: overlapping edits in ' ||
                           (SELECT file_path FROM ordered
                            WHERE prev_e IS NOT NULL
                              AND (s_off < prev_e OR s_off = prev_s)
                            LIMIT 1) ||
                           ' — edits must target disjoint source ranges (a selector ' ||
                           'matching both a node and one of its descendants is the ' ||
                           'usual cause). No partial output is produced.')
                ELSE true
            END AS ok
        ),

        -- ---------------------------------------------------------------
        -- Reassembly: single ordered pass over disjoint ranges — for each
        -- edit, emit the untouched gap since the previous edit's end, then
        -- its replacement; append the tail after the last edit. Offsets are
        -- all against pristine content, so application is order-independent
        -- (the same invariant bottom-up application provides).
        -- ---------------------------------------------------------------
        content_blobs AS (
            SELECT file_path, encode(content) AS cblob
            FROM file_contents
        ),
        pieces AS (
            SELECT file_path, s_off, e_off, replacement,
                   COALESCE(lag(e_off) OVER (
                       PARTITION BY file_path ORDER BY s_off, e_off), 0) AS prev_e
            FROM ranges
        ),
        assembled AS (
            SELECT p.file_path,
                   string_agg(
                       decode(cb.cblob[p.prev_e + 1 : p.s_off]) || p.replacement,
                       '' ORDER BY p.s_off, p.e_off) AS head,
                   max(p.e_off) AS last_e
            FROM pieces p
            JOIN content_blobs cb USING (file_path)
            GROUP BY p.file_path
        )
    SELECT a.file_path,
           a.head || decode(cb.cblob[a.last_e + 1 :]) AS patched_source
    FROM assembled a
    JOIN content_blobs cb USING (file_path)
    WHERE (SELECT ok FROM kind_validation)
      AND (SELECT ok FROM anchor_validation)
      AND (SELECT ok FROM coverage_validation)
      AND (SELECT ok FROM range_validation)
      AND (SELECT ok FROM overlap_validation)
    ORDER BY a.file_path;

-- =============================================================================
-- ast_replace: one-shot selector-driven replacement — match nodes with a CSS
-- selector (the ast_select engine), replace each match's exact byte range
-- with new_text, return the patched text per file.
--
--   source   — file path / glob (parsed internally with source := 'full';
--              also re-read by ast_patch — parse and patch happen in one
--              motion, which is exactly the staleness guidance above)
--   selector — CSS selector (see ast_select); e.g. 'identifier[name=old_fn]'
--   new_text — literal replacement text for every match. No capture
--              interpolation yet: captures would need per-capture exact
--              source, which requires source-text retention (v2 RFC substrate
--              gap) — literal-only until then.
--   language — optional language override (as read_ast)
--
-- Matcher choice: CSS selectors (ast_select_from), not ast_match patterns —
-- selector matches pass through the full read_ast row (including
-- start_column/end_column when parsed with source := 'full'), while
-- ast_match's captures carry line-only positions (no columns) and its
-- internal parse omits location columns entirely.
--
-- Matches that match nothing produce zero output rows (searched correctly,
-- not there). A selector matching both a node and its descendant produces
-- overlapping edits → ast_patch errors (refine the selector).
-- =============================================================================
CREATE OR REPLACE MACRO ast_replace(source, selector, new_text, language := NULL) AS TABLE
    WITH __ast_replace_src AS (
        SELECT * FROM read_ast(source, language, source := 'full',
            -- [peek...] attribute filters need materialized peek (NULL-definite
            -- semantics — see ast_select); peek stays presentation-only here:
            -- it can steer WHICH nodes match, never what text is written.
            peek := CASE WHEN selector LIKE '%[peek%'
                         THEN 'full' ELSE 'none+schema' END)
    ),
    __ast_replace_edits AS (
        SELECT DISTINCT file_path, start_line, start_column, end_line, end_column,
               'replace' AS edit_kind,
               new_text AS new_text
        FROM ast_select_from('__ast_replace_src', selector)
    )
    SELECT * FROM ast_patch('__ast_replace_edits', source);
