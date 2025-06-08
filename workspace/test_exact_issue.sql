-- Test exact issue from ensure_integer_array

-- This is the exact macro that's failing
CREATE OR REPLACE MACRO ensure_integer_array(val) AS (
    CASE 
        WHEN val IS NULL THEN []::INTEGER[]
        WHEN typeof(val) = 'INTEGER[]' THEN val
        WHEN typeof(val) = 'BIGINT[]' THEN val::INTEGER[]
        ELSE [val]::INTEGER[]
    END
);

-- Test various inputs
SELECT 'Test 1: Integer' as test, ensure_integer_array(5);
SELECT 'Test 2: Array' as test, ensure_integer_array([1, 2, 3]);
SELECT 'Test 3: NULL' as test, ensure_integer_array(NULL);

-- Now let's test with the full path like in ast_at_depth
CREATE OR REPLACE MACRO test_ast_at_depth(nodes, depths) AS (
    COALESCE(
        (SELECT json_group_array(je.value)
         FROM json_each(nodes) AS je
         WHERE list_contains(ensure_integer_array(depths), json_extract(je.value, '$.depth')::INTEGER)),
        '[]'::JSON
    )
);

-- Test with sample data
WITH test_data AS (
    SELECT '[{"depth": 1, "type": "test"}, {"depth": 2, "type": "test2"}]'::JSON as nodes
)
SELECT test_ast_at_depth(nodes, 1) FROM test_data;