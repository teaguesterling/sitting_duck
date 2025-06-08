-- AI Agent Progressive Discovery Workflow
-- How an AI agent would learn and use the AST API

-- Step 1: Discovery - What can I do?
-- =================================

-- Q: "What functions are available for AST analysis?"
SELECT function_name, description
FROM ast_macro_help()
WHERE function_name LIKE 'ast_%'
ORDER BY function_name;

-- Q: "What node types exist in this Python file?"  
SELECT ast_available_types(ast_obj) as available_types
FROM read_ast_objects('example.py', 'python') as ast_obj;
-- Result: ['module', 'function_definition', 'class_definition', 'import_statement', ...]

-- Step 2: Simple Queries - Getting Started
-- =======================================

-- Q: "Show me all functions in this file"
SELECT ast_extract_names(ast_obj, types := 'function_definition') as functions
FROM read_ast_objects('example.py', 'python') as ast_obj;

-- Q: "I meant all functions AND methods"
SELECT ast_extract_names(
    ast_obj, 
    types := ['function_definition', 'method_definition']
) as all_functions
FROM read_ast_objects('example.py', 'python') as ast_obj;

-- Step 3: Learning from Errors
-- ============================

-- Mistake: Wrong node type
SELECT ast_filter_type(ast_obj, 'function')
FROM read_ast_objects('example.py', 'python') as ast_obj;
-- ERROR: Unknown node type 'function'. Did you mean 'function_definition'?

-- Mistake: Positional parameters
SELECT ast_extract_source(ast_obj, 5)
FROM read_ast_objects('example.py', 'python') as ast_obj;
-- WARNING: Use named parameter: ast_extract_source(ast_obj, pad_lines := 5)

-- Step 4: Using Shortcuts
-- ======================

-- Q: "Find test functions"
SELECT 
    file_path,
    ast_extract_names(ast_test_functions(ast_obj)) as test_names
FROM read_ast_objects('tests/*.py', 'python') as ast_obj;

-- Q: "Find public API"
SELECT *
FROM read_ast_objects('lib/*.py', 'python') as ast_obj,
     TABLE(ast_extract_entities(ast_public_api(ast_obj))) as api;

-- Step 5: Complex Queries with Chaining
-- ====================================

-- Q: "Find complex methods in classes"
WITH class_methods AS (
    SELECT 
        ast_obj.file_path,
        -- Find classes, then get their methods
        ast_obj
            .ast_filter_type('class_definition')
            .ast_descendants()
            .ast_filter_type(['function_definition', 'method_definition']) as methods
    FROM read_ast_objects('**/*.py', 'python') as ast_obj
)
SELECT 
    cm.file_path,
    src.node_name as method_name,
    src.line_count,
    src.source_with_context
FROM class_methods cm,
     TABLE(ast_extract_source(
         cm.methods,
         pad_lines := 2
     )) as src
WHERE src.line_count > 20  -- Complex methods
ORDER BY src.line_count DESC;

-- Step 6: Building Analysis Tables
-- ===============================

-- Q: "Create a code quality report"
WITH file_analysis AS (
    SELECT 
        ast_obj.file_path,
        ast_obj,
        ast_extract_summary(ast_obj) as summary,
        ast_test_functions(ast_obj) as test_funcs,
        ast_public_api(ast_obj) as public_api
    FROM read_ast_objects('src/**/*.py', 'python') as ast_obj
),
detailed_metrics AS (
    SELECT 
        fa.file_path,
        fa.summary.total_nodes,
        fa.summary.function_count,
        fa.summary.class_count,
        fa.summary.max_depth,
        ast_extract_summary(fa.test_funcs).function_count as test_count,
        ast_extract_summary(fa.public_api).function_count as public_api_count,
        -- Complex functions (> 50 lines)
        (SELECT COUNT(*) 
         FROM TABLE(ast_extract_entities(fa.ast_obj, types := 'function_definition')) e
         WHERE e.end_line - e.start_line > 50) as complex_functions
    FROM file_analysis fa
)
SELECT 
    file_path,
    function_count,
    test_count,
    ROUND(100.0 * test_count / NULLIF(function_count, 0), 1) as test_coverage_pct,
    complex_functions,
    CASE 
        WHEN max_depth > 10 THEN 'High complexity'
        WHEN max_depth > 6 THEN 'Medium complexity'
        ELSE 'Low complexity'
    END as complexity_rating
FROM detailed_metrics
ORDER BY complex_functions DESC, file_path;

-- Step 7: Cross-Language Analysis
-- ==============================

-- Q: "Compare Python and JavaScript codebases"
WITH multi_language AS (
    SELECT 
        ast_obj.language,
        ast_obj.file_path,
        ast_extract_summary(ast_obj) as summary
    FROM (
        SELECT * FROM read_ast_objects('backend/**/*.py', 'python')
        UNION ALL
        SELECT * FROM read_ast_objects('frontend/**/*.js', 'javascript')
    ) as ast_obj
)
SELECT 
    language,
    COUNT(DISTINCT file_path) as file_count,
    SUM(summary.function_count) as total_functions,
    AVG(summary.function_count) as avg_functions_per_file,
    AVG(summary.max_depth) as avg_max_depth,
    MAX(summary.lines_of_code) as largest_file_loc
FROM multi_language
GROUP BY language;

-- Step 8: Interactive Exploration
-- ==============================

-- Q: "What's in this specific function?"
WITH target_function AS (
    SELECT 
        ast_obj
            .ast_filter_type('function_definition')
            .ast_filter_name('process_data') as func
    FROM read_ast_objects('processor.py', 'python') as ast_obj
)
SELECT 
    -- Get the function source
    src.source_with_context,
    -- What does it call?
    ast_extract_names(
        func.ast_descendants().ast_filter_type('call'),
        pattern := '*'
    ) as function_calls,
    -- What variables does it use?
    ast_extract_names(
        func.ast_descendants().ast_filter_type('identifier')
    ) as identifiers
FROM target_function,
     TABLE(ast_extract_source(func, pad_lines := 3)) as src;

-- AI Agent Learning Path:
-- 1. Start with discovery functions
-- 2. Use simple extraction queries
-- 3. Learn from helpful error messages
-- 4. Discover and use shortcuts
-- 5. Build complex chained queries
-- 6. Create domain-specific analyses
-- 7. Compare across languages
-- 8. Deep-dive into specific code

-- The API guides the agent from simple to complex usage naturally!