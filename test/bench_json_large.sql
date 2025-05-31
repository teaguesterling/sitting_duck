-- JSON benchmark on large dataset (4638 nodes)
.timer on

SELECT 'Total nodes' as test, COUNT(*) as result
FROM json_test,
     json_each(json_nodes::JSON);

SELECT 'Filter functions' as test, COUNT(*) as result
FROM json_test,
     json_each(json_nodes::JSON) as je
WHERE json_extract_string(je.value, '$.type') LIKE '%function%';

SELECT 'Deep nodes (>5)' as test, COUNT(*) as result
FROM json_test,
     json_each(json_nodes::JSON) as je
WHERE CAST(json_extract(je.value, '$.depth') AS INTEGER) > 5;

SELECT 'Identifiers' as test, COUNT(*) as result
FROM json_test,
     json_each(json_nodes::JSON) as je
WHERE json_extract_string(je.value, '$.type') = 'identifier';

SELECT 'Aggregate types' as test, COUNT(DISTINCT json_extract_string(je.value, '$.type')) as unique_types
FROM json_test,
     json_each(json_nodes::JSON) as je;

.timer off