-- Test different macro syntax possibilities with parentheses

-- First, create a simple macro
CREATE OR REPLACE MACRO double_value(x) AS x * 2;

-- Test 1: Standard function call (works)
SELECT double_value(5);

-- Test 2a: Can we use dot notation? (expected to fail)
-- SELECT 5.double_value();

-- Test 2b: Can we use dot notation with explicit parentheses?
SELECT (5).double_value();

-- Test 3: With a column reference
CREATE TABLE test_data (value INTEGER);
INSERT INTO test_data VALUES (10);

-- Test 3a: Column with dot notation
-- SELECT value.double_value() FROM test_data;

-- Test 3b: Parenthesized column with dot notation
SELECT (value).double_value() FROM test_data;

-- Test 4: Create a more complex macro
CREATE OR REPLACE MACRO get_info(x) AS {
    'original': x,
    'doubled': x * 2,
    'squared': x * x
};

-- Test 4a: Can we chain?
SELECT (5).get_info().doubled;

-- Test 5: With JSON data
CREATE OR REPLACE MACRO json_info(j) AS {
    'length': json_array_length(j),
    'first': j[0],
    'type': json_type(j)
};

-- Test 5a: Applying to JSON
SELECT ('[1,2,3]'::JSON).json_info().length;

-- Test 6: Real AST example
-- Let's create a macro that could use dot notation
CREATE OR REPLACE MACRO analyze(nodes) AS {
    'functions': ast_find_type(nodes, 'function_definition'),
    'classes': ast_find_type(nodes, 'class_definition'),
    'count': json_array_length(nodes)
};

-- This would be the ideal syntax if it works:
-- SELECT (nodes).analyze().functions FROM read_ast_objects('test.py', 'python');