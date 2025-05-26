-- AI-Friendly Macros and Shortcuts
-- These provide semantic operations and discovery functions

-- Discover available node types in AST
CREATE OR REPLACE MACRO ast_available_types(ast_obj) AS (
    SELECT LIST(DISTINCT json_extract_string(je.value, '$.type') ORDER BY 1)
    FROM json_each(ast_obj.nodes) AS je
);

-- Find test functions (common patterns)
CREATE OR REPLACE MACRO ast_test_functions(
    ast_obj, 
    patterns := ['test_%', '%_test', 'Test%', '%Test']
) AS (
    WITH filtered_types AS (
        SELECT ast_filter_type(ast_obj, ['function_definition', 'method_definition']) as filtered
    ),
    pattern_array AS (
        SELECT ensure_varchar_array(patterns) as pattern_list
    ),
    test_nodes AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(filtered_types.filtered.nodes) AS je, pattern_array
        WHERE EXISTS (
            SELECT 1 FROM unnest(pattern_array.pattern_list) as pattern
            WHERE LOWER(json_extract_string(je.value, '$.name')) LIKE LOWER(pattern)
        )
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(test_nodes.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(test_nodes.nodes) as n), 
            0
        ),
        nodes := COALESCE(test_nodes.nodes, '[]'::JSON)
    )
    FROM filtered_types, test_nodes
);

-- Find public API (top-level non-private entities)
CREATE OR REPLACE MACRO ast_public_api(ast_obj, max_depth := 2) AS (
    WITH filtered AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.type') IN ('function_definition', 'class_definition')
          AND json_extract(je.value, '$.depth')::INTEGER <= max_depth
          AND NOT (json_extract_string(je.value, '$.name') LIKE '\_%' 
                   OR json_extract_string(je.value, '$.name') LIKE '_%')
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(filtered.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(filtered.nodes) as n), 
            0
        ),
        nodes := COALESCE(filtered.nodes, '[]'::JSON)
    )
    FROM filtered
);

-- Find methods (functions inside classes)
CREATE OR REPLACE MACRO ast_methods(ast_obj, include_private := true) AS (
    WITH class_ids AS (
        SELECT json_extract(je.value, '$.id')::BIGINT as class_id
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.type') = 'class_definition'
    ),
    method_nodes AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.type') IN ('function_definition', 'method_definition')
          AND EXISTS (
              SELECT 1 FROM class_ids
              WHERE ast_is_descendant_of(
                  ast_obj.nodes,
                  json_extract(je.value, '$.id')::BIGINT,
                  class_ids.class_id
              )
          )
          AND (include_private OR NOT json_extract_string(je.value, '$.name') LIKE '\_%')
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(method_nodes.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(method_nodes.nodes) as n), 
            0
        ),
        nodes := COALESCE(method_nodes.nodes, '[]'::JSON)
    )
    FROM method_nodes
);

-- Helper: Check if node is descendant of another
CREATE OR REPLACE MACRO ast_is_descendant_of(nodes, child_id, ancestor_id) AS (
    WITH RECURSIVE path AS (
        SELECT 
            json_extract(je.value, '$.id')::BIGINT as id,
            json_extract(je.value, '$.parent_id')::BIGINT as parent_id
        FROM json_each(nodes) AS je
        WHERE json_extract(je.value, '$.id')::BIGINT = child_id
        
        UNION ALL
        
        SELECT 
            p.id,
            json_extract(je.value, '$.parent_id')::BIGINT as parent_id
        FROM path p
        JOIN json_each(nodes) AS je 
          ON json_extract(je.value, '$.id')::BIGINT = p.parent_id
        WHERE p.parent_id IS NOT NULL
    )
    SELECT COUNT(*) > 0
    FROM path
    WHERE parent_id = ancestor_id OR id = ancestor_id
);

-- Find error handlers (try/except/catch blocks)
CREATE OR REPLACE MACRO ast_error_handlers(ast_obj) AS (
    ast_filter_type(
        ast_obj, 
        ['try_statement', 'except_clause', 'catch_clause', 'finally_clause']
    )
);

-- Find imports
CREATE OR REPLACE MACRO ast_imports(ast_obj) AS (
    ast_filter_type(
        ast_obj,
        ['import_statement', 'import_from', 'require', 'include']
    )
);

-- Find TODO/FIXME comments
CREATE OR REPLACE MACRO ast_todos(ast_obj) AS (
    WITH todo_nodes AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je
        WHERE json_extract_string(je.value, '$.type') IN ('comment', 'string')
          AND (UPPER(json_extract_string(je.value, '$.content')) LIKE '%TODO%'
               OR UPPER(json_extract_string(je.value, '$.content')) LIKE '%FIXME%')
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := COALESCE(json_array_length(todo_nodes.nodes), 0),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(todo_nodes.nodes) as n), 
            0
        ),
        nodes := COALESCE(todo_nodes.nodes, '[]'::JSON)
    )
    FROM todo_nodes
);

-- Get code quality indicators
CREATE OR REPLACE MACRO ast_code_quality(ast_obj) AS (
    WITH metrics AS (
        SELECT 
            ast_extract_summary(ast_obj) as summary,
            ast_extract_summary(ast_test_functions(ast_obj)) as test_summary,
            ast_extract_summary(ast_todos(ast_obj)) as todo_summary,
            ast_extract_summary(ast_public_api(ast_obj)) as api_summary
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        total_functions := metrics.summary.function_count,
        test_functions := metrics.test_summary.function_count,
        test_coverage_pct := ROUND(
            100.0 * metrics.test_summary.function_count / 
            NULLIF(metrics.summary.function_count, 0), 1
        ),
        todos_count := metrics.todo_summary.total_nodes,
        public_api_count := metrics.api_summary.function_count + metrics.api_summary.class_count,
        max_depth := metrics.summary.max_depth,
        avg_depth := metrics.summary.avg_depth,
        lines_of_code := metrics.summary.lines_of_code,
        complexity_rating := CASE
            WHEN metrics.summary.max_depth > 10 THEN 'High'
            WHEN metrics.summary.max_depth > 6 THEN 'Medium'
            ELSE 'Low'
        END
    )
    FROM metrics
);

-- Suggest relevant macros based on intent (simplified)
CREATE OR REPLACE MACRO ast_suggest_macro(intent) AS (
    WITH suggestions AS (
        SELECT 
            CASE LOWER(intent)
                WHEN intent LIKE '%function%' THEN 
                    ['ast_filter_type', 'ast_extract_function_names', 'ast_methods']
                WHEN intent LIKE '%test%' THEN 
                    ['ast_test_functions', 'ast_filter_name_pattern']
                WHEN intent LIKE '%class%' THEN 
                    ['ast_filter_type', 'ast_extract_class_names', 'ast_methods']
                WHEN intent LIKE '%import%' THEN 
                    ['ast_imports', 'ast_filter_type']
                WHEN intent LIKE '%error%' OR intent LIKE '%exception%' THEN 
                    ['ast_error_handlers', 'ast_filter_type']
                WHEN intent LIKE '%source%' OR intent LIKE '%code%' THEN 
                    ['ast_extract_source', 'ast_extract_entities']
                WHEN intent LIKE '%quality%' OR intent LIKE '%metric%' THEN 
                    ['ast_code_quality', 'ast_extract_summary', 'ast_complexity']
                ELSE 
                    ['ast_filter_type', 'ast_extract_entities', 'ast_extract_summary']
            END as macros
    )
    SELECT macros FROM suggestions
);