-- Test different macro syntax possibilities

-- First, create a simple macro
CREATE OR REPLACE MACRO double_value(x) AS x * 2;

-- Test 1: Standard function call (works)
SELECT double_value(5);

-- Test 2: Can we use dot notation? (expected to fail)
SELECT 5.double_value();

-- Test 3: What about with a column?
CREATE TABLE test_data (value INTEGER);
INSERT INTO test_data VALUES (10);
SELECT value.double_value() FROM test_data;

-- Test 4: Correct syntax with column
SELECT double_value(value) FROM test_data;

-- Test 5: Create a macro that returns a struct
CREATE OR REPLACE MACRO make_point(x, y) AS 
    {'x': x, 'y': y, 'magnitude': sqrt(x*x + y*y)};

-- Test 6: We can use dot notation on the struct result
SELECT make_point(3, 4).magnitude;

-- Test 7: But not to call the macro itself
SELECT 3.make_point(4);  -- Expected to fail