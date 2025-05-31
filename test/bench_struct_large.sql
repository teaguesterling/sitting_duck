-- Struct benchmark on large dataset (4638 nodes)
.timer on

SELECT 'Total nodes' as test, COUNT(*) as result
FROM struct_test,
     UNNEST(struct_nodes);

SELECT 'Filter functions' as test, COUNT(*) as result
FROM struct_test,
     UNNEST(struct_nodes)
WHERE unnest.type LIKE '%function%';

SELECT 'Deep nodes (>5)' as test, COUNT(*) as result
FROM struct_test,
     UNNEST(struct_nodes)
WHERE unnest.depth > 5;

SELECT 'Identifiers' as test, COUNT(*) as result
FROM struct_test,
     UNNEST(struct_nodes)
WHERE unnest.type = 'identifier';

SELECT 'Aggregate types' as test, COUNT(DISTINCT unnest.type) as unique_types
FROM struct_test,
     UNNEST(struct_nodes);

.timer off