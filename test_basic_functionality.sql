-- Basic AST Extension Functionality Test
-- This test focuses on core functionality rather than complex integrations

-- Test 1: Supported Languages
SELECT 'Test 1: Supported Languages' as test_name;
SELECT language FROM ast_supported_languages() ORDER BY language;

-- Test 2: Auto-detection
SELECT 'Test 2: Auto-detection' as test_name;
-- Python
SELECT COUNT(*) as python_nodes FROM read_ast('/tmp/hello.py') WHERE name IS NOT NULL;
-- JavaScript  
SELECT COUNT(*) as js_nodes FROM read_ast('/tmp/hello.js') WHERE name IS NOT NULL;
-- Go
SELECT COUNT(*) as go_nodes FROM read_ast('/tmp/hello.go') WHERE name IS NOT NULL;

-- Test 3: Explicit Language Specification
SELECT 'Test 3: Explicit Language Specification' as test_name;
SELECT COUNT(*) as explicit_python FROM read_ast('/tmp/hello.py', 'python') WHERE name IS NOT NULL;
SELECT COUNT(*) as explicit_go FROM read_ast('/tmp/hello.go', 'go') WHERE name IS NOT NULL;

-- Test 4: Basic Name Extraction
SELECT 'Test 4: Basic Name Extraction' as test_name;
-- Python function names
SELECT name, type FROM read_ast('/tmp/hello.py') 
WHERE type = 'function_definition' AND name IS NOT NULL 
ORDER BY start_line;

-- Go function names  
SELECT name, type FROM read_ast('/tmp/hello.go')
WHERE type = 'function_declaration' AND name IS NOT NULL
ORDER BY start_line;

-- Test 5: Semantic Types
SELECT 'Test 5: Semantic Types' as test_name;
SELECT type, semantic_type, COUNT(*) as count
FROM read_ast('/tmp/hello.py')
WHERE semantic_type IS NOT NULL
GROUP BY type, semantic_type
ORDER BY count DESC
LIMIT 5;

-- Test 6: Tree Structure
SELECT 'Test 6: Tree Structure' as test_name;
SELECT depth, COUNT(*) as node_count 
FROM read_ast('/tmp/hello.py')
GROUP BY depth 
ORDER BY depth
LIMIT 5;

-- Test 7: File Position Information
SELECT 'Test 7: File Position Information' as test_name;
SELECT name, start_line, start_column, end_line, end_column
FROM read_ast('/tmp/hello.py')
WHERE name IS NOT NULL AND start_line <= 5
ORDER BY start_line, start_column;

-- Test 8: Different File Types
SELECT 'Test 8: Different File Types' as test_name;
-- Create and test simple files for each language
SELECT 'Python nodes:' || COUNT(*) as result FROM read_ast('/tmp/hello.py')
UNION ALL
SELECT 'JavaScript nodes:' || COUNT(*) as result FROM read_ast('/tmp/hello.js')  
UNION ALL
SELECT 'Go nodes:' || COUNT(*) as result FROM read_ast('/tmp/hello.go');