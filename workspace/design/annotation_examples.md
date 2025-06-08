# Annotation System: Practical Examples

## Example 1: Progressive Enhancement

```sql
-- Start with minimal AST for performance
CREATE TABLE project_ast AS
SELECT file_path, nodes 
FROM read_ast_objects('src/**/*.py', 'python', annotations := 'minimal');

-- Later, enrich specific files with signatures
WITH enriched AS (
    SELECT 
        file_path,
        ast(nodes).with_annotations('signatures').nodes as nodes
    FROM project_ast
    WHERE file_path LIKE '%/api/%'
)
SELECT * FROM enriched, 
    LATERAL (SELECT * FROM ast(nodes).get_signatures()) sigs;
```

## Example 2: Custom Security Annotation

```sql
-- Define a security scanner annotation
CREATE OR REPLACE MACRO annotate_security_risks(nodes) AS (
    SELECT json_array_agg(
        CASE 
            -- Check for dangerous functions
            WHEN node->>'type' = 'call_expression' 
                AND node->'annotations'->>'extracted_name' IN ('eval', 'exec', '__import__') 
            THEN json_patch(node, json_object(
                'annotations', json_merge_patch(
                    node->'annotations',
                    json_object('security', json_object(
                        'risk_level', 'high',
                        'issue_type', 'dangerous_function',
                        'message', 'Use of ' || node->'annotations'->>'extracted_name' || ' is a security risk'
                    ))
                )
            ))
            -- Check for SQL in strings
            WHEN node->>'type' = 'string' 
                AND upper(node->>'text') LIKE '%SELECT%FROM%' 
            THEN json_patch(node, json_object(
                'annotations', json_merge_patch(
                    node->'annotations',
                    json_object('security', json_object(
                        'risk_level', 'medium',
                        'issue_type', 'possible_sql_injection',
                        'message', 'Possible SQL in string literal'
                    ))
                )
            ))
            ELSE node
        END
    ) 
    FROM json_each(nodes) as t(node)
);

-- Use it in analysis
SELECT 
    file_path,
    ast(nodes)
        .with_annotations('standard')
        .with_annotations(annotate_security_risks)
        .filter_by_annotation('security.risk_level', 'high')
        .get_locations() as security_issues
FROM read_ast_objects('**/*.py', 'python');
```

## Example 3: Team-Specific Coding Standards

```sql
-- Create a coding standards checker
CREATE OR REPLACE MACRO check_naming_conventions(nodes) AS (
    SELECT json_array_agg(
        CASE 
            -- Check function naming
            WHEN node->'annotations'->>'normalized_type' = 'function_declaration'
                AND node->'annotations'->>'extracted_name' NOT SIMILAR TO '[a-z_][a-z0-9_]*'
            THEN json_patch(node, json_object(
                'annotations', json_merge_patch(
                    node->'annotations',
                    json_object('standards', json_object(
                        'compliant', false,
                        'issues', json_array('function_name_not_snake_case')
                    ))
                )
            ))
            -- Check class naming
            WHEN node->'annotations'->>'normalized_type' = 'class_declaration'
                AND node->'annotations'->>'extracted_name' NOT SIMILAR TO '[A-Z][a-zA-Z0-9]*'
            THEN json_patch(node, json_object(
                'annotations', json_merge_patch(
                    node->'annotations',
                    json_object('standards', json_object(
                        'compliant', false,
                        'issues', json_array('class_name_not_pascal_case')
                    ))
                )
            ))
            ELSE json_patch(node, json_object(
                'annotations', json_merge_patch(
                    node->'annotations',
                    json_object('standards', json_object('compliant', true, 'issues', json_array()))
                )
            ))
        END
    )
    FROM json_each(nodes) as t(node)
    WHERE node->'annotations'->>'normalized_type' IN ('function_declaration', 'class_declaration')
);

-- Generate report
WITH analyzed AS (
    SELECT ast(nodes)
        .with_annotations('standard')
        .with_annotations(check_naming_conventions)
        .nodes as nodes
    FROM read_ast_objects('src/**/*.py', 'python')
)
SELECT 
    annotations->>'extracted_name' as name,
    type,
    annotations->'standards'->>'compliant' as compliant,
    annotations->'standards'->'issues' as issues
FROM analyzed, json_each(nodes) as t(node)
WHERE annotations->'standards'->>'compliant' = 'false';
```

## Example 4: Cross-Language Analysis

```sql
-- Create a universal complexity annotator
CREATE OR REPLACE MACRO annotate_complexity(nodes) AS (
    SELECT json_array_agg(
        CASE 
            WHEN node->'annotations'->>'normalized_type' IN ('function_declaration', 'method_declaration')
            THEN json_patch(node, json_object(
                'annotations', json_merge_patch(
                    node->'annotations',
                    json_object('metrics', json_object(
                        'line_count', 
                            (node->'position'->>'end_row')::int - 
                            (node->'position'->>'start_row')::int + 1,
                        'complexity_estimate',
                            -- Simple heuristic: count control flow statements
                            (
                                SELECT COUNT(*)
                                FROM json_each(node->'children') as child(c)
                                WHERE c->'annotations'->>'normalized_type' IN (
                                    'if_statement', 'loop_statement', 'return_statement'
                                )
                            )
                    ))
                )
            ))
            ELSE node
        END
    )
    FROM json_each(nodes) as t(node)
);

-- Analyze across multiple languages
WITH all_code AS (
    SELECT 'python' as language, nodes FROM read_ast_objects('**/*.py', 'python')
    UNION ALL
    SELECT 'javascript' as language, nodes FROM read_ast_objects('**/*.js', 'javascript')
    UNION ALL
    SELECT 'cpp' as language, nodes FROM read_ast_objects('**/*.cpp', 'cpp')
)
SELECT 
    language,
    annotations->>'extracted_name' as function_name,
    annotations->'metrics'->>'line_count' as lines,
    annotations->'metrics'->>'complexity_estimate' as complexity
FROM all_code,
    LATERAL (
        SELECT * FROM json_each(
            ast(nodes)
                .with_annotations('standard')
                .with_annotations(annotate_complexity)
                .filter_by_type(['function_declaration', 'method_declaration'])
                .nodes
        )
    ) as t(node)
WHERE (annotations->'metrics'->>'line_count')::int > 50
ORDER BY complexity DESC;
```

## Example 5: Building a Dependency Graph

```sql
-- Annotate with dependencies
CREATE OR REPLACE MACRO annotate_dependencies(nodes) AS (
    -- This is simplified; real implementation would traverse the tree
    SELECT json_array_agg(
        CASE 
            WHEN node->'annotations'->>'normalized_type' = 'function_declaration'
            THEN json_patch(node, json_object(
                'annotations', json_merge_patch(
                    node->'annotations',
                    json_object('dependencies', json_object(
                        'calls', (
                            -- Extract function calls within this function
                            SELECT json_array_agg(
                                child->'annotations'->>'extracted_name'
                            )
                            FROM json_each(node->'children') as t(child)
                            WHERE child->'annotations'->>'normalized_type' = 'function_call'
                        ),
                        'imports', json_array()  -- Would be populated for module-level
                    ))
                )
            ))
            ELSE node
        END
    )
    FROM json_each(nodes) as t(node)
);

-- Create dependency graph
WITH deps AS (
    SELECT 
        annotations->>'extracted_name' as function_name,
        annotations->'dependencies'->'calls' as calls
    FROM read_ast_objects('module.py', 'python', annotations := 'standard'),
        LATERAL json_each(
            ast(nodes)
                .with_annotations(annotate_dependencies)
                .filter_by_type(['function_declaration'])
                .nodes
        ) as t(node)
)
SELECT 
    d.function_name as caller,
    json_array_elements_text(d.calls) as callee
FROM deps d
WHERE d.calls != 'null';