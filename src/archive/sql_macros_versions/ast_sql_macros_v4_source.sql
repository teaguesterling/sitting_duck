-- SQL Macros for Source Code Integration
-- These handle file reading and source extraction

-- Read file lines with line numbers (improved version)
CREATE OR REPLACE MACRO read_file_lines(file_path, start_line := 1, end_line := 2147483647) AS (
    WITH file_content AS (
        SELECT 
            filename,
            size as filesize,
            last_modified,
            string_split(content, E'\n') as lines
        FROM read_text(file_path)
    ),
    numbered_lines AS (
        SELECT 
            filename,
            filesize,
            last_modified,
            generate_series(1, array_length(lines)) as lineno,
            lines[generate_series(1, array_length(lines))] as line
        FROM file_content
    )
    SELECT 
        filename,
        filesize,
        last_modified,
        lineno,
        line,
        length(line) as line_length
    FROM numbered_lines
    WHERE lineno BETWEEN start_line AND end_line
    ORDER BY lineno
);

-- Get source text with optional line numbers
CREATE OR REPLACE MACRO get_source_text(file_path, start_line, end_line, include_line_numbers := true) AS (
    SELECT string_agg(
        CASE 
            WHEN include_line_numbers 
            THEN format('{:4d}: {}', lineno, line)
            ELSE line
        END,
        E'\n'
        ORDER BY lineno
    ) as source_text
    FROM read_file_lines(file_path, start_line, end_line)
);

-- Extract source code for AST nodes with context
CREATE OR REPLACE MACRO ast_extract_source(
    ast_obj,
    node_ids := NULL,
    pad_lines := 0,
    min_lines := 0,
    include_line_numbers := true
) AS (
    SELECT 
        ast_obj.file_path,
        json_extract(je.value, '$.id')::BIGINT as node_id,
        json_extract_string(je.value, '$.type') as node_type,
        json_extract_string(je.value, '$.name') as node_name,
        json_extract(je.value, '$.start.line')::INTEGER as start_line,
        json_extract(je.value, '$.end.line')::INTEGER as end_line,
        json_extract(je.value, '$.end.line')::INTEGER - 
            json_extract(je.value, '$.start.line')::INTEGER + 1 as line_count,
        GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines) as context_start,
        json_extract(je.value, '$.end.line')::INTEGER + pad_lines as context_end,
        -- Original source without padding
        get_source_text(
            ast_obj.file_path,
            json_extract(je.value, '$.start.line')::INTEGER,
            json_extract(je.value, '$.end.line')::INTEGER,
            include_line_numbers
        ) as source_text,
        -- Source with context padding
        get_source_text(
            ast_obj.file_path,
            GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines),
            json_extract(je.value, '$.end.line')::INTEGER + pad_lines,
            include_line_numbers
        ) as source_with_context
    FROM json_each(ast_obj.nodes) AS je
    WHERE 
        -- Filter by line count if specified
        (min_lines = 0 OR 
         json_extract(je.value, '$.end.line')::INTEGER - 
         json_extract(je.value, '$.start.line')::INTEGER + 1 >= min_lines)
        -- Filter by node IDs if specified
        AND (node_ids IS NULL OR 
             list_contains(node_ids, json_extract(je.value, '$.id')::BIGINT))
);

-- Extract source for specific node types
CREATE OR REPLACE MACRO ast_extract_typed_source(
    ast_obj,
    node_types,
    pad_lines := 0,
    include_line_numbers := true
) AS (
    SELECT *
    FROM ast_extract_source(
        ast_filter_type(ast_obj, node_types),
        pad_lines := pad_lines,
        include_line_numbers := include_line_numbers
    )
);

-- Extract source matching name pattern
CREATE OR REPLACE MACRO ast_extract_matching_source(
    ast_obj,
    name_pattern,
    pad_lines := 0,
    include_line_numbers := true
) AS (
    SELECT *
    FROM ast_extract_source(
        ast_filter_name_pattern(ast_obj, name_pattern),
        pad_lines := pad_lines,
        include_line_numbers := include_line_numbers
    )
);

-- Get file statistics for AST analysis
CREATE OR REPLACE MACRO ast_file_stats(file_path) AS (
    WITH file_info AS (
        SELECT 
            filename,
            size as filesize,
            last_modified,
            array_length(string_split(content, E'\n')) as total_lines,
            length(content) as total_chars
        FROM read_text(file_path)
    ),
    line_stats AS (
        SELECT 
            MAX(line_length) as max_line_length,
            AVG(line_length)::INTEGER as avg_line_length,
            COUNT(*) FILTER (WHERE trim(line) = '') as empty_lines,
            COUNT(*) FILTER (WHERE line LIKE '%TODO%' OR line LIKE '%FIXME%') as todo_lines
        FROM read_file_lines(file_path)
    )
    SELECT 
        f.filename,
        f.filesize,
        f.last_modified,
        f.total_lines,
        f.total_chars,
        l.max_line_length,
        l.avg_line_length,
        l.empty_lines,
        l.todo_lines,
        ROUND(100.0 * l.empty_lines / f.total_lines, 2) as empty_line_pct
    FROM file_info f, line_stats l
);

-- Build comprehensive code entity table with source
CREATE OR REPLACE MACRO ast_code_entities_with_source(
    ast_obj,
    pad_lines := 2,
    include_private := false
) AS (
    WITH entities AS (
        SELECT *
        FROM ast_extract_entities(
            ast_obj,
            types := ['function_definition', 'class_definition', 'method_definition']
        )
        WHERE include_private OR NOT entity_name LIKE '_%'
    )
    SELECT 
        e.*,
        s.line_count,
        s.source_text,
        s.source_with_context
    FROM entities e
    JOIN ast_extract_source(
        ast_obj,
        node_ids := LIST(e.node_id),
        pad_lines := pad_lines
    ) s ON e.node_id = s.node_id
);

-- Example usage: Find long functions with their source
-- SELECT * 
-- FROM read_ast_objects('src/*.py', 'python') as ast_obj,
--      TABLE(ast_code_entities_with_source(
--          ast_obj, 
--          pad_lines := 3
--      )) as entity
-- WHERE entity_type = 'function_definition' 
--   AND line_count > 50
-- ORDER BY line_count DESC;