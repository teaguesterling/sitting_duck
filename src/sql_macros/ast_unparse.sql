-- ============================================================================
-- ast_unparse — reconstruct source text from a sitting_duck AST
-- ============================================================================
-- Rules-based unparser that reconstructs source from AST rows WITHOUT `peek`.
-- Guarantees pseudo-identity: parse(S) == parse(unparse(parse(S))).
--
-- Layout and spacing rules are loaded from ast_unparse_rules() by default,
-- which includes Tier-2 universal punctuation rules and Tier-3 language profiles.
-- Supports style presets (pep8/black, gofmt, prettier, llvm, google, rustfmt,
-- tabs, 2spaces, 4spaces) and custom user rule tables via ast_unparse_custom.
-- ============================================================================

CREATE OR REPLACE MACRO ast_unparse_from(ast_table, preset := '') AS TABLE (
  WITH ast AS (
    SELECT * FROM query_table(ast_table)
  ),
  rules AS (
    SELECT language, rule, target, int_arg, str_arg FROM ast_unparse_rules('', preset)
  ),
  tight_rules AS (
    SELECT
      language,
      target,
      bool_or(rule = 'TIGHT_BEFORE') AND NOT bool_or(rule = 'SPACE_BEFORE') AS tight_before,
      bool_or(rule = 'TIGHT_AFTER') AND NOT bool_or(rule = 'SPACE_AFTER') AS tight_after
    FROM rules
    WHERE rule IN ('TIGHT_BEFORE', 'TIGHT_AFTER', 'SPACE_BEFORE', 'SPACE_AFTER')
    GROUP BY language, target
  ),
  indent_rules AS (
    SELECT
      language,
      str_arg AS indent_str
    FROM (
      SELECT language, str_arg, row_number() OVER () AS rnum
      FROM rules
      WHERE rule = 'INDENT_STRING'
    )
    QUALIFY row_number() OVER (PARTITION BY language ORDER BY rnum DESC) = 1
  ),
  blk AS (
    SELECT a.file_path, a.node_id, a.descendant_count, r.int_arg AS indent_level
    FROM ast a
    JOIN rules r ON (a.language = r.language OR r.language = '*')
                AND a.type = r.target
                AND r.rule = 'INDENT_BLOCK'
  ),
  leaves AS (
    SELECT
      l.file_path, l.language, l.node_id, l.start_line, l.end_line,
      l.type AS typ,
      is_syntax_only(l.flags::UTINYINT) AS is_syntax,
      COALESCE(NULLIF(l.name, ''), l.type) AS tok,
      COALESCE((SELECT sum(b.indent_level)::BIGINT FROM blk b
                WHERE b.file_path = l.file_path
                  AND b.node_id <= l.node_id
                  AND l.node_id <= b.node_id + b.descendant_count), 0::BIGINT) AS indent
    FROM ast l
    WHERE l.children_count = 0
  ),
  leaf_sp AS (
    SELECT l.*,
      COALESCE(tb_lang.tight_before, tb_star.tight_before, false) AS tight_before,
      COALESCE(tb_lang.tight_after, tb_star.tight_after, false) AS tight_after,
      COALESCE(ir_lang.indent_str, ir_star.indent_str, '    ') AS indent_str
    FROM leaves l
    LEFT JOIN tight_rules tb_lang ON tb_lang.language = l.language AND tb_lang.target = l.typ
    LEFT JOIN tight_rules tb_star ON tb_star.language = '*' AND tb_star.target = l.typ
    LEFT JOIN indent_rules ir_lang ON ir_lang.language = l.language
    LEFT JOIN indent_rules ir_star ON ir_star.language = '*'
  ),
  seq AS (
    SELECT *,
      row_number()      OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(typ)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptyp,
      LAG(is_syntax)    OVER (PARTITION BY file_path ORDER BY node_id) AS prev_is_syntax,
      LAG(end_line)     OVER (PARTITION BY file_path ORDER BY node_id) AS pel,
      LAG(indent)       OVER (PARTITION BY file_path ORDER BY node_id) AS prev_indent,
      LAG(tight_after)  OVER (PARTITION BY file_path ORDER BY node_id) AS prev_tight_after,
      MAX(COALESCE(start_line, 0)) OVER (PARTITION BY file_path) = 0 AS is_synthetic
    FROM leaf_sp
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL            THEN ''
        WHEN is_synthetic AND (ptok IN ('{', ';') OR tok IN ('}') OR (ptok = ':' AND indent > prev_indent))
          THEN chr(10) || repeat(indent_str, CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN start_line > pel        THEN repeat(chr(10), (start_line - pel)::BIGINT) || repeat(indent_str, CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN prev_tight_after OR tight_before THEN ''
        WHEN ptok IN ('"', '''', '`') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptok IN ('"', '''', '`') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok = '${' THEN ''
        WHEN ptok = '}' AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN tok IN ('(', '[') AND (ptyp IN ('identifier', 'type_identifier', 'field_identifier', 'primitive_type') OR ptok IN (')', ']')) THEN ''
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);

CREATE OR REPLACE MACRO ast_unparse(path, preset := '') AS TABLE (
  WITH ast AS (
    SELECT * FROM read_ast(path)
  ),
  rules AS (
    SELECT language, rule, target, int_arg, str_arg FROM ast_unparse_rules('', preset)
  ),
  tight_rules AS (
    SELECT
      language,
      target,
      bool_or(rule = 'TIGHT_BEFORE') AND NOT bool_or(rule = 'SPACE_BEFORE') AS tight_before,
      bool_or(rule = 'TIGHT_AFTER') AND NOT bool_or(rule = 'SPACE_AFTER') AS tight_after
    FROM rules
    WHERE rule IN ('TIGHT_BEFORE', 'TIGHT_AFTER', 'SPACE_BEFORE', 'SPACE_AFTER')
    GROUP BY language, target
  ),
  indent_rules AS (
    SELECT
      language,
      str_arg AS indent_str
    FROM (
      SELECT language, str_arg, row_number() OVER () AS rnum
      FROM rules
      WHERE rule = 'INDENT_STRING'
    )
    QUALIFY row_number() OVER (PARTITION BY language ORDER BY rnum DESC) = 1
  ),
  blk AS (
    SELECT a.file_path, a.node_id, a.descendant_count, r.int_arg AS indent_level
    FROM ast a
    JOIN rules r ON (a.language = r.language OR r.language = '*')
                AND a.type = r.target
                AND r.rule = 'INDENT_BLOCK'
  ),
  leaves AS (
    SELECT
      l.file_path, l.language, l.node_id, l.start_line, l.end_line,
      l.type AS typ,
      is_syntax_only(l.flags::UTINYINT) AS is_syntax,
      COALESCE(NULLIF(l.name, ''), l.type) AS tok,
      COALESCE((SELECT sum(b.indent_level)::BIGINT FROM blk b
                WHERE b.file_path = l.file_path
                  AND b.node_id <= l.node_id
                  AND l.node_id <= b.node_id + b.descendant_count), 0::BIGINT) AS indent
    FROM ast l
    WHERE l.children_count = 0
  ),
  leaf_sp AS (
    SELECT l.*,
      COALESCE(tb_lang.tight_before, tb_star.tight_before, false) AS tight_before,
      COALESCE(tb_lang.tight_after, tb_star.tight_after, false) AS tight_after,
      COALESCE(ir_lang.indent_str, ir_star.indent_str, '    ') AS indent_str
    FROM leaves l
    LEFT JOIN tight_rules tb_lang ON tb_lang.language = l.language AND tb_lang.target = l.typ
    LEFT JOIN tight_rules tb_star ON tb_star.language = '*' AND tb_star.target = l.typ
    LEFT JOIN indent_rules ir_lang ON ir_lang.language = l.language
    LEFT JOIN indent_rules ir_star ON ir_star.language = '*'
  ),
  seq AS (
    SELECT *,
      row_number()      OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(typ)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptyp,
      LAG(is_syntax)    OVER (PARTITION BY file_path ORDER BY node_id) AS prev_is_syntax,
      LAG(end_line)     OVER (PARTITION BY file_path ORDER BY node_id) AS pel,
      LAG(indent)       OVER (PARTITION BY file_path ORDER BY node_id) AS prev_indent,
      LAG(tight_after)  OVER (PARTITION BY file_path ORDER BY node_id) AS prev_tight_after,
      MAX(COALESCE(start_line, 0)) OVER (PARTITION BY file_path) = 0 AS is_synthetic
    FROM leaf_sp
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL            THEN ''
        WHEN is_synthetic AND (ptok IN ('{', ';') OR tok IN ('}') OR (ptok = ':' AND indent > prev_indent))
          THEN chr(10) || repeat(indent_str, CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN start_line > pel        THEN repeat(chr(10), (start_line - pel)::BIGINT) || repeat(indent_str, CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN prev_tight_after OR tight_before THEN ''
        WHEN ptok IN ('"', '''', '`') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptok IN ('"', '''', '`') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok = '${' THEN ''
        WHEN ptok = '}' AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN tok IN ('(', '[') AND (ptyp IN ('identifier', 'type_identifier', 'field_identifier', 'primitive_type') OR ptok IN (')', ']')) THEN ''
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);

CREATE OR REPLACE MACRO ast_unparse_code(source_code, lang, preset := '') AS TABLE (
  WITH ast AS (
    SELECT * FROM parse_ast(source_code, lang)
  ),
  rules AS (
    SELECT language, rule, target, int_arg, str_arg FROM ast_unparse_rules(lang, preset)
  ),
  tight_rules AS (
    SELECT
      language,
      target,
      bool_or(rule = 'TIGHT_BEFORE') AND NOT bool_or(rule = 'SPACE_BEFORE') AS tight_before,
      bool_or(rule = 'TIGHT_AFTER') AND NOT bool_or(rule = 'SPACE_AFTER') AS tight_after
    FROM rules
    WHERE rule IN ('TIGHT_BEFORE', 'TIGHT_AFTER', 'SPACE_BEFORE', 'SPACE_AFTER')
    GROUP BY language, target
  ),
  indent_rules AS (
    SELECT
      language,
      str_arg AS indent_str
    FROM (
      SELECT language, str_arg, row_number() OVER () AS rnum
      FROM rules
      WHERE rule = 'INDENT_STRING'
    )
    QUALIFY row_number() OVER (PARTITION BY language ORDER BY rnum DESC) = 1
  ),
  blk AS (
    SELECT a.file_path, a.node_id, a.descendant_count, r.int_arg AS indent_level
    FROM ast a
    JOIN rules r ON (a.language = r.language OR r.language = '*')
                AND a.type = r.target
                AND r.rule = 'INDENT_BLOCK'
  ),
  leaves AS (
    SELECT
      l.file_path, l.language, l.node_id, l.start_line, l.end_line,
      l.type AS typ,
      is_syntax_only(l.flags::UTINYINT) AS is_syntax,
      COALESCE(NULLIF(l.name, ''), l.type) AS tok,
      COALESCE((SELECT sum(b.indent_level)::BIGINT FROM blk b
                WHERE b.file_path = l.file_path
                  AND b.node_id <= l.node_id
                  AND l.node_id <= b.node_id + b.descendant_count), 0::BIGINT) AS indent
    FROM ast l
    WHERE l.children_count = 0
  ),
  leaf_sp AS (
    SELECT l.*,
      COALESCE(tb_lang.tight_before, tb_star.tight_before, false) AS tight_before,
      COALESCE(tb_lang.tight_after, tb_star.tight_after, false) AS tight_after,
      COALESCE(ir_lang.indent_str, ir_star.indent_str, '    ') AS indent_str
    FROM leaves l
    LEFT JOIN tight_rules tb_lang ON tb_lang.language = l.language AND tb_lang.target = l.typ
    LEFT JOIN tight_rules tb_star ON tb_star.language = '*' AND tb_star.target = l.typ
    LEFT JOIN indent_rules ir_lang ON ir_lang.language = l.language
    LEFT JOIN indent_rules ir_star ON ir_star.language = '*'
  ),
  seq AS (
    SELECT *,
      row_number()      OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(typ)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptyp,
      LAG(is_syntax)    OVER (PARTITION BY file_path ORDER BY node_id) AS prev_is_syntax,
      LAG(end_line)     OVER (PARTITION BY file_path ORDER BY node_id) AS pel,
      LAG(indent)       OVER (PARTITION BY file_path ORDER BY node_id) AS prev_indent,
      LAG(tight_after)  OVER (PARTITION BY file_path ORDER BY node_id) AS prev_tight_after,
      MAX(COALESCE(start_line, 0)) OVER (PARTITION BY file_path) = 0 AS is_synthetic
    FROM leaf_sp
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL            THEN ''
        WHEN is_synthetic AND (ptok IN ('{', ';') OR tok IN ('}') OR (ptok = ':' AND indent > prev_indent))
          THEN chr(10) || repeat(indent_str, CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN start_line > pel        THEN repeat(chr(10), (start_line - pel)::BIGINT) || repeat(indent_str, CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN prev_tight_after OR tight_before THEN ''
        WHEN ptok IN ('"', '''', '`') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptok IN ('"', '''', '`') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok = '${' THEN ''
        WHEN ptok = '}' AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN tok IN ('(', '[') AND (ptyp IN ('identifier', 'type_identifier', 'field_identifier', 'primitive_type') OR ptok IN (')', ']')) THEN ''
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);

CREATE OR REPLACE MACRO ast_unparse_custom(ast_table, rules_table) AS TABLE (
  WITH ast AS (
    SELECT * FROM query_table(ast_table)
  ),
  rules AS (
    SELECT language, rule, target, int_arg, str_arg FROM query_table(rules_table)
  ),
  tight_rules AS (
    SELECT
      language,
      target,
      bool_or(rule = 'TIGHT_BEFORE') AND NOT bool_or(rule = 'SPACE_BEFORE') AS tight_before,
      bool_or(rule = 'TIGHT_AFTER') AND NOT bool_or(rule = 'SPACE_AFTER') AS tight_after
    FROM rules
    WHERE rule IN ('TIGHT_BEFORE', 'TIGHT_AFTER', 'SPACE_BEFORE', 'SPACE_AFTER')
    GROUP BY language, target
  ),
  indent_rules AS (
    SELECT
      language,
      str_arg AS indent_str
    FROM (
      SELECT language, str_arg, row_number() OVER () AS rnum
      FROM rules
      WHERE rule = 'INDENT_STRING'
    )
    QUALIFY row_number() OVER (PARTITION BY language ORDER BY rnum DESC) = 1
  ),
  blk AS (
    SELECT a.file_path, a.node_id, a.descendant_count, r.int_arg AS indent_level
    FROM ast a
    JOIN rules r ON (a.language = r.language OR r.language = '*')
                AND a.type = r.target
                AND r.rule = 'INDENT_BLOCK'
  ),
  leaves AS (
    SELECT
      l.file_path, l.language, l.node_id, l.start_line, l.end_line,
      l.type AS typ,
      is_syntax_only(l.flags::UTINYINT) AS is_syntax,
      COALESCE(NULLIF(l.name, ''), l.type) AS tok,
      COALESCE((SELECT sum(b.indent_level)::BIGINT FROM blk b
                WHERE b.file_path = l.file_path
                  AND b.node_id <= l.node_id
                  AND l.node_id <= b.node_id + b.descendant_count), 0::BIGINT) AS indent
    FROM ast l
    WHERE l.children_count = 0
  ),
  leaf_sp AS (
    SELECT l.*,
      COALESCE(tb_lang.tight_before, tb_star.tight_before, false) AS tight_before,
      COALESCE(tb_lang.tight_after, tb_star.tight_after, false) AS tight_after,
      COALESCE(ir_lang.indent_str, ir_star.indent_str, '    ') AS indent_str
    FROM leaves l
    LEFT JOIN tight_rules tb_lang ON tb_lang.language = l.language AND tb_lang.target = l.typ
    LEFT JOIN tight_rules tb_star ON tb_star.language = '*' AND tb_star.target = l.typ
    LEFT JOIN indent_rules ir_lang ON ir_lang.language = l.language
    LEFT JOIN indent_rules ir_star ON ir_star.language = '*'
  ),
  seq AS (
    SELECT *,
      row_number()      OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(typ)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptyp,
      LAG(is_syntax)    OVER (PARTITION BY file_path ORDER BY node_id) AS prev_is_syntax,
      LAG(end_line)     OVER (PARTITION BY file_path ORDER BY node_id) AS pel,
      LAG(indent)       OVER (PARTITION BY file_path ORDER BY node_id) AS prev_indent,
      LAG(tight_after)  OVER (PARTITION BY file_path ORDER BY node_id) AS prev_tight_after,
      MAX(COALESCE(start_line, 0)) OVER (PARTITION BY file_path) = 0 AS is_synthetic
    FROM leaf_sp
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL            THEN ''
        WHEN is_synthetic AND (ptok IN ('{', ';') OR tok IN ('}') OR (ptok = ':' AND indent > prev_indent))
          THEN chr(10) || repeat(indent_str, CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN start_line > pel        THEN repeat(chr(10), (start_line - pel)::BIGINT) || repeat(indent_str, CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN prev_tight_after OR tight_before THEN ''
        WHEN ptok IN ('"', '''', '`') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptok IN ('"', '''', '`') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok = '${' THEN ''
        WHEN ptok = '}' AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN tok IN ('(', '[') AND (ptyp IN ('identifier', 'type_identifier', 'field_identifier', 'primitive_type') OR ptok IN (')', ']')) THEN ''
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);

-- ============================================================================
-- Byte-exact unparse — ast_unparse_exact* (tracker 047 "4b", first half)
-- ============================================================================
-- The macros above are the RULES-BASED unparser. They reconstruct source from
-- node types and names alone and deliberately never read a position column, so
-- they satisfy only the structural law
--
--     read_ast(write_ast(read_ast(x))) = read_ast(x)
--
-- and normalise inter-token whitespace. That is conformant, not a bug (#89):
-- `write_ast(read_ast(x)) = x` is explicitly NOT required below
-- `source := 'full'`. Nothing below changes them.
--
-- The macros in THIS section implement the other, stronger law, the one that
-- `source := 'full'` buys (docs/planning/v2-architecture.md, "write_ast laws",
-- settled 2026-10-07):
--
--     write_ast(read_ast(x, source := 'full')) = x          -- byte for byte
--
-- HOW: A SPLICE, NOT A RECONSTRUCTION
-- -----------------------------------
-- Nothing is reconstructed from node types. The output is assembled from byte
-- ranges of the original bytes, in one ordered pass over the tree's LEAF
-- FRONTIER (the rows with no descendants — an antichain, so their spans are
-- disjoint):
--
--     for each leaf, in (start_byte, end_byte, node_id) order:
--         emit bytes [previous leaf's end_byte, this leaf's start_byte)   -- the GAP
--         emit bytes [this leaf's start_byte,   this leaf's end_byte)     -- the LEAF
--     emit bytes [last leaf's end_byte, EOF)                             -- the TAIL
--
-- Two consequences are worth stating because they are what this formulation
-- buys over the obvious alternative ("slice the root's whole span"):
--
--  1. It is a TILING PROOF, not just an equality. Every byte of the file is
--     emitted exactly once, attributed either to a leaf or to a gap. A whole-
--     span slice of the root would also be byte-exact for an unmodified tree
--     while proving nothing at all about the tree, and would not generalise.
--  2. It is the shape a MODIFIED tree needs. Replace one leaf's slice with new
--     text and everything around it still comes from the original bytes — the
--     "untouched subtrees keep their text" splice the v2 RFC describes. (Doing
--     that well — layout for SYNTHESIZED nodes — is the second half of 047's
--     4b and is NOT implemented here.)
--
-- GAPS ARE COMPUTED FROM OFFSETS, NOT FROM GRAMMAR, which dissolves a limit
-- 047 recorded as structural. tree-sitter-kotlin's `string_literal` has no
-- child node for its quote characters, so leaf CONCATENATION cannot recover
-- them. Here those bytes are simply not inside any leaf, so they fall into the
-- gap before the first inner leaf and are emitted verbatim. The same holds for
-- every other hidden delimiter, for inter-token whitespace, and for leading
-- and trailing whitespace outside the root's own span. No per-language
-- knowledge is involved at any point: the splice is language-agnostic, exactly
-- as the RFC says the engine-owned textual law should be.
--
-- WHERE THE BYTES COME FROM
-- -------------------------
-- No extraction configuration retains per-node source text (v2 RFC, "Substrate
-- gap"), and `peek` is presentation, never a correctness substrate. So the
-- bytes are re-read: from the file for `ast_unparse_exact` /
-- `ast_unparse_exact_from`, and from the string argument itself for
-- `ast_unparse_exact_code`. The stated law takes a PATH, and a path is
-- re-readable; per-node retention is only needed for tables that outlive their
-- files, and is not attempted here.
--
-- HONEST FAILURE (#89) — every one of these ERRORS, none falls back to the
-- normalising unparser, because silently returning normalised text where
-- byte-exact text was asked for is the one outcome worse than no answer:
--
--   * the table carries more than one `language`            -> error
--   * the table carries more than one `file_path`           -> error
--   * `start_byte`/`end_byte` are NULL (parsed below 'full')-> error
--     (parsed below 'full' WITHOUT `+schema` the columns do not exist at all,
--      and the macro fails to bind — also an error, with DuckDB's message)
--   * no root row, or more than one                         -> error
--   * the table is not a complete tree (a filtered subset)  -> error
--   * the file is not readable via the `files` argument     -> error
--   * the file's length no longer matches the parse         -> error (staleness)
--   * any `end_byte` past end of file                       -> error (staleness)
--   * leaf spans overlap                                    -> error
--
-- STALENESS, AND WHAT IT CANNOT CATCH. The guard is
-- `root.end_byte = octet_length(file)`: tree-sitter's root node ends at EOF
-- (measured for all 26 tree-sitter languages, including empty, whitespace-only
-- and no-trailing-newline files), so any change to the file's LENGTH is
-- detected and errors. A same-length edit is invisible, exactly as for
-- `ast_patch` — parse and unparse in one motion; do not cache a node table
-- across a file change. `ast_unparse_exact(path)` does both in one statement
-- for precisely this reason.
--
-- NOT SERVED: input that is not valid UTF-8. `source` is VARCHAR and DuckDB
-- requires VARCHAR to be valid UTF-8, so such a file cannot be returned
-- byte-exactly at all and decode() raises a conversion error. Honest failure,
-- not corruption — and worth contrasting: read_ast PARSES such a file happily
-- (tree-sitter works on bytes) and the rules-based ast_unparse returns a string
-- for it, having silently dropped the offending bytes. Serving it would need a
-- BLOB-returning variant.
--
-- NOT SERVED: the `duckdb` language adapter, for two reasons that both
-- outlive #197. (#197's constant-zero children_count / sibling_index / depth
-- were fixed by PR #205, so the tree structure is now correct and the
-- single-root guard no longer fires — the premise changed, the conclusion did
-- not.) Re-measured against #205 on an 84-byte, 7-statement SQL file:
--   * it reports start_byte = end_byte = 0 for EVERY node, and an empty
--     file_path, so there is nothing to slice and nothing to re-read; and
--   * its tree is an AST, not a CST. 15 nodes for that file, with no node for
--     any keyword, punctuation or comment, and ORDER BY / LIMIT absent
--     entirely. No leaf frontier over such a tree could tile the text, so byte
--     offsets alone would not make it serveable.
-- Both conditions error (a dedicated guard reports the zero offsets rather
-- than letting the readability or staleness guard misdiagnose it). Byte
-- offsets are a tree-sitter-backed property.
--
-- MULTI-FILE IS AN ERROR HERE, unlike `ast_unparse(glob)` which returns one
-- row per file. That is deliberate, not an oversight: the textual law is
-- stated over a single `x`, and the same writer has to serve the COPY sink
-- (045/#174) where a single destination means a single textual answer. Scope
-- to one file, or pass `file_path := '<path>'`.
-- ============================================================================

-- ----------------------------------------------------------------------------
-- ast_unparse_exact_splice — the shared core. You normally want
-- `ast_unparse_exact(path)`; this is exposed because it is the one place the
-- splice lives, and because it is useful on its own.
--
--   ast_table  — name of a relation (table or CTE, resolved via query_table())
--                holding UNFILTERED `read_ast(..., source := 'full')` output.
--                Must expose file_path, language, node_id, depth,
--                descendant_count, start_byte, end_byte.
--   blob_table — name of a relation exposing exactly two columns:
--                  fp    VARCHAR -- file path, matching ast_table.file_path
--                                -- (matched separator- and './'-insensitively;
--                                --  see PATH MATCHING below)
--                  cblob BLOB    -- that file's pristine bytes
--                A relation rather than a path because DuckDB table functions
--                (read_blob) accept only literal arguments — no per-row
--                lateral paths. The same constraint `ast_patch` documents.
--
--                THIS ARGUMENT IS TRUSTED. The macro verifies that the bytes
--                are the right LENGTH for the parse (the staleness guard) but
--                cannot verify they are that path's bytes at all: hand it a
--                same-length blob from somewhere else and it will splice that,
--                which is exactly what test/sql/ast_unparse_exact.test §5g
--                does on purpose to make the guard fire. `ast_unparse_exact`
--                and `ast_unparse_exact_from` exist because they establish the
--                bytes themselves from a path; prefer them.
--   language   — optional override / disambiguator (the law: inferred from the
--                data, never required). Filters rows to that language.
--   file_path  — optional scoping disambiguator for a multi-file table.
--                Matched separator-insensitively, so the forward-slash spelling
--                you handed read_ast works on Windows too.
--
-- Returns (file_path, source), one row. An empty ast_table yields no rows.
--
-- PATH MATCHING. Three relations have to agree on one join key: the node
-- table's `file_path`, the blob relation's `fp`, and the `file_path :=`
-- argument. They can disagree on spelling without disagreeing on the file:
--
--   * DuckDB's globber returns './'-prefixed relative paths while an exact
--     path passes through verbatim, so one file has two spellings;
--   * ON WINDOWS the globber returns NATIVE separators — a table built from
--     read_ast('test/data/*') carries `test\data\x.py` — while an exact path
--     and any hand-written `file_path :=` keep their forward slashes. Mixing a
--     globbed node table with an exact `files` path therefore made the blob
--     join miss on Windows and the macro report the file as unreadable, which
--     was a wrong diagnosis of a correct input.
--
-- So all three are normalised for MATCHING only: leading './' stripped,
-- backslashes folded to forward slashes. The `file_path` COLUMN RETURNED is
-- the spelling your node table carried, untouched, so it still joins back to
-- that table. (A path that genuinely contains a backslash — legal on POSIX,
-- pathological — would be folded together with its forward-slash twin. The
-- same trade `ast_patch` makes for './'.)
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_unparse_exact_splice(ast_table, blob_table,
                                                 language := NULL,
                                                 file_path := NULL) AS TABLE
    WITH
        -- Rename the two columns whose names collide with this macro's own
        -- parameters BEFORE any filtering, in a CTE that references nothing
        -- unqualified. A bare `language` in a scope that also has a `language`
        -- column would bind to the column and turn the filter into a silent
        -- no-op; after this CTE no such column exists, so `language` and
        -- `file_path` can only mean the parameters.
        --
        -- `fp` is the normalised MATCHING key (see PATH MATCHING above);
        -- `fp_raw` is the spelling the node table carried, which is what gets
        -- returned and what error messages name.
        __sdx_all AS (
            SELECT replace(regexp_replace(t.file_path, '^\./', ''), '\', '/') AS fp,
                   t.file_path                             AS fp_raw,
                   t.language                              AS node_language,
                   t.node_id                               AS node_id,
                   t.depth                                 AS depth,
                   t.descendant_count                      AS descendant_count,
                   t.start_byte                            AS start_byte,
                   t.end_byte                              AS end_byte
            FROM (SELECT * FROM query_table(ast_table)) t
        ),
        __sdx_nodes AS (
            SELECT * FROM __sdx_all
            WHERE (language  IS NULL OR node_language = language)
              AND (file_path IS NULL
                   OR fp = replace(regexp_replace(file_path, '^\./', ''), '\', '/'))
        ),
        __sdx_bytes AS (
            SELECT DISTINCT ON (fp) *
            FROM (SELECT replace(regexp_replace(b.fp, '^\./', ''), '\', '/') AS fp,
                         b.cblob                           AS cblob,
                         octet_length(b.cblob)             AS nbytes
                  FROM (SELECT * FROM query_table(blob_table)) b)
            WHERE fp IN (SELECT fp FROM __sdx_nodes WHERE fp IS NOT NULL)
        ),
        __sdx_root AS (
            SELECT fp, start_byte AS root_start, end_byte AS root_end,
                   descendant_count AS root_dc
            FROM __sdx_nodes WHERE depth = 0
        ),
        -- The LEAF FRONTIER. `descendant_count = 0` rather than
        -- `children_count = 0`: it is definitionally "no descendant rows
        -- exist", which is the antichain property the splice needs, and it is
        -- immune to an adapter that miscounts children (#197). Measured to
        -- select exactly the same rows as `children_count = 0` on all 26
        -- tree-sitter languages.
        __sdx_leaves AS (
            SELECT fp, fp_raw, node_id, start_byte, end_byte,
                   COALESCE(lag(end_byte) OVER (PARTITION BY fp
                       ORDER BY start_byte, end_byte, node_id), 0) AS prev_end
            FROM __sdx_nodes
            WHERE descendant_count = 0
        ),
        __sdx_stats AS (
            SELECT count(*)                      AS n_rows,
                   count(DISTINCT fp)            AS n_files,
                   count(DISTINCT node_language) AS n_langs,
                   count(*) FILTER (WHERE start_byte IS NULL OR end_byte IS NULL) AS n_null_pos,
                   count(*) FILTER (WHERE depth = 0)  AS n_roots,
                   count(*) FILTER (WHERE fp IS NULL) AS n_null_fp,
                   count(*) FILTER (WHERE fp = '')    AS n_empty_fp,
                   min(node_language) AS lang_lo, max(node_language) AS lang_hi,
                   -- the node table's own spelling, so a message names the path
                   -- the caller wrote rather than a normalised rewrite of it
                   min(fp_raw) AS fp_lo, max(fp_raw) AS fp_hi,
                   max(end_byte) AS max_end
            FROM __sdx_nodes
        ),
        -- One CASE, in priority order, so that when several things are wrong
        -- the message is deterministic (independent CTEs have no guaranteed
        -- evaluation order).
        --
        -- Every scalar subquery below is wrapped in an AGGREGATE even where a
        -- single row is expected. CASE short-circuits its *result* expressions,
        -- but a scalar subquery is materialized by the planner whether or not
        -- its branch is taken — so a bare `(SELECT nbytes FROM __sdx_bytes)`
        -- on a two-file table raises DuckDB's own "more than one row returned
        -- by a subquery" before the n_files branch can produce the message
        -- that actually explains the problem. max() keeps every probe
        -- single-row so the diagnosis, not the symptom, is what surfaces.
        __sdx_validate AS (
            SELECT CASE
                WHEN s.n_langs > 1
                    THEN error('ast_unparse_exact: the node table carries ' ||
                               s.n_langs::VARCHAR || ' distinct languages (including ''' ||
                               s.lang_lo || ''' and ''' || s.lang_hi || '''), so there is no ' ||
                               'single textual answer. Scope the table to one language, or ' ||
                               'pass language := ''<lang>'' to disambiguate.')
                WHEN s.n_files > 1
                    THEN error('ast_unparse_exact: the node table carries ' ||
                               s.n_files::VARCHAR || ' distinct file_paths (including ''' ||
                               s.fp_lo || ''' and ''' || s.fp_hi || '''), so there is no ' ||
                               'single textual answer. Scope the table to one file, or pass ' ||
                               'file_path := ''<path>'' to disambiguate. (ast_unparse(glob) ' ||
                               'returns one row per file; the byte-exact law is stated over ' ||
                               'a single input.)')
                -- A parser that reports no byte positions at all. Only a
                -- ZERO-BYTE file can legitimately have max(end_byte) = 0, and
                -- that file has exactly one node, so the n_rows > 1 conjunct
                -- makes this unambiguous. Checked BEFORE the readability and
                -- staleness guards, both of which would otherwise fire first
                -- and misdiagnose it as a missing or changed file.
                WHEN s.n_rows > 1 AND s.max_end = 0
                    THEN error('ast_unparse_exact: every node reports ' ||
                               'start_byte = end_byte = 0, so this parser surfaces no byte ' ||
                               'positions and there is nothing to slice. Byte offsets are a ' ||
                               'tree-sitter-backed property; the `duckdb` adapter wraps ' ||
                               'DuckDB''s own parser and has none (and its tree is an AST, ' ||
                               'not a CST — no keyword, punctuation or comment nodes — so ' ||
                               'even with offsets its leaves could not cover the text). ' ||
                               'Parse with a tree-sitter language.')
                WHEN s.n_null_fp > 0
                    THEN error('ast_unparse_exact: ' || s.n_null_fp::VARCHAR || ' node rows ' ||
                               'have a NULL file_path, so their bytes cannot be located.')
                WHEN s.n_empty_fp > 0
                    THEN error('ast_unparse_exact: ' || s.n_empty_fp::VARCHAR || ' node rows ' ||
                               'have an empty file_path, so their bytes cannot be located.')
                WHEN s.n_null_pos > 0
                    THEN error('ast_unparse_exact: ' || s.n_null_pos::VARCHAR || ' node rows ' ||
                               'have a NULL start_byte/end_byte. Byte offsets exist only under ' ||
                               'source := ''full'' (a ''<level>+schema'' parse declares the ' ||
                               'columns but fills them with NULL). Re-parse with ' ||
                               'read_ast(..., source := ''full'').')
                WHEN s.n_rows > 0 AND s.n_roots <> 1
                    THEN error('ast_unparse_exact: expected exactly one root row (depth = 0), ' ||
                               'found ' || s.n_roots::VARCHAR || '. Byte-exact unparse ' ||
                               'reproduces one whole parse; pass unfiltered ' ||
                               'read_ast(..., source := ''full'') output.')
                WHEN s.n_rows > 0 AND s.n_rows <> (SELECT max(root_dc) FROM __sdx_root) + 1
                    THEN error('ast_unparse_exact: the node table is not a complete tree — ' ||
                               s.n_rows::VARCHAR || ' rows, but the root reports ' ||
                               ((SELECT max(root_dc) FROM __sdx_root) + 1)::VARCHAR ||
                               ' nodes. A filtered subset has no byte-exact answer (its ' ||
                               'missing nodes would be silently absorbed into the gaps and ' ||
                               'the output would look right while proving nothing). Pass ' ||
                               'unfiltered read_ast(..., source := ''full'') output.')
                WHEN s.n_rows > 0 AND (SELECT count(*) FROM __sdx_bytes) = 0
                              AND s.fp_lo = '<inline>'
                    THEN error('ast_unparse_exact: the node table is in-memory parse_ast() ' ||
                               'output (file_path = ''<inline>''), which has no file to ' ||
                               're-read. Use ast_unparse_exact_code(source_code, language) to ' ||
                               'splice against the string itself, or parse from a file with ' ||
                               'read_ast(path, source := ''full'').')
                WHEN s.n_rows > 0 AND (SELECT count(*) FROM __sdx_bytes) = 0
                    THEN error('ast_unparse_exact: ''' || s.fp_lo || ''' was not readable as ' ||
                               'bytes — pass the same path/glob used for read_ast. Byte-exact ' ||
                               'unparse needs the original bytes; there is no fallback to the ' ||
                               'normalising unparser, because returning normalised text where ' ||
                               'byte-exact text was asked for would be a silent wrong answer.')
                WHEN s.n_rows > 0 AND (SELECT max(root_end) FROM __sdx_root)
                                      <> (SELECT max(nbytes) FROM __sdx_bytes)
                    THEN error('ast_unparse_exact: ''' || s.fp_lo || ''' is now ' ||
                               (SELECT max(nbytes) FROM __sdx_bytes)::VARCHAR || ' bytes but the ' ||
                               'parse covers ' || (SELECT max(root_end) FROM __sdx_root)::VARCHAR ||
                               ' — the file changed since it was parsed, so any output would ' ||
                               'be stale text dressed up as a round trip. Re-parse and ' ||
                               'unparse in one statement (ast_unparse_exact(path) does).')
                WHEN s.n_rows > 0 AND s.max_end > (SELECT max(nbytes) FROM __sdx_bytes)
                    THEN error('ast_unparse_exact: a node ends at byte ' ||
                               s.max_end::VARCHAR || ' but ''' || s.fp_lo || ''' is only ' ||
                               (SELECT max(nbytes) FROM __sdx_bytes)::VARCHAR || ' bytes — the ' ||
                               'file changed since it was parsed. Re-parse and unparse in ' ||
                               'one statement.')
                WHEN EXISTS (SELECT 1 FROM __sdx_leaves WHERE start_byte < prev_end)
                    THEN error('ast_unparse_exact: leaf spans overlap in ''' || s.fp_lo ||
                               ''' (a leaf starts before the previous leaf ends), so the ' ||
                               'frontier is not an antichain and a splice would duplicate or ' ||
                               'drop bytes. An adapter reporting children_count = 0 for every ' ||
                               'node produces exactly this — which is what the `duckdb` ' ||
                               'adapter did before PR #205 fixed it (issue #197).')
                ELSE true
            END AS ok
            FROM __sdx_stats s
        ),
        -- Reassembly: one ordered pass, gap then leaf, over disjoint ranges.
        -- Slicing is `blob[i:j]` on a BLOB, which is 1-based inclusive and
        -- BYTE-indexed (the same primitive ast_patch uses, and unicode-safe
        -- for the same reason). NOT substring(): on VARCHAR that counts
        -- CHARACTERS, and DuckDB v1.5.6 has no substring(BLOB, ...) overload,
        -- so the obvious spelling is silently wrong on multi-byte input.
        -- (API_REFERENCE documents a from_hex(substring(to_hex(...))) spelling
        -- for the same job; the two were verified to agree byte for byte on
        -- the CRLF + multi-byte fixtures. BLOB slicing is used here because it
        -- is O(slice) rather than O(file) per slice.)
        --
        -- decode() is safe on every piece: both ends of every gap and every
        -- leaf are node boundaries, hence character boundaries. A boundary
        -- that split a UTF-8 sequence (only reachable from a stale or
        -- hand-built table) fails decode() loudly rather than corrupting
        -- output.
        __sdx_assembled AS (
            SELECT l.fp,
                   -- min() rather than any_value() so the returned spelling is
                   -- deterministic if one file somehow arrived under two
                   -- spellings (it cannot from a single read_ast call).
                   min(l.fp_raw) AS fp_raw,
                   string_agg(decode(b.cblob[l.prev_end   + 1 : l.start_byte]) ||
                              decode(b.cblob[l.start_byte + 1 : l.end_byte]),
                              '' ORDER BY l.start_byte, l.end_byte, l.node_id) AS head,
                   max(l.end_byte) AS last_end
            FROM __sdx_leaves l JOIN __sdx_bytes b USING (fp)
            GROUP BY l.fp
        )
    SELECT a.fp_raw AS file_path,
           a.head || decode(b.cblob[a.last_end + 1 : ]) AS source
    FROM __sdx_assembled a
    JOIN __sdx_bytes b USING (fp)
    WHERE (SELECT ok FROM __sdx_validate)
    ORDER BY a.fp;

-- ----------------------------------------------------------------------------
-- ast_unparse_exact(path, language := NULL) — the law, in one statement.
--
--     SELECT source FROM ast_unparse_exact('src/main.py');
--
-- Parses with source := 'full' and re-reads the bytes in the SAME statement,
-- which is the narrowest possible staleness window. `language` is an override;
-- omitted, it is detected from the path exactly as read_ast does.
-- A glob matching more than one file errors (see the section header).
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_unparse_exact(path, language := NULL) AS TABLE
    WITH __sdx_src AS (
        SELECT * FROM read_ast(path, language, source := 'full', peek := 'none')
    ),
    __sdx_file AS (
        SELECT filename AS fp, content AS cblob FROM read_blob(path)
    )
    SELECT * FROM ast_unparse_exact_splice('__sdx_src', '__sdx_file');

-- ----------------------------------------------------------------------------
-- ast_unparse_exact_from(ast_table, files, language := NULL, file_path := NULL)
--
-- For a node table you already have. `files` is a path/glob/list covering the
-- file, passed to read_blob() — required because DuckDB table functions take
-- only literal arguments (no per-row lateral paths); pass the same path used
-- for read_ast. Beware the staleness window this opens: the file is read now,
-- the table was parsed whenever it was parsed. The length guard catches a
-- change of size and nothing else.
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_unparse_exact_from(ast_table, files,
                                               language := NULL,
                                               file_path := NULL) AS TABLE
    WITH __sdx_from_file AS (
        SELECT filename AS fp, content AS cblob FROM read_blob(files)
    )
    SELECT * FROM ast_unparse_exact_splice(ast_table, '__sdx_from_file',
                                           language := language,
                                           file_path := file_path);

-- ----------------------------------------------------------------------------
-- ast_unparse_exact_code(source_code, language) — byte-exact for an in-memory
-- parse. There is no file and therefore no staleness: the bytes spliced
-- against are the string argument itself.
--
--     SELECT source = $code FROM ast_unparse_exact_code($code, 'python');
--
-- This goes beyond the law as stated (which is path-scoped) and is the cheapest
-- way to test it: `ast_unparse_exact_code(s, l) = s` for any s that parses.
-- ----------------------------------------------------------------------------
CREATE OR REPLACE MACRO ast_unparse_exact_code(source_code, language) AS TABLE
    WITH __sdx_code_src AS (
        SELECT * FROM parse_ast(source_code, language, source := 'full', peek := 'none')
    ),
    __sdx_code_file AS (
        SELECT '<inline>' AS fp, encode(source_code) AS cblob
    )
    SELECT * FROM ast_unparse_exact_splice('__sdx_code_src', '__sdx_code_file');
