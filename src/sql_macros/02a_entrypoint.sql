-- AST Entrypoint and Chaining Support
-- Provides the ast() function for natural chaining syntax

-- ===================================
-- MAIN ENTRYPOINT
-- ===================================

-- Universal entrypoint that normalizes input to JSON array
CREATE OR REPLACE MACRO ast(input) AS (
    CASE 
        WHEN input IS NULL THEN '[]'::JSON
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'ARRAY' THEN input
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'OBJECT' THEN json_array(input)
        WHEN typeof(input) = 'STRUCT' THEN 
            COALESCE(TRY_CAST(json_extract(CAST(input AS JSON), '$.nodes') AS JSON), '[]'::JSON)
        WHEN typeof(input) = 'VARCHAR' THEN 
            CASE 
                WHEN TRY_CAST(input AS JSON) IS NULL THEN '[]'::JSON
                WHEN json_type(TRY_CAST(input AS JSON)) = 'ARRAY' THEN TRY_CAST(input AS JSON)
                WHEN json_type(TRY_CAST(input AS JSON)) = 'OBJECT' THEN json_array(TRY_CAST(input AS JSON))
                ELSE '[]'::JSON
            END
        ELSE '[]'::JSON
    END
);

-- ===================================
-- CHAIN METHODS
-- ===================================
-- Note: Chain methods (unprefixed names) are now created dynamically
-- by the duckdb_ast_register_short_names() C++ function