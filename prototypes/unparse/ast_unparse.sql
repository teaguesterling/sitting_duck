-- ============================================================================
-- ast_unparse — reconstruct source text from a sitting_duck AST (PROTOTYPE)
-- ============================================================================
-- Rules-based unparser. Reconstructs source from `read_ast` rows WITHOUT `peek`
-- (peek is a truncated preview — lossy on long tokens) such that
-- parse -> unparse -> re-parse yields the SAME tree ("pseudo-identity").
--
-- DESIGN NOTE: unparse is PRESENTATION, not semantics, so its rules do NOT live
-- in node-type semantic flags. They live in the two per-language, tag-keyed
-- lookup tables below (`indent_blocks`, `spacing`). Semantic flags
-- (IS_SCOPE, IS_CONSTITUENT, …) stay reserved for interpreting generalized
-- semantics. (Note IS_SCOPE does NOT track indentation — it sits on the
-- def/module, not the `block`/suite — so indentation needs this explicit list.)
--
-- Load on top of the extension:
--   LOAD 'build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
--   .read prototypes/unparse/ast_unparse.sql
--   SELECT source FROM ast_unparse('path/to/file.py');
--
-- Layers: leaf text = COALESCE(NULLIF(name,''), type) (anonymous tokens carry
-- text in `type`; named leaves in `name` via name_strategy=NODE_TEXT); spacing =
-- whitespace-by-default with an OR rule (drop the space iff the left token is
-- tight_after OR the right is tight_before); layout = newline when a leaf starts
-- below where the previous ended, indent = count of enclosing indent-block tags.
-- ============================================================================

CREATE OR REPLACE MACRO ast_unparse(path) AS TABLE (
  WITH ast AS (
    SELECT * FROM read_ast(path)
  ),

  -- ===================== UNPARSE RULES (per language, by tag) =====================
  -- (1) Indent-defining block tags. Nesting of these drives indentation.
  indent_blocks(language, tag) AS (VALUES
    ('python',     'block'),
    ('javascript', 'statement_block'),
    ('typescript', 'statement_block'),
    ('c',          'compound_statement'),
    ('cpp',        'compound_statement'),
    ('java',       'block'),
    ('go',         'block'),
    ('rust',       'block'),
    ('ruby',       'body_statement')
  ),
  -- (2) Tight-spacing tokens. language '*' = universal. Whitespace is the DEFAULT
  -- (a space always re-parses); a token may opt out of the space before and/or
  -- after it. OR rule: the space between A and B is dropped iff A.tight_after OR
  -- B.tight_before. Punctuation controls its own spacing regardless of neighbour.
  spacing(language, tag, tight_before, tight_after) AS (VALUES
    ('*', ',', true,  false),
    ('*', ';', true,  false),
    ('*', ':', true,  false),
    ('*', ')', true,  false),
    ('*', ']', true,  false),
    ('*', '}', true,  false),
    ('*', '(', false, true ),
    ('*', '[', false, true ),
    ('*', '{', false, true ),
    ('*', '.', true,  true )
  ),
  -- ================================================================================

  blk AS (
    SELECT a.file_path, a.node_id, a.descendant_count
    FROM ast a
    JOIN indent_blocks ib ON a.language = ib.language AND a.type = ib.tag
  ),
  leaves AS (
    SELECT
      l.file_path, l.language, l.node_id, l.start_line, l.end_line,
      l.type AS typ,                              -- tag, for the spacing lookup
      COALESCE(NULLIF(l.name, ''), l.type) AS tok, -- reconstructed leaf text
      (SELECT count(*) FROM blk b
       WHERE b.file_path = l.file_path
         AND b.node_id <= l.node_id
         AND l.node_id <= b.node_id + b.descendant_count) AS indent
    FROM ast l
    WHERE l.children_count = 0
  ),
  -- resolve each leaf's tight-before/after from the spacing table (language or '*')
  leaf_sp AS (
    SELECT l.*,
      COALESCE((SELECT bool_or(s.tight_before) FROM spacing s
                WHERE (s.language = l.language OR s.language = '*') AND s.tag = l.typ), false) AS tight_before,
      COALESCE((SELECT bool_or(s.tight_after)  FROM spacing s
                WHERE (s.language = l.language OR s.language = '*') AND s.tag = l.typ), false) AS tight_after
    FROM leaves l
  ),
  seq AS (
    SELECT *,
      row_number()      OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(end_line)     OVER (PARTITION BY file_path ORDER BY node_id) AS pel,
      LAG(tight_after)  OVER (PARTITION BY file_path ORDER BY node_id) AS prev_tight_after
    FROM leaf_sp
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL            THEN ''
        WHEN start_line > pel        THEN repeat(chr(10), start_line - pel) || repeat('    ', indent)
        WHEN prev_tight_after OR tight_before THEN ''   -- OR rule: either side drops the space
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);
