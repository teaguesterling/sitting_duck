-- AST API v2 Examples: Structure-Preserving vs Extraction Macros

-- Example 1: Current approach loses metadata
-- ==========================================
-- This returns just a JSON array, losing file_path and other metadata
SELECT 
    ast_find_type(nodes, 'function_definition') as functions
FROM read_ast_objects('src/main.py', 'python');
-- Result: JSON array of function nodes only

-- Example 2: Structure-preserving approach
-- ========================================
-- This returns full AST object with filtered nodes
SELECT 
    filtered.file_path,
    filtered.node_count as filtered_count,
    ast_extract_function_names(filtered) as function_names
FROM read_ast_objects('src/main.py', 'python') as ast,
     LATERAL ast_filter_type(ast, 'function_definition') as filtered;

-- Example 3: Chaining operations while preserving context
-- =======================================================
WITH analysis AS (
    SELECT 
        ast_obj.file_path,
        -- Chain structure-preserving operations
        ast_obj
            .ast_filter_type('function_definition')
            .ast_at_depth_range(2, 4)  -- Only functions at depth 2-4
            .ast_filter_name_pattern('%process%') as filtered_ast
    FROM read_ast_objects('src/**/*.py', 'python') as ast_obj
)
SELECT 
    file_path,
    filtered_ast.node_count as matching_functions,
    ast_extract_source(filtered_ast, 5) as source_with_context
FROM analysis
WHERE filtered_ast.node_count > 0;

-- Example 4: Building comprehensive analysis table
-- ================================================
CREATE OR REPLACE VIEW code_entity_analysis AS
WITH ast_analysis AS (
    -- Get all classes and functions with their context
    SELECT 
        ast_obj.file_path,
        ast_obj
            .ast_filter_types(['class_definition', 'function_definition']) as entities
    FROM read_ast_objects('src/**/*.py', 'python') as ast_obj
),
extracted_entities AS (
    -- Extract structured data from filtered ASTs
    SELECT 
        e.*,
        -- Get parent class name if this is a method
        CASE 
            WHEN e.parent_type = 'class_definition' THEN
                (SELECT json_extract_string(p.value, '$.name')
                 FROM json_each(a.entities.nodes) as p
                 WHERE json_extract(p.value, '$.id')::INTEGER = e.parent_id)
            ELSE NULL
        END as parent_class
    FROM ast_analysis a,
         TABLE(ast_extract_entities(a.entities)) e
),
entity_signatures AS (
    -- Extract function signatures (would need more AST parsing)
    SELECT 
        node_id,
        -- Simplified signature extraction
        '(' || COALESCE(param_list, '') || ') -> ' || COALESCE(return_type, 'None') as signature
    FROM (
        SELECT 
            node_id,
            -- Would extract from AST parameters node
            'self, *args, **kwargs' as param_list,
            -- Would extract from AST return annotation
            'Dict[str, Any]' as return_type
        FROM extracted_entities
        WHERE entity_type = 'function_definition'
    )
)
SELECT 
    e.file as file,
    e.start_line || '-' || e.end_line as lines,
    COALESCE(e.parent_class || '.', '') || e.entity_name as entity,
    COALESCE(s.signature, '') as signature,
    -- These would come from external analysis tools
    'implementation_status: COMPLETE' as annotations,
    'complexity: 0.7, coverage: 85%' as metrics,
    'passed: 10, failed: 0' as test_results
FROM extracted_entities e
LEFT JOIN entity_signatures s ON e.node_id = s.node_id
ORDER BY e.file, e.start_line;

-- Example 5: Source extraction with context
-- =========================================
-- Get all error handling code with context
WITH error_handlers AS (
    SELECT 
        ast_obj.file_path,
        ast_obj
            .ast_find_pattern('try_statement')  -- Find all try blocks
            .ast_expand_to_parent('function_definition') as containing_functions
    FROM read_ast_objects('src/**/*.py', 'python') as ast_obj
)
SELECT 
    file_path,
    source_extract.node_name as function_name,
    source_extract.context_start as start_line,
    source_extract.context_end as end_line,
    source_extract.source_with_context
FROM error_handlers,
     TABLE(ast_extract_source(containing_functions, 3)) as source_extract
WHERE source_extract.source_with_context LIKE '%except%';

-- Example 6: Language-agnostic analysis
-- =====================================
-- Works across different languages with same API
WITH multi_language_analysis AS (
    SELECT * FROM read_ast_objects('src/**/*.py', 'python')
    UNION ALL
    SELECT * FROM read_ast_objects('src/**/*.js', 'javascript')
    UNION ALL  
    SELECT * FROM read_ast_objects('src/**/*.ts', 'typescript')
)
SELECT 
    language,
    COUNT(DISTINCT file_path) as file_count,
    SUM(ast_extract_summary(ast_obj).function_count) as total_functions,
    SUM(ast_extract_summary(ast_obj).class_count) as total_classes,
    AVG(ast_extract_summary(ast_obj).max_depth) as avg_max_depth
FROM multi_language_analysis
GROUP BY language;

-- Example 7: Advanced filtering with metadata preservation
-- ========================================================
-- Find all async functions that call external APIs
SELECT 
    filtered.file_path,
    filtered.language,
    api_calls.*
FROM read_ast_objects('src/**/*.py', 'python') as ast,
     -- First filter to async functions
     LATERAL ast_filter_pattern(ast, 'async_function_definition') as async_funcs,
     -- Then find API calls within those functions  
     LATERAL ast_filter_pattern(async_funcs, 'call[name~="requests|urllib|httpx"]') as filtered,
     -- Extract details about the API calls
     TABLE(ast_extract_calls(filtered)) as api_calls
WHERE filtered.node_count > 0;