-- Comprehensive test cases for native context extraction functionality
-- This file tests the semantic signature extraction across different languages

-- Test 1: Basic Python function extraction
.print "=== Test 1: Python Function Extraction ==="
SELECT 
    context.name,
    context.native.signature_type,
    len(context.native.parameters) as param_count,
    len(context.native.modifiers) as modifier_count
FROM read_ast('test/data/comprehensive_python_test.py', 'python') 
WHERE type = 'function_definition' 
  AND context.name IS NOT NULL
ORDER BY context.name;

-- Test 2: Python async functions
.print "=== Test 2: Python Async Functions ==="
SELECT 
    context.name,
    context.native.signature_type,
    context.native.modifiers,
    len(context.native.parameters) as param_count
FROM read_ast('test/data/comprehensive_python_test.py', 'python') 
WHERE type = 'async_function_definition';

-- Test 3: Python class definitions
.print "=== Test 3: Python Class Definitions ==="
SELECT 
    context.name,
    context.native.signature_type,
    context.native.modifiers,
    len(context.native.parameters) as param_count
FROM read_ast('test/data/comprehensive_python_test.py', 'python') 
WHERE type = 'class_definition';

-- Test 4: JavaScript function extraction
.print "=== Test 4: JavaScript Function Extraction ==="
SELECT 
    context.name,
    context.native.signature_type,
    len(context.native.parameters) as param_count,
    context.native.modifiers
FROM read_ast('test/data/javascript_test.js', 'javascript') 
WHERE type = 'function_declaration' 
  AND context.name IS NOT NULL
ORDER BY context.name;

-- Test 5: JavaScript arrow functions
.print "=== Test 5: JavaScript Arrow Functions ==="
SELECT 
    context.name,
    context.native.signature_type,
    context.native.modifiers,
    len(context.native.parameters) as param_count
FROM read_ast('test/data/javascript_test.js', 'javascript') 
WHERE type = 'arrow_function';

-- Test 6: JavaScript class methods
.print "=== Test 6: JavaScript Class Methods ==="
SELECT 
    context.name,
    context.native.signature_type,
    context.native.modifiers
FROM read_ast('test/data/javascript_test.js', 'javascript') 
WHERE type = 'method_definition';

-- Test 7: TypeScript typed functions
.print "=== Test 7: TypeScript Typed Functions ==="
SELECT 
    context.name,
    context.native.signature_type,
    len(context.native.parameters) as param_count,
    context.native.modifiers
FROM read_ast('test/data/typescript_test.ts', 'typescript') 
WHERE type = 'function_declaration' 
  AND context.name IS NOT NULL
ORDER BY context.name;

-- Test 8: TypeScript class definitions
.print "=== Test 8: TypeScript Class Definitions ==="
SELECT 
    context.name,
    context.native.signature_type,
    context.native.modifiers
FROM read_ast('test/data/typescript_test.ts', 'typescript') 
WHERE type = 'class_declaration';

-- Test 9: Detailed parameter extraction for Python
.print "=== Test 9: Python Parameter Details ==="
SELECT 
    context.name,
    unnest(context.native.parameters) as param_details
FROM read_ast('test/data/comprehensive_python_test.py', 'python') 
WHERE type = 'function_definition' 
  AND context.name = 'typed_function';

-- Test 10: Native context structure validation
.print "=== Test 10: Native Context Structure Validation ==="
SELECT 
    type,
    context.name,
    CASE 
        WHEN context.native.signature_type IS NOT NULL THEN 'HAS_SIG_TYPE'
        ELSE 'NO_SIG_TYPE'
    END as signature_status,
    CASE 
        WHEN len(context.native.parameters) > 0 THEN 'HAS_PARAMS'
        ELSE 'NO_PARAMS'
    END as params_status,
    CASE 
        WHEN len(context.native.modifiers) > 0 THEN 'HAS_MODIFIERS'
        ELSE 'NO_MODIFIERS'
    END as modifiers_status
FROM read_ast('test/data/comprehensive_python_test.py', 'python') 
WHERE type IN ('function_definition', 'async_function_definition', 'class_definition')
  AND context.name IS NOT NULL
LIMIT 10;

-- Test 11: Cross-language comparison
.print "=== Test 11: Cross-Language Function Comparison ==="
WITH python_funcs AS (
    SELECT 'Python' as language, context.name, len(context.native.parameters) as param_count
    FROM read_ast('test/data/comprehensive_python_test.py', 'python') 
    WHERE type = 'function_definition' AND context.name IS NOT NULL
),
js_funcs AS (
    SELECT 'JavaScript' as language, context.name, len(context.native.parameters) as param_count
    FROM read_ast('test/data/javascript_test.js', 'javascript') 
    WHERE type = 'function_declaration' AND context.name IS NOT NULL
),
ts_funcs AS (
    SELECT 'TypeScript' as language, context.name, len(context.native.parameters) as param_count
    FROM read_ast('test/data/typescript_test.ts', 'typescript') 
    WHERE type = 'function_declaration' AND context.name IS NOT NULL
)
SELECT * FROM python_funcs
UNION ALL SELECT * FROM js_funcs
UNION ALL SELECT * FROM ts_funcs
ORDER BY language, context.name;

-- Test 12: Hierarchical structure integrity
.print "=== Test 12: Hierarchical Structure Integrity ==="
SELECT 
    node_id,
    type,
    source.language,
    structure.depth,
    context.name,
    CASE 
        WHEN context.native IS NOT NULL THEN 'NATIVE_CONTEXT_PRESENT'
        ELSE 'NATIVE_CONTEXT_MISSING'
    END as native_status
FROM read_ast('test/data/test_native_context.py', 'python') 
WHERE type IN ('function_definition', 'class_definition')
LIMIT 5;

.print "=== Native Context Extraction Tests Complete ==="