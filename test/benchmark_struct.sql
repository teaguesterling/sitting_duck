-- Struct Performance Tests
.timer on

-- Test 1: Filter by type
SELECT 'Filter functions' as test, COUNT(*) as result
FROM ast_data,
     UNNEST(struct_nodes)
WHERE unnest.type = 'function_definition';

-- Test 2: Aggregate by type
SELECT 'Aggregate by type' as test, COUNT(*) as unique_types
FROM (
    SELECT DISTINCT unnest.type
    FROM ast_data,
         UNNEST(struct_nodes)
);

-- Test 3: Complex filter  
SELECT 'Deep identifiers' as test, COUNT(*) as result
FROM ast_data,
     UNNEST(struct_nodes)
WHERE unnest.depth > 2
  AND unnest.type = 'identifier';

-- Test 4: Extract names
SELECT 'Function names' as test, COUNT(*) as result
FROM ast_data,
     UNNEST(struct_nodes)
WHERE unnest.type = 'function_definition'
  AND unnest.name IS NOT NULL;

.timer off