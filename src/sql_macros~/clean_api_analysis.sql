-- Clean API Analysis Functions
-- Analysis and metrics functions with consistent naming

-- ===================================
-- 5. ANALYSIS (ast_analyze_*)
-- ===================================

-- Overall AST statistics and summary
CREATE OR REPLACE MACRO ast_analyze_summary(nodes) AS (
    json_object(
        'total_nodes', json_array_length(COALESCE(nodes, '[]'::JSON)),
        'node_types', ast_analyze_types(nodes),
        'max_depth', COALESCE(
            (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
             FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je),
            0
        ),
        'functions', ast_get_names(nodes, 'function_definition'),
        'classes', ast_get_names(nodes, 'class_definition'), 
        'function_count', json_array_length(ast_get_type(nodes, 'function_definition')),
        'class_count', json_array_length(ast_get_type(nodes, 'class_definition')),
        'has_names_count', json_array_length(ast_filter_has_name(nodes)),
        'line_span', CASE 
            WHEN json_array_length(COALESCE(nodes, '[]'::JSON)) = 0 THEN NULL
            ELSE json_object(
                'start', (SELECT MIN(json_extract(je.value, '$.start.line')::INTEGER) 
                         FROM json_each(nodes) AS je),
                'end', (SELECT MAX(json_extract(je.value, '$.end.line')::INTEGER) 
                       FROM json_each(nodes) AS je)
            )
        END
    )
);

-- Complexity metrics for code analysis
CREATE OR REPLACE MACRO ast_analyze_complexity(nodes) AS (
    json_object(
        'total_nodes', json_array_length(COALESCE(nodes, '[]'::JSON)),
        'avg_depth', COALESCE(
            (SELECT AVG(json_extract(je.value, '$.depth')::INTEGER) 
             FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je),
            0.0
        ),
        'max_depth', COALESCE(
            (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
             FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je),
            0
        ),
        'depth_distribution', (
            SELECT json_group_object(
                CAST(json_extract(je.value, '$.depth')::INTEGER AS VARCHAR), 
                COUNT(*)
            )
            FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
            GROUP BY json_extract(je.value, '$.depth')::INTEGER
        ),
        'function_count', json_array_length(ast_get_type(nodes, 'function_definition')),
        'class_count', json_array_length(ast_get_type(nodes, 'class_definition')),
        'lines_of_code', CASE 
            WHEN json_array_length(COALESCE(nodes, '[]'::JSON)) = 0 THEN 0
            ELSE COALESCE(
                (SELECT MAX(json_extract(je.value, '$.end.line')::INTEGER) 
                 FROM json_each(nodes) AS je),
                0
            )
        END,
        'complexity_score', CASE 
            WHEN json_array_length(COALESCE(nodes, '[]'::JSON)) = 0 THEN 0
            ELSE COALESCE(
                (SELECT AVG(json_extract(je.value, '$.depth')::INTEGER) * 
                        COUNT(*) / 100.0
                 FROM json_each(nodes) AS je),
                0.0
            )
        END
    )
);

-- Count of each node type
CREATE OR REPLACE MACRO ast_analyze_types(nodes) AS (
    COALESCE(
        (SELECT json_group_object(node_type, cnt)
         FROM (
             SELECT json_extract_string(je.value, '$.type') as node_type, COUNT(*) as cnt
             FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
             WHERE json_extract_string(je.value, '$.type') IS NOT NULL
             GROUP BY node_type
             ORDER BY cnt DESC
         )),
        '{}'::JSON
    )
);

-- Depth analysis - distribution of nodes across depth levels
CREATE OR REPLACE MACRO ast_analyze_depth_distribution(nodes) AS (
    COALESCE(
        (SELECT json_group_object(
            'depth_' || CAST(depth AS VARCHAR), 
            json_object(
                'count', cnt,
                'percentage', ROUND(cnt * 100.0 / total_nodes, 2)
            )
        )
         FROM (
             SELECT 
                 json_extract(je.value, '$.depth')::INTEGER as depth,
                 COUNT(*) as cnt,
                 SUM(COUNT(*)) OVER () as total_nodes
             FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
             GROUP BY depth
             ORDER BY depth
         )),
        '{}'::JSON
    )
);

-- Line coverage analysis - which lines have AST nodes
CREATE OR REPLACE MACRO ast_analyze_line_coverage(nodes) AS (
    json_object(
        'total_lines', CASE 
            WHEN json_array_length(COALESCE(nodes, '[]'::JSON)) = 0 THEN 0
            ELSE COALESCE(
                (SELECT MAX(json_extract(je.value, '$.end.line')::INTEGER) 
                 FROM json_each(nodes) AS je),
                0
            )
        END,
        'covered_lines', (
            SELECT COUNT(DISTINCT line_num)
            FROM (
                SELECT generate_series(
                    json_extract(je.value, '$.start.line')::INTEGER,
                    json_extract(je.value, '$.end.line')::INTEGER
                ) as line_num
                FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
            )
        ),
        'coverage_percentage', CASE 
            WHEN json_array_length(COALESCE(nodes, '[]'::JSON)) = 0 THEN 0.0
            ELSE COALESCE(
                (SELECT COUNT(DISTINCT line_num) * 100.0 / MAX(json_extract(je.value, '$.end.line')::INTEGER)
                 FROM (
                     SELECT generate_series(
                         json_extract(je.value, '$.start.line')::INTEGER,
                         json_extract(je.value, '$.end.line')::INTEGER
                     ) as line_num
                     FROM json_each(nodes) AS je
                 ), json_each(nodes) AS je),
                0.0
            )
        END
    )
);