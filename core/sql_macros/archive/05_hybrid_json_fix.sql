-- Macro to create properly escaped JSON from AST data
CREATE OR REPLACE MACRO ast_to_json(file_path, language) AS (
    SELECT json_group_array(
        json_object(
            'id', node_id,
            'type', type,
            'name', name,
            'start', json_object('line', start_line, 'column', start_column),
            'end', json_object('line', end_line, 'column', end_column),
            'parent_id', parent_id,
            'depth', depth,
            'sibling_index', sibling_index
        )
    ) as nodes
    FROM read_ast(file_path, language)
);

-- Test macro to verify JSON escaping works correctly
CREATE OR REPLACE MACRO test_json_escaping(text_value) AS (
    SELECT to_json(text_value) as escaped_json
);