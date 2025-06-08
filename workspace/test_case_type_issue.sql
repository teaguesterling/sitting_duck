-- Minimal reproducer for CASE type mixing issue

-- This fails with: Cannot mix values of type INTEGER[] and INTEGER_LITERAL in CASE expression
CREATE OR REPLACE MACRO broken_case(val) AS (
    CASE 
        WHEN val IS NULL THEN []
        WHEN typeof(val) = 'INTEGER[]' THEN val
        WHEN typeof(val) = 'INTEGER' THEN [val]
        ELSE []
    END
);

-- Test it
SELECT broken_case(5);