-- Cross-line DuckDB-SQL corpus for scripts/compare_duckdb_lines.sh.
--
-- Every statement here must PARSE on both DuckDB lines sitting_duck targets
-- (v1.5.6 "Variegata" and v2.0 "Cyanoptera"). It is never executed: the
-- comparison only calls read_ast(<file>, 'duckdb'), which runs DuckDB's
-- parser and walks the resulting statement tree. So the tables and types
-- referenced below need not exist, but the grammar must be accepted by both
-- parsers -- a statement that parses on only one line collapses the WHOLE
-- file to a single `parse_error` node and destroys the comparison.
--
-- Because nothing here is bound, a statement can parse on both lines and
-- still be unexecutable on one of them: v2.0 rejects the deprecated lambda
-- arrow (`x -> ...`) with a Binder Error that read_ast never sees. The
-- `lambda x: ...` spelling below works on both lines, and is used on purpose.
--
-- Breadth is the point: DDL, DML, queries and expressions, so that a parser
-- or deparser change on either line has somewhere to show up. In particular
-- boolean literals appear as COMPARISON OPERANDS (`col = true`), not only in
-- DEFAULT/SET/option positions: only the operand position is walked into a
-- node, and that node is where v1.5.6's implicit cast_expression wrapper
-- (dropped in v2.0) shows up as structural drift.

-- ---------------------------------------------------------------- DDL
CREATE SCHEMA IF NOT EXISTS staging;

CREATE TYPE mood AS ENUM ('sad', 'ok', 'happy');

CREATE SEQUENCE staging.order_seq START 100 INCREMENT BY 5;

CREATE TABLE staging.customers (
    id          BIGINT PRIMARY KEY DEFAULT nextval('staging.order_seq'),
    name        VARCHAR NOT NULL,
    email       VARCHAR UNIQUE,
    signup_at   TIMESTAMP DEFAULT now(),
    tier        VARCHAR CHECK (tier IN ('free', 'pro', 'team')),
    disposition mood
);

CREATE TABLE staging.orders (
    id          BIGINT PRIMARY KEY,
    customer_id BIGINT REFERENCES staging.customers (id),
    placed_on   DATE NOT NULL,
    total_cents INTEGER NOT NULL,
    tags        VARCHAR[],
    attributes  STRUCT(channel VARCHAR, campaign VARCHAR)
);

CREATE INDEX orders_by_customer ON staging.orders (customer_id, placed_on);

CREATE OR REPLACE VIEW staging.active_customers AS
SELECT id, name, tier
FROM staging.customers
WHERE signup_at >= now() - INTERVAL 90 DAY;

CREATE OR REPLACE MACRO dollars(cents) AS cents / 100.0;

CREATE OR REPLACE MACRO orders_for(cust) AS TABLE
SELECT * FROM staging.orders WHERE customer_id = cust;

ALTER TABLE staging.orders ADD COLUMN refunded BOOLEAN DEFAULT false;

COMMENT ON TABLE staging.orders IS 'One row per placed order.';

-- ---------------------------------------------------------------- DML
INSERT INTO staging.customers (name, email, tier, disposition)
VALUES ('Ada Lovelace', 'ada@example.com', 'pro', 'happy'),
       ('Alan Turing', 'alan@example.com', 'team', 'ok');

INSERT INTO staging.orders (id, customer_id, placed_on, total_cents, tags, attributes)
SELECT nextval('staging.order_seq'),
       c.id,
       DATE '2024-03-01',
       1999,
       ['web', 'promo'],
       {'channel': 'web', 'campaign': 'spring'}
FROM staging.customers AS c
WHERE c.tier <> 'free';

INSERT INTO staging.customers (id, name, email)
VALUES (1, 'Duplicate', 'dup@example.com')
ON CONFLICT (id) DO UPDATE SET name = excluded.name;

INSERT INTO staging.customers (id, name)
VALUES (2, 'Ignored')
ON CONFLICT DO NOTHING;

UPDATE staging.orders
SET total_cents = total_cents - 100,
    refunded = true
WHERE placed_on < DATE '2024-01-01'
  AND customer_id IN (SELECT id FROM staging.customers WHERE tier = 'free');

DELETE FROM staging.orders
WHERE total_cents <= 0;

COPY (SELECT * FROM staging.orders) TO 'orders.csv' (HEADER, DELIMITER ',');

COPY staging.customers FROM 'customers.csv' (AUTO_DETECT true);

-- ---------------------------------------------------------------- Queries
SELECT c.name, o.placed_on, o.total_cents
FROM staging.customers AS c
INNER JOIN staging.orders AS o ON o.customer_id = c.id
LEFT OUTER JOIN staging.active_customers AS a USING (id)
WHERE o.total_cents > 500
ORDER BY o.placed_on DESC, c.name ASC
LIMIT 25 OFFSET 5;

SELECT c.tier, count(*) AS order_count, sum(o.total_cents) AS gross
FROM staging.customers AS c
JOIN staging.orders AS o ON o.customer_id = c.id
GROUP BY c.tier
HAVING sum(o.total_cents) > 10000
ORDER BY gross DESC;

WITH recent AS (
    SELECT * FROM staging.orders WHERE placed_on >= DATE '2024-01-01'
),
per_customer AS (
    SELECT customer_id, sum(total_cents) AS spend FROM recent GROUP BY customer_id
)
SELECT c.name, p.spend
FROM per_customer AS p
JOIN staging.customers AS c ON c.id = p.customer_id;

WITH RECURSIVE countdown(n) AS (
    SELECT 5
    UNION ALL
    SELECT n - 1 FROM countdown WHERE n > 1
)
SELECT n FROM countdown ORDER BY n;

SELECT customer_id,
       total_cents,
       row_number() OVER (PARTITION BY customer_id ORDER BY placed_on) AS seq,
       sum(total_cents) OVER w AS running_total,
       lag(total_cents, 1, 0) OVER (ORDER BY placed_on) AS prev_total
FROM staging.orders
WINDOW w AS (PARTITION BY customer_id ORDER BY placed_on
             ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW);

SELECT name,
       (SELECT count(*) FROM staging.orders o WHERE o.customer_id = c.id) AS orders,
       EXISTS (SELECT 1 FROM staging.orders o WHERE o.customer_id = c.id AND o.refunded) AS had_refund
FROM staging.customers AS c
WHERE c.id = ANY (SELECT customer_id FROM staging.orders);

SELECT customer_id, sum(total_cents) AS spend
FROM staging.orders
GROUP BY customer_id
QUALIFY spend > 1000;

SELECT id FROM staging.customers
UNION
SELECT customer_id FROM staging.orders
EXCEPT
SELECT id FROM staging.customers WHERE tier = 'free'
INTERSECT
SELECT customer_id FROM staging.orders WHERE refunded;

SELECT c.name, t.tag
FROM staging.orders AS o,
     LATERAL (SELECT unnest(o.tags) AS tag) AS t
JOIN staging.customers AS c ON c.id = o.customer_id;

SELECT unnest(tags) AS tag, count(*) AS n
FROM staging.orders
GROUP BY ALL
ORDER BY ALL;

SELECT * FROM staging.orders AS o
WHERE NOT EXISTS (SELECT 1 FROM staging.customers c WHERE c.id = o.customer_id)
  AND o.placed_on BETWEEN DATE '2023-01-01' AND DATE '2024-12-31';

-- ---------------------------------------------------------------- Expressions
SELECT CASE
           WHEN total_cents > 10000 THEN 'large'
           WHEN total_cents > 1000 THEN 'medium'
           ELSE 'small'
       END AS bucket,
       CASE tier WHEN 'pro' THEN 1 WHEN 'team' THEN 2 ELSE 0 END AS tier_rank
FROM staging.orders
JOIN staging.customers ON staging.customers.id = staging.orders.customer_id;

-- Casts in BOTH casings on purpose. v1.5.6 normalises the type name to
-- canonical uppercase and quotes it (`CAST(x AS date)` -> `CAST(x AS "DATE")`);
-- v2.0 preserves whatever the source wrote and drops the quotes. A corpus that
-- spelled every type uppercase would make the two lines look closer than they
-- are, because the de-quoting would be the only visible half of the change.
SELECT CAST(total_cents AS DOUBLE) / 100 AS dollars,
       CAST(total_cents AS double) / 100 AS dollars_lower,
       CAST(placed_on AS DATE) AS day_upper,
       CAST(placed_on AS date) AS day_lower,
       total_cents::VARCHAR AS as_text,
       total_cents::varchar AS as_text_lower,
       TRY_CAST('not a number' AS INTEGER) AS maybe_null,
       TRY_CAST('not a number' AS integer) AS maybe_null_lower,
       CAST(attributes AS JSON) AS attrs_json
FROM staging.orders;

-- Schema qualification, both spellings. v1.5.6 renders IMPLICITLY generated
-- function references schema-qualified (`[1,2,3][1]` -> `main.list_value(1, 2,
-- 3)[1]`); v2.0 does not. An EXPLICITLY written `main.upper('x')` is
-- byte-identical on both lines, so having only the explicit form would miss
-- the change entirely, and having only the implicit form would hide that the
-- change is implicit-only.
SELECT main.upper(name) AS explicitly_qualified,
       upper(name) AS unqualified,
       [1, 2, 3][1] AS implicitly_qualified
FROM staging.customers;

SELECT {'channel': 'web', 'nested': {'depth': 2}} AS a_struct,
       [1, 2, 3, 5, 8] AS a_list,
       MAP {'alpha': 1, 'beta': 2} AS a_map,
       list_value(10, 20, 30) AS built_list,
       a_struct.channel AS channel,
       a_list[2] AS second,
       a_list[1:3] AS sliced;

-- `lambda x: ...`, not `x -> ...`: the arrow parses on both lines but is a hard
-- Binder Error on v2.0, and a parse-only corpus would never notice.
SELECT list_transform([1, 2, 3], lambda x: x * x) AS squares,
       list_filter([1, 2, 3, 4], lambda x: x % 2 = 0) AS evens,
       list_reduce([1, 2, 3, 4], lambda acc, x: acc + x) AS total;

SELECT name
FROM staging.customers
WHERE name LIKE 'A%'
   OR name ILIKE '%turing%'
   OR name SIMILAR TO '[A-Z].*'
   OR name NOT LIKE '%test%';

SELECT name COLLATE NOCASE AS cased,
       tier IS NOT NULL AS has_tier,
       tier IS DISTINCT FROM 'free' AS not_free,
       coalesce(email, 'unknown') AS email_or_default
FROM staging.customers
ORDER BY name COLLATE NOCASE;

SELECT INTERVAL 3 MONTH AS quarter,
       INTERVAL '1 year 2 months' AS mixed,
       DATE '2024-03-01' + INTERVAL 7 DAY AS week_later,
       TIMESTAMP '2024-03-01 12:30:00' AS stamp,
       date_part('year', placed_on) AS yr,
       strftime(placed_on, '%Y-%m') AS month_key
FROM staging.orders;

-- Boolean literals as comparison OPERANDS. v1.5.6 wraps each in an implicit
-- `cast_expression` node; v2.0 emits a bare `literal`. That is a structural
-- difference (type, name and semantic_type all move), so these lines are the
-- corpus's live check that the structural half of the comparison has teeth.
SELECT id
FROM staging.customers
WHERE tier = 'pro'
  AND (SELECT count(*) FROM staging.orders o WHERE o.customer_id = staging.customers.id) > 0;

SELECT *
FROM staging.orders
WHERE refunded = true
  AND (refunded <> false OR refunded IS NULL)
  AND (total_cents > 0) = true;

SELECT total_cents BETWEEN 100 AND 1000 AS in_band,
       total_cents NOT BETWEEN 0 AND 10 AS out_of_band,
       -total_cents AS negated,
       total_cents % 7 AS remainder,
       (total_cents + 1) * 2 - 3 AS arithmetic,
       total_cents > 0 AND NOT refunded OR refunded IS NULL AS mixed_logic
FROM staging.orders;
