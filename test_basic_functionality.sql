-- Quick validation script for unified backend
-- Run this after build completes

.load './build/debug/extension/duckdb_ast/duckdb_ast.duckdb_extension'

-- Test 1: Basic parsing works
SELECT 'Test 1: Basic parsing' as test;
SELECT COUNT(*) as node_count FROM parse_ast('def hello(): pass', 'python');

-- Test 2: Schema has new taxonomy fields  
SELECT 'Test 2: Schema check' as test;
SELECT 
    COUNT(*) as has_kind,
    COUNT(*) as has_flags,
    COUNT(*) as has_semantic_id
FROM parse_ast('def hello(): pass', 'python')
WHERE kind IS NOT NULL 
  AND universal_flags IS NOT NULL 
  AND semantic_id IS NOT NULL;

-- Test 3: Type normalization works
SELECT 'Test 3: Type normalization' as test;
SELECT type, normalized_type 
FROM parse_ast('def hello(): pass', 'python')
WHERE type = 'function_definition';

-- Test 4: Name extraction works
SELECT 'Test 4: Name extraction' as test;
SELECT type, name
FROM parse_ast('def hello(): pass', 'python')
WHERE name IS NOT NULL AND name != '';

-- Test 5: AST objects return struct
SELECT 'Test 5: AST objects' as test;
SELECT typeof(ast) as ast_type
FROM read_ast_objects('test/data/python/simple.py')
LIMIT 1;

-- Test 6: Cross-language support
SELECT 'Test 6: Multi-language' as test;
SELECT 'python' as lang, COUNT(*) as nodes FROM parse_ast('def f(): pass', 'python')
UNION ALL  
SELECT 'javascript' as lang, COUNT(*) as nodes FROM parse_ast('function f() {}', 'javascript')
UNION ALL
SELECT 'cpp' as lang, COUNT(*) as nodes FROM parse_ast('void f() {}', 'cpp');

SELECT 'All tests completed!' as result;