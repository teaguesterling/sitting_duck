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
