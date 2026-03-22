-- Example SQL file with CREATE FUNCTION and MACRO statements

-- Standard CREATE FUNCTION
CREATE FUNCTION get_user_name(user_id INTEGER)
RETURNS VARCHAR
AS $$
    SELECT name FROM users WHERE id = user_id;
$$;

-- CREATE OR REPLACE FUNCTION
CREATE OR REPLACE FUNCTION calculate_total(price DECIMAL, quantity INTEGER)
RETURNS DECIMAL
AS $$
    SELECT price * quantity;
$$;

-- CREATE TABLE for reference
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    price DECIMAL(10, 2)
);

-- CREATE VIEW
CREATE VIEW active_users AS
SELECT id, name FROM users WHERE active = true;
