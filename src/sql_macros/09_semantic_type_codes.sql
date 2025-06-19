-- Semantic Type Codes Reference
-- Returns all semantic type codes from 0 to 252 by 4s with their names

CREATE OR REPLACE TEMPORARY MACRO semantic_type_codes() AS (
    SELECT 
        get_super_kind(code::UTINYINT) AS super_kind, 
        get_kind(code::UTINYINT) AS kind, 
        semantic_type_to_string(code::UTINYINT) AS semantic_type, 
        code 
    FROM generate_series(0, 255, 4) AS generate_series(code)
);