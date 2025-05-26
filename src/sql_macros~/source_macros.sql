-- SQL Macros for Source Code Integration - Final Version
-- Using list comprehension approach for efficient line numbering

-- Read file lines with line numbers (using list comprehension)
CREATE OR REPLACE MACRO read_file_lines(file_path, start_line := 1, end_line := 2147483647) AS (
    SELECT 
        filename,
        size AS filesize,
        last_modified AS file_last_modified,
        line_info.lineno AS line_number,
        line_info.content AS line,
        length(line_info.content) AS line_length
    FROM (
        SELECT 
            filename,
            size,
            last_modified,
            unnest([{
                'lineno': i, 
                'content': lines[i]
            } for i in generate_series(1, array_length(lines))]) AS line_info
        FROM (
            SELECT *, string_split(content, E'\n') AS lines
            FROM read_text(file_path)
        )
    )
    WHERE line_info.lineno BETWEEN start_line AND end_line
    ORDER BY line_info.lineno
);

-- Get source text with optional line numbers and formatting
CREATE OR REPLACE MACRO get_source_text(
    file_path, 
    start_line, 
    end_line, 
    include_line_numbers := true,
    line_format := '{:4d}: {}'
) AS (
    SELECT string_agg(
        CASE 
            WHEN include_line_numbers 
            THEN format(line_format, line_number, line)
            ELSE line
        END,
        E'\n'
        ORDER BY line_number
    ) AS source_text
    FROM read_file_lines(file_path, start_line, end_line)
);

-- Extract source code for AST nodes with context
CREATE OR REPLACE MACRO ast_extract_source(
    ast_obj,
    node_ids := NULL,
    pad_lines := 0,
    min_lines := 0,
    include_line_numbers := true,
    highlight_range := false
) AS (
    SELECT 
        ast_obj.file_path,
        json_extract(je.value, '$.id')::BIGINT AS node_id,
        json_extract_string(je.value, '$.type') AS node_type,
        json_extract_string(je.value, '$.name') AS node_name,
        json_extract(je.value, '$.start.line')::INTEGER AS start_line,
        json_extract(je.value, '$.end.line')::INTEGER AS end_line,
        json_extract(je.value, '$.start.column')::INTEGER AS start_column,
        json_extract(je.value, '$.end.column')::INTEGER AS end_column,
        json_extract(je.value, '$.end.line')::INTEGER - 
            json_extract(je.value, '$.start.line')::INTEGER + 1 AS line_count,
        GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines) AS context_start,
        json_extract(je.value, '$.end.line')::INTEGER + pad_lines AS context_end,
        -- Original source without padding
        get_source_text(
            ast_obj.file_path,
            json_extract(je.value, '$.start.line')::INTEGER,
            json_extract(je.value, '$.end.line')::INTEGER,
            include_line_numbers
        ) AS source_text,
        -- Source with context padding
        CASE 
            WHEN highlight_range THEN
                -- Add visual markers for the actual node range
                CONCAT(
                    CASE WHEN pad_lines > 0 THEN
                        get_source_text(
                            ast_obj.file_path,
                            GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines),
                            json_extract(je.value, '$.start.line')::INTEGER - 1,
                            include_line_numbers
                        ) || E'\n' || REPEAT('-', 40) || ' [START] ' || REPEAT('-', 40) || E'\n'
                    ELSE '' END,
                    get_source_text(
                        ast_obj.file_path,
                        json_extract(je.value, '$.start.line')::INTEGER,
                        json_extract(je.value, '$.end.line')::INTEGER,
                        include_line_numbers
                    ),
                    CASE WHEN pad_lines > 0 THEN
                        E'\n' || REPEAT('-', 40) || ' [END] ' || REPEAT('-', 40) || E'\n' ||
                        get_source_text(
                            ast_obj.file_path,
                            json_extract(je.value, '$.end.line')::INTEGER + 1,
                            json_extract(je.value, '$.end.line')::INTEGER + pad_lines,
                            include_line_numbers
                        )
                    ELSE '' END
                )
            ELSE
                get_source_text(
                    ast_obj.file_path,
                    GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines),
                    json_extract(je.value, '$.end.line')::INTEGER + pad_lines,
                    include_line_numbers
                )
        END AS source_with_context
    FROM json_each(ast_obj.nodes) AS je
    WHERE 
        -- Filter by line count if specified
        (min_lines = 0 OR 
         json_extract(je.value, '$.end.line')::INTEGER - 
         json_extract(je.value, '$.start.line')::INTEGER + 1 >= min_lines)
        -- Filter by node IDs if specified
        AND (node_ids IS NULL OR 
             CASE 
                WHEN pg_typeof(node_ids) = 'BIGINT[]' THEN
                    list_contains(node_ids, json_extract(je.value, '$.id')::BIGINT)
                WHEN pg_typeof(node_ids) = 'BIGINT' THEN
                    json_extract(je.value, '$.id')::BIGINT = node_ids
                ELSE false
             END)
);

-- Get file statistics with code metrics
CREATE OR REPLACE MACRO ast_file_stats(file_path) AS (
    WITH file_metrics AS (
        SELECT 
            filename,
            filesize,
            file_last_modified,
            COUNT(*) AS total_lines,
            COUNT(*) FILTER (WHERE trim(line) = '') AS empty_lines,
            COUNT(*) FILTER (WHERE line LIKE '%TODO%' OR line LIKE '%FIXME%') AS todo_lines,
            COUNT(*) FILTER (WHERE trim(line) LIKE '#%' OR trim(line) LIKE '//%') AS comment_lines,
            MAX(line_length) AS max_line_length,
            AVG(line_length)::INTEGER AS avg_line_length,
            SUM(line_length) AS total_chars
        FROM read_file_lines(file_path)
        GROUP BY filename, filesize, file_last_modified
    )
    SELECT 
        filename,
        filesize,
        file_last_modified,
        total_lines,
        total_lines - empty_lines AS non_empty_lines,
        empty_lines,
        comment_lines,
        todo_lines,
        ROUND(100.0 * empty_lines / total_lines, 2) AS empty_line_pct,
        ROUND(100.0 * comment_lines / total_lines, 2) AS comment_line_pct,
        max_line_length,
        avg_line_length,
        total_chars
    FROM file_metrics
);

-- Find and extract TODO/FIXME comments with context
CREATE OR REPLACE MACRO ast_extract_todos(ast_obj, context_lines := 2) AS (
    WITH todo_lines AS (
        SELECT 
            line_number,
            line,
            line LIKE '%TODO%' AS is_todo,
            line LIKE '%FIXME%' AS is_fixme
        FROM read_file_lines(ast_obj.file_path)
        WHERE line LIKE '%TODO%' OR line LIKE '%FIXME%'
    )
    SELECT 
        ast_obj.file_path,
        tl.line_number,
        CASE 
            WHEN tl.is_todo AND tl.is_fixme THEN 'TODO+FIXME'
            WHEN tl.is_todo THEN 'TODO'
            ELSE 'FIXME'
        END AS todo_type,
        trim(tl.line) AS todo_text,
        get_source_text(
            ast_obj.file_path,
            GREATEST(1, tl.line_number - context_lines),
            tl.line_number + context_lines,
            true
        ) AS context
    FROM todo_lines tl
    ORDER BY tl.line_number
);

-- Build code review table with source snippets
CREATE OR REPLACE MACRO ast_code_review_table(
    ast_obj,
    complexity_threshold := 10,
    length_threshold := 50
) AS (
    WITH complex_entities AS (
        SELECT *
        FROM ast_extract_entities(
            ast_obj,
            types := ['function_definition', 'class_definition', 'method_definition']
        )
        WHERE (end_line - start_line + 1) > length_threshold
    ),
    entity_metrics AS (
        SELECT 
            ce.*,
            ce.end_line - ce.start_line + 1 AS line_count,
            -- Calculate cyclomatic complexity estimate (simplified)
            (SELECT COUNT(*) 
             FROM json_each(ast_obj.nodes) AS je
             WHERE json_extract_string(je.value, '$.type') IN 
                   ('if_statement', 'while_statement', 'for_statement', 'except_clause')
               AND json_extract(je.value, '$.start.line')::INTEGER >= ce.start_line
               AND json_extract(je.value, '$.end.line')::INTEGER <= ce.end_line
            ) + 1 AS estimated_complexity
        FROM complex_entities ce
    )
    SELECT 
        em.*,
        src.source_with_context,
        fs.todo_lines,
        CASE 
            WHEN em.estimated_complexity > complexity_threshold THEN 'HIGH'
            WHEN em.estimated_complexity > complexity_threshold / 2 THEN 'MEDIUM'
            ELSE 'LOW'
        END AS complexity_rating
    FROM entity_metrics em
    JOIN ast_extract_source(
        ast_obj,
        node_ids := LIST(em.node_id),
        pad_lines := 3,
        highlight_range := true
    ) src ON em.node_id = src.node_id
    CROSS JOIN ast_file_stats(em.file_path) fs
    ORDER BY em.estimated_complexity DESC, em.line_count DESC
);

-- Example usage comments:
-- 
-- 1. Get source for a specific function:
-- SELECT * FROM read_ast_objects('app.py', 'python') AS ast,
--      TABLE(ast_extract_source(
--          ast_filter_type(ast, 'function_definition'),
--          pad_lines := 5
--      )) AS src
-- WHERE src.node_name = 'process_data';
--
-- 2. Find complex functions for review:
-- SELECT * FROM read_ast_objects('**/*.py', 'python') AS ast,
--      TABLE(ast_code_review_table(ast, complexity_threshold := 10))
-- WHERE complexity_rating = 'HIGH';
