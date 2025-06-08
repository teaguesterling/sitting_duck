-- Test extension initialization
-- This tests if the segfault happens during initialization

-- Test if the extension loads properly multiple times
.timer on

-- Test 1: Basic function availability
SELECT 'Testing function availability';
SELECT function_name FROM duckdb_functions() 
WHERE function_name LIKE '%read_ast%' 
ORDER BY function_name;

-- Test 2: Check if setting exists
SELECT 'Testing setting access';
SELECT CASE WHEN current_setting('duckdb_ast_short_names') IS NULL 
            THEN 'Setting not set' 
            ELSE current_setting('duckdb_ast_short_names') 
       END as setting_value;

-- Test 3: Check macro loading
SELECT 'Testing macro loading';
SELECT duckdb_ast_load_embedded_sql('01_core.sql') LIMIT 1;

-- Test 4: Multiple sequential calls (stress test)
SELECT 'Testing sequential calls';
SELECT COUNT(*) FROM read_ast('src/unified_ast_backend.cpp');
SELECT COUNT(*) FROM read_ast('src/unified_ast_backend.cpp');
SELECT COUNT(*) FROM read_ast('src/unified_ast_backend.cpp');