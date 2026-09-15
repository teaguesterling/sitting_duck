-- ============================================================================
-- ast_unparse — reconstruct source text from a sitting_duck AST (PROTOTYPE)
-- ============================================================================
-- Rules-based unparser. Reconstructs source from `read_ast` rows WITHOUT `peek`
-- (peek is a truncated preview — lossy on long tokens). See prototypes/unparse/
-- README.md for the design and the `.def` integration plan.
--
-- Load on top of the sitting_duck extension:
--   LOAD 'build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
--   .read prototypes/unparse/ast_unparse.sql
--   SELECT source FROM ast_unparse('path/to/file.py');
--
-- Three rule layers, all derivable from data the AST already carries:
--   1. Leaf text  — COALESCE(NULLIF(name,''), type). Anonymous tokens have
--                    type == the literal (`def`, `(`, `+`); named leaves carry
--                    their text in `name` when name_strategy = NODE_TEXT.
--   2. Spacing    — universal tight-token rules (no space before ,;:)]} / after
--                    ([{. ). Language-agnostic; the code conventions are shared.
--   3. Layout     — newline when a leaf starts on a later line than the previous
--                    leaf ENDED on (preserves blank lines); indent = number of
--                    enclosing "block" nodes, found without recursion via the
--                    contiguous-descendant-range invariant.
--
-- The per-language knob today is the block-type map below. The README shows how
-- this becomes an `IS_INDENT_BLOCK` flag in the .def files (language-agnostic).
-- ============================================================================

CREATE OR REPLACE MACRO ast_unparse(path) AS TABLE (
  WITH ast AS (
    SELECT * FROM read_ast(path)
  ),
  -- PER-LANGUAGE KNOB (prototype): the node type whose nesting drives indentation.
  -- Production: replace this join with `WHERE is_indent_block(flags)` — see README.
  block_type_map AS (
    SELECT * FROM (VALUES
      ('python',     'block'),
      ('javascript', 'statement_block'),
      ('typescript', 'statement_block'),
      ('c',          'compound_statement'),
      ('cpp',        'compound_statement'),
      ('java',       'block'),
      ('go',         'block'),
      ('rust',       'block'),
      ('ruby',       'body_statement')
    ) t(language, block_type)
  ),
  blocks AS (
    SELECT a.file_path, a.node_id, a.descendant_count
    FROM ast a
    JOIN block_type_map m ON a.language = m.language AND a.type = m.block_type
  ),
  leaves AS (
    SELECT
      l.file_path,
      l.node_id,
      l.start_line,
      l.end_line,
      COALESCE(NULLIF(l.name, ''), l.type) AS tok,
      -- indent = count of enclosing block nodes. A is an ancestor of L iff
      -- A.node_id <= L.node_id <= A.node_id + A.descendant_count (pre-order ranges).
      (SELECT count(*) FROM blocks b
       WHERE b.file_path = l.file_path
         AND b.node_id <= l.node_id
         AND l.node_id <= b.node_id + b.descendant_count) AS indent
    FROM ast l
    WHERE l.children_count = 0            -- leaves carry all the source text
  ),
  seq AS (
    SELECT *,
      row_number()  OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)      OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(end_line) OVER (PARTITION BY file_path ORDER BY node_id) AS p_end_line
    FROM leaves
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL                       THEN ''
        -- newline(s) when this leaf starts below where the previous one ended;
        -- prefix indentation for the new line. (Blank lines preserved by count.)
        WHEN start_line > p_end_line            THEN repeat(chr(10), start_line - p_end_line)
                                                     || repeat('    ', indent)
        -- tight tokens: no space before these, no space after those (universal).
        WHEN tok  IN (',', ';', ':', ')', ']', '}') THEN ''
        WHEN ptok IN ('(', '[', '{', '.')       THEN ''
        WHEN tok = '.'                          THEN ''
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);
