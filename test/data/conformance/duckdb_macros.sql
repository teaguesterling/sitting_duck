-- DuckDB-dialect fixture for the conformance kit.
--
-- The `duckdb` language adapter wraps DuckDB's own parser, so a fixture for it
-- must be valid DuckDB SQL -- not the postgres `$$`-quoted dialect used by
-- test/data/sql/create_function_example.sql, which the tree-sitter `sql`
-- grammar accepts but DuckDB's own parser rejects.
--
-- Deliberately carries one of each thing the kit measures: named definitions,
-- function calls, and a parameter list.
CREATE TABLE products (id INTEGER PRIMARY KEY, name VARCHAR, price DECIMAL(10, 2));

CREATE MACRO total_price(price, quantity) AS price * quantity;

CREATE VIEW active_products AS
SELECT id, upper(name) AS name, total_price(price, 2) AS pair_price
FROM products
WHERE length(name) > 0;

SELECT count(*) FROM active_products;
