-- Standard-SQL fixture for the conformance kit's `sql` (tree-sitter) row.
--
-- WHY THIS EXISTS SEPARATELY FROM duckdb_macros.sql: the tree-sitter `sql`
-- grammar cannot parse DuckDB's `CREATE MACRO`, so pointing the `sql` language
-- at duckdb_macros.sql produces an ERROR node and the parse guard quarantines
-- the file -- which left a language declaring FIVE naming call node types with
-- no call coverage at all, because the other two sql fixtures (a bare SELECT
-- and a postgres `$$`-quoted CREATE FUNCTION) contain no call nodes either.
--
-- So: ordinary portable SQL, with function calls the tree-sitter grammar
-- parses cleanly, and a parameterised CREATE FUNCTION in the dialect that
-- grammar accepts.
SELECT upper(name)        AS shout,
       length(name)       AS len,
       coalesce(note, '') AS note,
       round(price, 2)    AS price
FROM products
WHERE lower(name) LIKE 'a%'
ORDER BY len DESC;

SELECT count(*) AS n, max(price) AS top FROM products;
