-- Minimal test for the CASE type issue

-- First, let's see what happens with the simplest case
CREATE OR REPLACE MACRO simple_test(val) AS (
    CASE 
        WHEN typeof(val) = 'INTEGER' THEN [val]::INTEGER[]
        ELSE val
    END
);

SELECT simple_test(5);

-- Now with NULL check
CREATE OR REPLACE MACRO with_null_test(val) AS (
    CASE 
        WHEN val IS NULL THEN []::INTEGER[]
        WHEN typeof(val) = 'INTEGER' THEN [val]::INTEGER[]
        ELSE val
    END
);

SELECT with_null_test(5);

-- The actual failing case
CREATE OR REPLACE MACRO failing_test(val) AS (
    CASE 
        WHEN val IS NULL THEN []
        WHEN typeof(val) = 'INTEGER[]' THEN val
        ELSE [val]
    END
);

SELECT failing_test(5);