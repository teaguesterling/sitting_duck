-- Peer Review Feature Implementations
-- These macros implement high-priority features from peer review feedback

-- ===================================
-- ast_get_source - Extract source code with context
-- ===================================
-- Extracts source code for a node with optional context lines before/after
CREATE OR REPLACE MACRO ast_get_source(
    source_text,
    start_line, 
    end_line,
    context_lines := 0
) AS (
    WITH 
    -- Split source into lines
    lines AS (
        SELECT 
            ROW_NUMBER() OVER () as line_num,
            line
        FROM (
            SELECT UNNEST(string_split(source_text, chr(10))) as line
        )
    ),
    -- Calculate line range with context
    line_range AS (
        SELECT 
            GREATEST(1, start_line - context_lines) as first_line,
            end_line + context_lines as last_line
    )
    -- Extract and join the lines
    (SELECT string_agg(line, chr(10) ORDER BY line_num) as source
     FROM lines, line_range
     WHERE line_num >= line_range.first_line 
       AND line_num <= line_range.last_line)
);

-- Simpler version for single nodes (uses position from node)
CREATE OR REPLACE MACRO ast_node_get_source(
    node,
    source_text,
    context_lines := 0
) AS (
    ast_get_source(
        source_text,
        CAST(node->'start'->>'line' AS INTEGER),
        CAST(node->'end'->>'line' AS INTEGER),
        context_lines := context_lines
    )
);

-- ===================================
-- ast_get_locations - Extract location information
-- ===================================
-- Returns a struct array of location objects for named nodes
CREATE OR REPLACE MACRO ast_get_locations(nodes) AS (
    [
        {
            'name': node.name,
            'type': node.type,
            'start_line': node.start_line,
            'end_line': node.end_line,
            'start_column': node.start_column,
            'end_column': node.end_column
        }
        for node in COALESCE(nodes, [])
        if node.name IS NOT NULL AND node.name != ''
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO get_locations(nodes) AS (
    ast_get_locations(nodes)
);

-- ===================================
-- ast_get_parent_chain - Get ancestors of a node
-- ===================================
-- Note: This requires parent_id to be present in nodes
-- For now, this is a placeholder that will work once we add parent tracking
CREATE OR REPLACE MACRO ast_get_parent_chain(
    nodes,
    target_node_id,
    max_depth := NULL
) AS (
    WITH RECURSIVE parent_chain AS (
        -- Start with the target node
        SELECT 
            node,
            0 as depth
        FROM json_each(nodes) as t(node)
        WHERE node->>'id' = target_node_id
        
        UNION ALL
        
        -- Find parent recursively
        SELECT 
            parent.node,
            pc.depth + 1
        FROM parent_chain pc,
             json_each(nodes) as parent(node)
        WHERE parent.node->>'id' = pc.node->>'parent_id'
          AND (max_depth IS NULL OR pc.depth < max_depth)
    )
    SELECT json_agg(node ORDER BY depth DESC) as parent_chain
    FROM parent_chain
);

-- ===================================
-- ast_get_calls - Extract function calls from a node
-- ===================================
-- Returns a struct array of function call objects
CREATE OR REPLACE MACRO ast_get_calls(nodes, root_node_id := NULL) AS (
    [
        {
            'called_function': node.name,
            'call_type': node.type,
            'line': node.start_line
        }
        for node in COALESCE(nodes, [])
        if (node.type LIKE '%call%')
           AND (root_node_id IS NULL 
                OR node.node_id = root_node_id
                OR node.parent_id = root_node_id)
    ]
);

-- Chain method version
CREATE OR REPLACE MACRO get_calls(nodes) AS (
    ast_get_calls(nodes)
);

-- ===================================
-- Helper: Check if file content is available
-- ===================================
CREATE OR REPLACE MACRO ast_has_source(node) AS (
    node->>'source_file' IS NOT NULL
);

-- ===================================
-- Convenience wrapper: Extract source from file
-- ===================================
CREATE OR REPLACE MACRO ast_extract_source(
    file_path,
    start_line,
    end_line,
    context_lines := 0
) AS (
    WITH file AS (
        SELECT 
            string_split(content, chr(10)) AS lines,
            generate_subscripts(lines, 1) AS lineno,
            UNNEST(lines) AS line
        FROM read_text(file_path)
    )
    SELECT string_agg(line, chr(10) ORDER BY lineno) AS source
    FROM file 
    WHERE lineno BETWEEN GREATEST(1, start_line - context_lines) 
                     AND end_line + context_lines
);

-- ===================================
-- Integration example: Get function with source
-- ===================================
CREATE OR REPLACE MACRO ast_get_functions_with_source(
    file_path,
    language,
    context_lines := 2
) AS (
    WITH parsed AS (
        SELECT 
            nodes,
            (SELECT content FROM read_text(file_path)) as source_text
        FROM read_ast_objects(file_path, language)
    ),
    functions AS (
        SELECT 
            node,
            source_text
        FROM parsed,
             json_each(parsed.nodes) as t(node)
        WHERE node->>'type' = 'function_definition'
           OR node->>'normalized_type' = 'function_declaration'
    )
    SELECT 
        node->>'name' as function_name,
        CAST(node->'start'->>'line' AS INTEGER) as start_line,
        CAST(node->'end'->>'line' AS INTEGER) as end_line,
        ast_node_get_source(node, source_text, context_lines) as source_code
    FROM functions
);

-- ===================================
-- ast_nodes_get_source - Extract source for nodes using embedded file_path
-- ===================================
-- This macro extracts source code for AST nodes that have file_path embedded
-- Note: This version requires passing the file content since DuckDB doesn't support dynamic file paths
CREATE OR REPLACE MACRO ast_nodes_get_source(nodes, file_content, context_lines := 0) AS (
    COALESCE(
        (SELECT json_group_array(json_object(
            'id', json_extract_string(je.value, '$.id'),
            'type', json_extract_string(je.value, '$.type'),
            'name', json_extract_string(je.value, '$.name'),
            'file_path', json_extract_string(je.value, '$.file_path'),
            'start_line', CAST(json_extract(je.value, '$.start.line') AS INTEGER),
            'end_line', CAST(json_extract(je.value, '$.end.line') AS INTEGER),
            'source', ast_get_source(
                file_content,
                CAST(json_extract(je.value, '$.start.line') AS INTEGER),
                CAST(json_extract(je.value, '$.end.line') AS INTEGER),
                context_lines := context_lines
            )
        ))
        FROM json_each(COALESCE(nodes, '[]'::JSON)) AS je
        WHERE json_extract_string(je.value, '$.file_path') IS NOT NULL),
        '[]'::JSON
    )
);

-- Chain method version for extracting source
-- Note: This requires the file content to be passed separately
CREATE OR REPLACE MACRO get_source(nodes, file_content, context_lines := 0) AS (
    ast_nodes_get_source(nodes, file_content, context_lines := context_lines)
);

-- ===================================
-- Helper macro for common case: nodes from read_ast_objects
-- ===================================
-- This macro works when you have the result from read_ast_objects
-- It reads the file content once and applies source extraction
CREATE OR REPLACE MACRO ast_with_source(file_path, language, node_filter := NULL, context_lines := 0) AS (
    WITH parsed AS (
        SELECT 
            nodes,
            (SELECT content FROM read_text(file_path)) as file_content
        FROM read_ast_objects(file_path, language)
    ),
    filtered_nodes AS (
        SELECT 
            CASE 
                WHEN node_filter IS NOT NULL THEN
                    (SELECT json_group_array(value)
                     FROM json_each(nodes::JSON)
                     WHERE json_extract_string(value, '$.type') = node_filter)
                ELSE nodes::JSON
            END as nodes,
            file_content
        FROM parsed
    )
    SELECT ast_nodes_get_source(nodes, file_content, context_lines) as nodes_with_source
    FROM filtered_nodes
);