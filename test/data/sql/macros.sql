-- SQL macro definitions for testing name extraction
CREATE MACRO resolve(x) AS x * 2;

CREATE OR REPLACE MACRO read_source(path) AS (
    SELECT * FROM read_csv(path)
);

CREATE MACRO add_numbers(a, b) AS a + b;

-- Regular table for comparison
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100)
);

-- View definition
CREATE VIEW active_users AS
SELECT * FROM users WHERE id > 0;
