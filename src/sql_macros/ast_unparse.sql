-- ============================================================================
-- ast_unparse — reconstruct source text from a sitting_duck AST
-- ============================================================================
-- Rules-based unparser that reconstructs source from AST rows WITHOUT `peek`.
-- Guarantees pseudo-identity: parse(S) == parse(unparse(parse(S))).
--
-- Layout and spacing rules are loaded from ast_unparse_rules() by default,
-- which includes Tier-2 universal punctuation rules and Tier-3 language profiles.
-- ============================================================================

CREATE OR REPLACE MACRO ast_unparse_from(ast_table) AS TABLE (
  WITH ast AS (
    SELECT * FROM query_table(ast_table)
  ),
  rules AS (
    SELECT language, rule, target, int_arg, str_arg FROM ast_unparse_rules()
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
      is_syntax_only(l.flags) AS is_syntax,
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
      COALESCE((SELECT bool_or(r.rule = 'TIGHT_BEFORE') FROM rules r
                WHERE (r.language = l.language OR r.language = '*') AND r.target = l.typ), false) AS tight_before,
      COALESCE((SELECT bool_or(r.rule = 'TIGHT_AFTER')  FROM rules r
                WHERE (r.language = l.language OR r.language = '*') AND r.target = l.typ), false) AS tight_after
    FROM leaves l
  ),
  seq AS (
    SELECT *,
      row_number()      OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(typ)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptyp,
      LAG(is_syntax)    OVER (PARTITION BY file_path ORDER BY node_id) AS prev_is_syntax,
      LAG(end_line)     OVER (PARTITION BY file_path ORDER BY node_id) AS pel,
      LAG(tight_after)  OVER (PARTITION BY file_path ORDER BY node_id) AS prev_tight_after
    FROM leaf_sp
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL            THEN ''
        WHEN start_line > pel        THEN repeat(chr(10), (start_line - pel)::BIGINT) || repeat('    ', CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN prev_tight_after OR tight_before THEN ''
        WHEN ptok IN ('"', '''', '`') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptok IN ('"', '''', '`') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok = '${' THEN ''
        WHEN ptok = '}' AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN tok IN ('(', '[') AND (ptyp = 'identifier' OR ptok IN (')', ']')) THEN ''
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);

CREATE OR REPLACE MACRO ast_unparse(path) AS TABLE (
  WITH ast AS (
    SELECT * FROM read_ast(path)
  ),
  rules AS (
    SELECT language, rule, target, int_arg, str_arg FROM ast_unparse_rules()
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
      is_syntax_only(l.flags) AS is_syntax,
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
      COALESCE((SELECT bool_or(r.rule = 'TIGHT_BEFORE') FROM rules r
                WHERE (r.language = l.language OR r.language = '*') AND r.target = l.typ), false) AS tight_before,
      COALESCE((SELECT bool_or(r.rule = 'TIGHT_AFTER')  FROM rules r
                WHERE (r.language = l.language OR r.language = '*') AND r.target = l.typ), false) AS tight_after
    FROM leaves l
  ),
  seq AS (
    SELECT *,
      row_number()      OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(typ)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptyp,
      LAG(is_syntax)    OVER (PARTITION BY file_path ORDER BY node_id) AS prev_is_syntax,
      LAG(end_line)     OVER (PARTITION BY file_path ORDER BY node_id) AS pel,
      LAG(tight_after)  OVER (PARTITION BY file_path ORDER BY node_id) AS prev_tight_after
    FROM leaf_sp
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL            THEN ''
        WHEN start_line > pel        THEN repeat(chr(10), (start_line - pel)::BIGINT) || repeat('    ', CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN prev_tight_after OR tight_before THEN ''
        WHEN ptok IN ('"', '''', '`') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptok IN ('"', '''', '`') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok = '${' THEN ''
        WHEN ptok = '}' AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN tok IN ('(', '[') AND (ptyp = 'identifier' OR ptok IN (')', ']')) THEN ''
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);

CREATE OR REPLACE MACRO ast_unparse_code(source_code, lang) AS TABLE (
  WITH ast AS (
    SELECT * FROM parse_ast(source_code, lang)
  ),
  rules AS (
    SELECT language, rule, target, int_arg, str_arg FROM ast_unparse_rules()
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
      is_syntax_only(l.flags) AS is_syntax,
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
      COALESCE((SELECT bool_or(r.rule = 'TIGHT_BEFORE') FROM rules r
                WHERE (r.language = l.language OR r.language = '*') AND r.target = l.typ), false) AS tight_before,
      COALESCE((SELECT bool_or(r.rule = 'TIGHT_AFTER')  FROM rules r
                WHERE (r.language = l.language OR r.language = '*') AND r.target = l.typ), false) AS tight_after
    FROM leaves l
  ),
  seq AS (
    SELECT *,
      row_number()      OVER (PARTITION BY file_path ORDER BY node_id) AS rn,
      LAG(tok)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptok,
      LAG(typ)          OVER (PARTITION BY file_path ORDER BY node_id) AS ptyp,
      LAG(is_syntax)    OVER (PARTITION BY file_path ORDER BY node_id) AS prev_is_syntax,
      LAG(end_line)     OVER (PARTITION BY file_path ORDER BY node_id) AS pel,
      LAG(tight_after)  OVER (PARTITION BY file_path ORDER BY node_id) AS prev_tight_after
    FROM leaf_sp
  )
  SELECT
    file_path,
    string_agg(
      CASE
        WHEN ptok IS NULL            THEN ''
        WHEN start_line > pel        THEN repeat(chr(10), (start_line - pel)::BIGINT) || repeat('    ', CASE WHEN tok IN ('}', ']', ')') THEN GREATEST(0, indent - 1)::BIGINT ELSE indent::BIGINT END)
        WHEN prev_tight_after OR tight_before THEN ''
        WHEN ptok IN ('"', '''', '`') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN ptok IN ('"', '''', '`') AND tok IN ('"', '''', '`') THEN ''
        WHEN ptyp IN ('string_content', 'string_fragment', 'escape_sequence') AND tok = '${' THEN ''
        WHEN ptok = '}' AND typ IN ('string_content', 'string_fragment', 'escape_sequence') THEN ''
        WHEN tok IN ('(', '[') AND (ptyp = 'identifier' OR ptok IN (')', ']')) THEN ''
        ELSE ' '
      END || tok,
      '' ORDER BY rn
    ) AS source
  FROM seq
  GROUP BY file_path
);
