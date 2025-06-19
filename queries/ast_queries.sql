-- AST Queries Library - Master File
-- Comprehensive SQL macro library for AST analysis
-- Load this file to get access to all AST analysis functions

-- Load the extension (update path as needed)
-- LOAD 'build/release/extension/sitting_duck/sitting_duck.duckdb_extension';

-- =============================================================================
-- LIBRARY INFORMATION
-- =============================================================================

CREATE OR REPLACE MACRO ast_library_info() AS TABLE
SELECT * FROM (VALUES 
    ('ast_queries_version', '1.0.0'),
    ('extension', 'sitting_duck'),
    ('description', 'Comprehensive AST analysis query library'),
    ('categories', 'search, analysis, navigation, utilities'),
    ('last_updated', '2025-06-18')
) as info(property, value);

-- =============================================================================
-- CORE DATABASE FUNCTIONS (from queries/core/)
-- =============================================================================

-- Check database status and statistics
CREATE OR REPLACE MACRO ast_db_status() AS TABLE
SELECT 
    'files' as table_name,
    COUNT(*) as record_count,
    COUNT(*) FILTER (WHERE status = 'current') as current_files,
    COUNT(*) FILTER (WHERE status = 'stale') as stale_files,
    COUNT(*) FILTER (WHERE status = 'deleted') as deleted_files
FROM files
UNION ALL
SELECT 
    'nodes' as table_name,
    COUNT(*) as record_count,
    COUNT(DISTINCT file_id) as unique_files,
    NULL as stale_files,
    NULL as deleted_files
FROM nodes
UNION ALL  
SELECT 
    'relationships' as table_name,
    COUNT(*) as record_count,
    COUNT(DISTINCT relationship_type) as unique_types,
    NULL as stale_files,
    NULL as deleted_files
FROM relationships;

-- Get comprehensive database statistics
CREATE OR REPLACE MACRO ast_db_stats() AS TABLE
WITH stats AS (
    SELECT 
        COUNT(DISTINCT f.file_path) as total_files,
        COUNT(DISTINCT f.language) as languages,
        COUNT(*) as total_nodes,
        COUNT(*) FILTER (WHERE (n.semantic_type & 240) = 112) as definitions,
        COUNT(*) FILTER (WHERE n.type IN ('function_definition', 'method_definition')) as functions,
        COUNT(*) FILTER (WHERE n.type IN ('class_definition', 'struct_declaration')) as classes,
        AVG(n.descendant_count) as avg_complexity,
        MAX(f.updated_at) as last_update
    FROM nodes n
    JOIN files f ON n.file_id = f.id
    WHERE f.status = 'current'
)
SELECT * FROM stats;

-- =============================================================================
-- SEARCH FUNCTIONS (from queries/search/)
-- =============================================================================

-- Main fuzzy search function (simplified - no complex name resolution)
CREATE OR REPLACE MACRO ast_search(search_term, type_filter := NULL, language_filter := NULL, limit_results := 50) AS TABLE
WITH search_matches AS (
    SELECT 
        f.file_path,
        f.language,
        n.name,
        n.type,
        CASE 
            WHEN (n.semantic_type & 240) = 112 THEN 'DEFINITION'
            WHEN n.type IN ('call_expression', 'function_call', 'method_invocation') THEN 'CALL'
            WHEN n.type IN ('identifier', 'variable_name') THEN 'REFERENCE'
            ELSE 'OTHER'
        END as usage_type,
        n.start_line,
        n.end_line,
        n.descendant_count,
        n.source_text,
        -- Simple similarity scoring
        CASE 
            WHEN LOWER(n.name) = LOWER(search_term) THEN 1.0
            WHEN LOWER(n.name) LIKE LOWER(search_term) || '%' THEN 0.9
            WHEN LOWER(n.name) LIKE '%' || LOWER(search_term) || '%' THEN 0.8
            WHEN levenshtein(n.name, search_term) <= 2 THEN 0.7
            ELSE 0.5
        END as similarity_score
    FROM nodes n
    JOIN files f ON n.file_id = f.id
    WHERE f.status = 'current'
      AND n.name IS NOT NULL 
      AND n.name != ''
      -- Apply filters
      AND (type_filter IS NULL OR n.type ILIKE '%' || type_filter || '%')
      AND (language_filter IS NULL OR f.language = language_filter)
      -- Pre-filter for performance
      AND (
          LOWER(n.name) LIKE '%' || LOWER(search_term) || '%'
          OR levenshtein(n.name, search_term) <= 3
      )
)
SELECT 
    file_path,
    language,
    name,
    type,
    usage_type,
    start_line,
    end_line,
    descendant_count,
    source_text,
    ROUND(similarity_score, 3) as similarity_score
FROM search_matches
WHERE similarity_score >= 0.6
ORDER BY similarity_score DESC, name ASC
LIMIT limit_results;

-- Exact name search (fastest)
CREATE OR REPLACE MACRO ast_find_exact(search_term, type_filter := NULL) AS TABLE
SELECT 
    f.file_path,
    f.language,
    n.name,
    n.type,
    CASE 
        WHEN (n.semantic_type & 240) = 112 THEN 'DEFINITION'
        WHEN n.type IN ('call_expression', 'function_call', 'method_invocation') THEN 'CALL'
        WHEN n.type IN ('identifier', 'variable_name') THEN 'REFERENCE'
        ELSE 'OTHER'
    END as usage_type,
    n.start_line,
    n.end_line,
    n.descendant_count,
    n.source_text
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.name = search_term
  AND (type_filter IS NULL OR n.type ILIKE '%' || type_filter || '%')
ORDER BY f.file_path, n.start_line;

-- Find functions (basic version without qualified name parsing)
CREATE OR REPLACE MACRO ast_find_functions(search_term := NULL, limit_results := 50) AS TABLE
SELECT 
    f.file_path,
    f.language,
    n.name as function_name,
    n.type,
    n.start_line,
    n.end_line,
    (n.end_line - n.start_line + 1) as line_count,
    n.children_count,
    n.descendant_count as complexity,
    n.source_text
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.type IN ('function_definition', 'method_definition', 'function_declarator')
  AND (n.semantic_type & 240) = 112  -- DEFINITION
  AND n.name IS NOT NULL
  AND n.name != ''
  AND (search_term IS NULL OR n.name ILIKE '%' || search_term || '%')
ORDER BY f.file_path, n.start_line
LIMIT limit_results;

-- Find classes/types
CREATE OR REPLACE MACRO ast_find_classes(search_term := NULL, limit_results := 50) AS TABLE
SELECT 
    f.file_path,
    f.language,
    n.name as class_name,
    n.type,
    n.start_line,
    n.end_line,
    n.children_count as member_count,
    n.descendant_count as complexity,
    n.source_text
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.type IN ('class_definition', 'class_declaration', 'struct_declaration', 'interface_declaration')
  AND (n.semantic_type & 240) = 112  -- DEFINITION
  AND n.name IS NOT NULL
  AND (search_term IS NULL OR n.name ILIKE '%' || search_term || '%')
ORDER BY f.file_path, n.start_line
LIMIT limit_results;

-- Find variables/constants
CREATE OR REPLACE MACRO ast_find_variables(search_term := NULL, limit_results := 50) AS TABLE
SELECT 
    f.file_path,
    f.language,
    n.name as variable_name,
    n.type,
    n.start_line,
    n.depth,
    n.source_text
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.type IN ('variable_declaration', 'assignment', 'const_declaration', 'let_declaration', 'var_declaration')
  AND (n.semantic_type & 240) = 112  -- DEFINITION
  AND n.name IS NOT NULL
  AND (search_term IS NULL OR n.name ILIKE '%' || search_term || '%')
ORDER BY f.file_path, n.start_line
LIMIT limit_results;

-- =============================================================================
-- COMPLEXITY ANALYSIS (from queries/analysis/)
-- =============================================================================

-- Find complex functions with configurable thresholds
CREATE OR REPLACE MACRO ast_complex_functions(min_nodes := 100, limit_results := 50) AS TABLE
SELECT 
    f.file_path,
    f.language,
    n.name as function_name,
    n.type,
    n.start_line,
    n.end_line,
    (n.end_line - n.start_line + 1) as line_count,
    n.children_count,
    n.descendant_count as total_nodes,
    ROUND(n.descendant_count::FLOAT / (n.end_line - n.start_line + 1), 2) as nodes_per_line,
    n.source_text
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.type IN ('function_definition', 'method_definition', 'function_declarator')
  AND (n.semantic_type & 240) = 112  -- DEFINITION
  AND n.name IS NOT NULL
  AND n.name != ''
  AND n.descendant_count >= min_nodes
ORDER BY n.descendant_count DESC
LIMIT limit_results;

-- File-level complexity metrics
CREATE OR REPLACE MACRO ast_file_complexity() AS TABLE
SELECT 
    f.file_path,
    f.language,
    f.line_count,
    f.size_bytes,
    COUNT(*) as total_nodes,
    COUNT(*) FILTER (WHERE (n.semantic_type & 240) = 112) as definitions,
    COUNT(*) FILTER (WHERE n.type IN ('function_definition', 'method_definition')) as functions,
    COUNT(*) FILTER (WHERE n.type IN ('class_definition', 'struct_declaration', 'interface_declaration')) as classes,
    COUNT(*) FILTER (WHERE n.type IN ('if_statement', 'for_statement', 'while_statement', 'switch_statement')) as control_structures,
    MAX(n.depth) as max_nesting_depth,
    ROUND(AVG(n.depth), 2) as avg_depth,
    MAX(n.descendant_count) as max_function_complexity,
    ROUND(AVG(n.descendant_count) FILTER (WHERE n.type IN ('function_definition', 'method_definition')), 2) as avg_function_complexity,
    ROUND(COUNT(*)::FLOAT / NULLIF(f.line_count, 0), 2) as nodes_per_line
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
GROUP BY f.file_path, f.language, f.line_count, f.size_bytes
ORDER BY total_nodes DESC;

-- Find hotspots (complexity + usage)
CREATE OR REPLACE MACRO ast_hotspots(complexity_threshold := 100, limit_results := 25) AS TABLE
WITH function_metrics AS (
    SELECT 
        f.file_path,
        f.language,
        n.name as function_name,
        n.start_line,
        n.end_line,
        n.descendant_count as complexity,
        -- Count how many times this function is called (simple version)
        (
            SELECT COUNT(*) 
            FROM nodes call_node
            JOIN files call_file ON call_node.file_id = call_file.id
            WHERE call_file.status = 'current'
              AND call_node.type IN ('call_expression', 'function_call', 'method_invocation')
              AND call_node.name = n.name
        ) as call_count,
        n.source_text
    FROM nodes n
    JOIN files f ON n.file_id = f.id
    WHERE f.status = 'current'
      AND n.type IN ('function_definition', 'method_definition')
      AND (n.semantic_type & 240) = 112  -- DEFINITION
      AND n.name IS NOT NULL
      AND n.descendant_count >= complexity_threshold
)
SELECT 
    file_path,
    language,
    function_name,
    start_line,
    end_line,
    complexity,
    call_count,
    -- Simple hotspot scoring
    ROUND((complexity * 0.7 + call_count * 0.3), 2) as hotspot_score,
    source_text
FROM function_metrics
ORDER BY hotspot_score DESC
LIMIT limit_results;

-- =============================================================================
-- BASIC NAVIGATION (from queries/navigation/ - simplified)
-- =============================================================================

-- Find function calls (simple version)
CREATE OR REPLACE MACRO ast_find_calls(function_name) AS TABLE
SELECT DISTINCT
    f.file_path,
    n.name as called_function,
    n.start_line,
    n.type as call_type,
    n.depth,
    -- Find containing function for context
    (
        SELECT func.name 
        FROM nodes func
        WHERE func.file_id = n.file_id
          AND func.type IN ('function_definition', 'method_definition')
          AND func.start_line <= n.start_line
          AND func.end_line >= n.end_line
          AND (func.semantic_type & 240) = 112  -- DEFINITION
        ORDER BY func.start_line DESC
        LIMIT 1
    ) as calling_function,
    n.source_text as context
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.type IN ('call_expression', 'function_call', 'method_invocation', 'call')
  AND n.name = function_name
ORDER BY f.file_path, n.start_line;

-- Find all references to a symbol
CREATE OR REPLACE MACRO ast_find_references(symbol_name) AS TABLE
SELECT 
    f.file_path,
    f.language,
    n.name as symbol,
    n.type as node_type,
    n.start_line,
    n.end_line,
    n.depth,
    CASE 
        WHEN (n.semantic_type & 240) = 112 THEN 'DEFINITION'
        WHEN n.type IN ('call_expression', 'function_call') THEN 'CALL'
        WHEN n.type IN ('identifier', 'variable_name') THEN 'REFERENCE'
        ELSE 'OTHER'
    END as usage_type,
    n.source_text as context
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.name = symbol_name
ORDER BY f.file_path, n.start_line;

-- Get context around a symbol (surrounding code)
CREATE OR REPLACE MACRO ast_get_context(symbol_name, context_lines := 5) AS TABLE
WITH symbol_locations AS (
    SELECT 
        f.file_path,
        n.name,
        n.start_line,
        n.end_line,
        n.type,
        n.source_text
    FROM nodes n
    JOIN files f ON n.file_id = f.id
    WHERE f.status = 'current'
      AND n.name = symbol_name
),
context_nodes AS (
    SELECT 
        sl.file_path,
        sl.name as target_symbol,
        sl.start_line as target_line,
        sl.type as target_type,
        n.name,
        n.type,
        n.start_line,
        n.end_line,
        n.depth,
        ABS(n.start_line - sl.start_line) as distance,
        n.source_text
    FROM symbol_locations sl
    JOIN files f ON f.file_path = sl.file_path AND f.status = 'current'
    JOIN nodes n ON n.file_id = f.id
    WHERE n.start_line BETWEEN (sl.start_line - context_lines) AND (sl.end_line + context_lines)
      AND n.name IS NOT NULL
      AND n.name != ''
)
SELECT 
    file_path,
    target_symbol,
    target_line,
    target_type,
    name as context_symbol,
    type as context_type,
    start_line,
    end_line,
    depth,
    distance,
    source_text
FROM context_nodes
ORDER BY file_path, target_line, distance ASC, start_line ASC;

-- =============================================================================
-- IMPORT/DEPENDENCY ANALYSIS (from queries/utilities/)
-- =============================================================================

-- Find all imports/includes in codebase
CREATE OR REPLACE MACRO ast_find_imports(file_pattern := NULL) AS TABLE
SELECT 
    f.file_path,
    f.language,
    n.name as imported_symbol,
    n.type as import_type,
    n.start_line,
    n.source_text as import_statement
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.type IN (
      'import_statement', 'import_from_statement', 'import_declaration',
      'preproc_include', 'include_statement'
  )
  AND (file_pattern IS NULL OR f.file_path LIKE file_pattern)
ORDER BY f.file_path, n.start_line;

-- Find which files import a specific module/symbol
CREATE OR REPLACE MACRO ast_find_importers(module_name) AS TABLE
SELECT 
    f.file_path,
    f.language,
    n.name as imported_symbol,
    n.type as import_type,
    n.start_line,
    n.source_text
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND n.type IN (
      'import_statement', 'import_from_statement', 'import_declaration',
      'preproc_include', 'include_statement'
  )
  AND (
      n.name = module_name
      OR n.source_text ILIKE '%' || module_name || '%'
  )
ORDER BY f.file_path, n.start_line;

-- =============================================================================
-- FILE AND STRUCTURE ANALYSIS
-- =============================================================================

-- Get high-level code structure
CREATE OR REPLACE MACRO ast_file_structure(file_path) AS TABLE
SELECT 
    name,
    type as node_type,
    start_line,
    end_line,
    depth,
    children_count,
    descendant_count,
    CASE 
        WHEN (semantic_type & 240) = 112 THEN 'DEFINITION'
        WHEN (semantic_type & 240) = 128 THEN 'EXECUTION'
        WHEN (semantic_type & 240) = 144 THEN 'FLOW_CONTROL'
        ELSE 'OTHER'
    END as semantic_category
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND f.file_path = file_path
  AND depth <= 3
  AND (
      (semantic_type & 240) = 112  -- DEFINITIONS
      OR type IN ('class_definition', 'function_definition', 'if_statement', 'for_statement', 'while_statement')
  )
  AND name IS NOT NULL
  AND name != ''
ORDER BY depth, start_line;

-- Quick file overview - top-level constructs only
CREATE OR REPLACE MACRO ast_file_summary(file_path) AS TABLE
SELECT 
    type as construct_type,
    name,
    start_line,
    (end_line - start_line + 1) as lines,
    CASE 
        WHEN (semantic_type & 240) = 112 THEN 'DEFINITION'
        WHEN type IN ('import_statement', 'import_from_statement', 'include_statement') THEN 'IMPORT'
        ELSE 'OTHER'
    END as category
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
  AND f.file_path = file_path
  AND depth <= 2
  AND (
      (semantic_type & 240) = 112  -- DEFINITIONS
      OR type IN ('import_statement', 'import_from_statement', 'include_statement', 'preproc_include')
  )
  AND name IS NOT NULL
  AND name != ''
ORDER BY start_line;

-- =============================================================================
-- UTILITY FUNCTIONS
-- =============================================================================

-- List supported languages
CREATE OR REPLACE MACRO ast_supported_languages() AS TABLE
SELECT 
    language,
    COUNT(DISTINCT file_path) as file_count,
    COUNT(*) as total_nodes,
    COUNT(*) FILTER (WHERE (semantic_type & 240) = 112) as definitions
FROM nodes n
JOIN files f ON n.file_id = f.id
WHERE f.status = 'current'
GROUP BY language
ORDER BY file_count DESC;

-- Find files with many dependencies (high coupling)
CREATE OR REPLACE MACRO ast_coupled_files() AS TABLE
WITH file_imports AS (
    SELECT 
        f.file_path,
        f.language,
        COUNT(*) as import_count
    FROM nodes n
    JOIN files f ON n.file_id = f.id
    WHERE f.status = 'current'
      AND n.type IN (
          'import_statement', 'import_from_statement', 'import_declaration',
          'preproc_include', 'include_statement'
      )
    GROUP BY f.file_path, f.language
)
SELECT 
    file_path,
    language,
    import_count
FROM file_imports
WHERE import_count > 5  -- Files with more than 5 imports
ORDER BY import_count DESC;

-- Find potentially unused functions (simple heuristic)
CREATE OR REPLACE MACRO ast_unused_functions() AS TABLE
WITH all_functions AS (
    SELECT 
        f.file_path,
        n.name as function_name,
        n.start_line,
        n.descendant_count
    FROM nodes n
    JOIN files f ON n.file_id = f.id
    WHERE f.status = 'current'
      AND n.type IN ('function_definition', 'method_definition')
      AND (n.semantic_type & 240) = 112  -- DEFINITION
      AND n.name IS NOT NULL
),
called_functions AS (
    SELECT DISTINCT n.name
    FROM nodes n
    JOIN files f ON n.file_id = f.id
    WHERE f.status = 'current'
      AND n.type IN ('call_expression', 'function_call', 'method_invocation')
      AND n.name IS NOT NULL
)
SELECT 
    af.file_path,
    af.function_name,
    af.start_line,
    af.descendant_count
FROM all_functions af
LEFT JOIN called_functions cf ON af.function_name = cf.name
WHERE cf.name IS NULL  -- No calls found
  AND af.function_name NOT IN ('main', '__init__', '__del__', 'setup', 'teardown')  -- Exclude likely entry points
ORDER BY af.descendant_count DESC;

-- =============================================================================
-- HELP AND DOCUMENTATION
-- =============================================================================

-- List all available macros
CREATE OR REPLACE MACRO ast_help() AS TABLE
SELECT * FROM (VALUES 
    ('SEARCH', 'ast_search(term, type_filter, language_filter, limit)', 'Fuzzy search for symbols'),
    ('SEARCH', 'ast_find_exact(term, type_filter)', 'Exact name matching'),
    ('SEARCH', 'ast_find_functions(term, limit)', 'Find function definitions'),
    ('SEARCH', 'ast_find_classes(term, limit)', 'Find class/struct definitions'),
    ('SEARCH', 'ast_find_variables(term, limit)', 'Find variable declarations'),
    ('ANALYSIS', 'ast_complex_functions(min_nodes, limit)', 'Find complex functions'),
    ('ANALYSIS', 'ast_file_complexity()', 'File-level complexity metrics'),
    ('ANALYSIS', 'ast_hotspots(threshold, limit)', 'Find complexity hotspots'),
    ('ANALYSIS', 'ast_unused_functions()', 'Find potentially unused functions'),
    ('NAVIGATION', 'ast_find_calls(function_name)', 'Find calls to a function'),
    ('NAVIGATION', 'ast_find_references(symbol_name)', 'Find all references to symbol'),
    ('NAVIGATION', 'ast_get_context(symbol_name, lines)', 'Get context around symbol'),
    ('IMPORTS', 'ast_find_imports(file_pattern)', 'Find import/include statements'),
    ('IMPORTS', 'ast_find_importers(module_name)', 'Find files that import module'),
    ('IMPORTS', 'ast_coupled_files()', 'Find highly coupled files'),
    ('STRUCTURE', 'ast_file_structure(file_path)', 'Get file structure overview'),
    ('STRUCTURE', 'ast_file_summary(file_path)', 'Get file summary'),
    ('UTILITIES', 'ast_db_status()', 'Database status and counts'),
    ('UTILITIES', 'ast_db_stats()', 'Comprehensive database statistics'),
    ('UTILITIES', 'ast_supported_languages()', 'List supported languages'),
    ('UTILITIES', 'ast_library_info()', 'Library version and info'),
    ('HELP', 'ast_help()', 'Show this help message')
) as help(category, function_signature, description)
ORDER BY category, function_signature;

-- Welcome message
SELECT 'AST Queries Library loaded successfully! Run ast_help() to see available functions.' as message;
