-- ast_unparse(path): reconstruct source from the AST, rules-based, no `peek`.
-- PoC (tracker/features/041). Per-language knowledge (block type for indentation,
-- tight-token spacing) is built in here as defaults; productization moves it to .def.
CREATE OR REPLACE MACRO ast_unparse(path) AS TABLE (
  WITH ast AS (SELECT * FROM read_ast(path)),
  bt AS (SELECT * FROM (VALUES
        ('python','block'),('javascript','statement_block'),('typescript','statement_block'),
        ('c','compound_statement'),('cpp','compound_statement'),('java','block'),('go','block')
     ) t(language, block_type)),
  blk AS (SELECT a.file_path, a.node_id, a.descendant_count
          FROM ast a JOIN bt ON a.language = bt.language AND a.type = bt.block_type),
  leaves AS (
    SELECT l.file_path, l.node_id, l.start_line, l.end_line,
           COALESCE(NULLIF(l.name,''), l.type) AS tok,
           (SELECT count(*) FROM blk b
            WHERE b.file_path = l.file_path
              AND b.node_id <= l.node_id AND l.node_id <= b.node_id + b.descendant_count) AS indent
    FROM ast l WHERE l.children_count = 0),
  seq AS (SELECT *, row_number()  OVER (PARTITION BY file_path ORDER BY node_id) rn,
                    LAG(tok)      OVER (PARTITION BY file_path ORDER BY node_id) ptok,
                    LAG(end_line) OVER (PARTITION BY file_path ORDER BY node_id) pel
          FROM leaves)
  SELECT file_path, string_agg(
    CASE WHEN ptok IS NULL                      THEN ''
         WHEN start_line > pel                  THEN repeat(chr(10), start_line - pel) || repeat('    ', indent)
         WHEN tok  IN (',',';',':',')',']','}') THEN ''
         WHEN ptok IN ('(','[','{','.')         THEN ''
         WHEN tok = '.'                         THEN ''
         ELSE ' ' END || tok, '' ORDER BY rn) AS source
  FROM seq GROUP BY file_path
);
