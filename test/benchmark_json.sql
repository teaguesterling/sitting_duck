-- JSON Performance Tests
.timer on

-- Test 1: Filter by type
SELECT 'Filter functions' as test, COUNT(*) as result
FROM ast_data,
     json_each(json_nodes::JSON) as je
WHERE json_extract_string(je.value, '$.type') = 'function_definition';

-- Test 2: Aggregate by type  
SELECT 'Aggregate by type' as test, COUNT(*) as unique_types
FROM (
    SELECT DISTINCT json_extract_string(je.value, '$.type') as type
    FROM ast_data,
         json_each(json_nodes::JSON) as je
);

-- Test 3: Complex filter
SELECT 'Deep identifiers' as test, COUNT(*) as result
FROM ast_data,
     json_each(json_nodes::JSON) as je
WHERE CAST(json_extract(je.value, '$.depth') AS INTEGER) > 2
  AND json_extract_string(je.value, '$.type') = 'identifier';

-- Test 4: Extract names
SELECT 'Function names' as test, COUNT(*) as result
FROM ast_data,
     json_each(json_nodes::JSON) as je
WHERE json_extract_string(je.value, '$.type') = 'function_definition'
  AND json_extract_string(je.value, '$.name') IS NOT NULL;

.timer off