-- AST Navigator Parquet Index System
-- Provides efficient parquet-based indexing for large-scale AST analysis

-- =============================================================================
-- INDEX GENERATION MACROS
-- =============================================================================

-- Create a parquet index for a specific file type
CREATE OR REPLACE MACRO ast_create_index(file_pattern, file_type := NULL, index_path := NULL) AS (
    WITH config AS (
        SELECT 
            COALESCE(file_type, REGEXP_EXTRACT(file_pattern, '\.([^.]+)$', 1)) as ftype,
            COALESCE(index_path, '.index-' || COALESCE(file_type, REGEXP_EXTRACT(file_pattern, '\.([^.]+)$', 1)) || '.parquet') as idx_path
    )
    SELECT 
        'Creating index: ' || idx_path || ' for pattern: ' || file_pattern as status,
        (SELECT COUNT(*) FROM read_ast(file_pattern, peek_mode := 'none')) as total_nodes
    FROM config
);

-- Actually create the index (run this as a statement, not a query)
CREATE OR REPLACE MACRO ast_create_index_exec(file_pattern, file_type) AS (
    'COPY (SELECT * FROM read_ast(''' || file_pattern || ''', peek_mode := ''none'')) TO ''.index-' || 
    COALESCE(file_type, REGEXP_EXTRACT(file_pattern, '\.([^.]+)$', 1)) || 
    '.parquet'' (FORMAT PARQUET, CODEC ''ZSTD'', COMPRESSION_LEVEL 22)'
);

-- =============================================================================
-- INDEX UPDATE MACROS (Selective Regeneration)
-- =============================================================================

-- Get list of files in the current index
CREATE OR REPLACE MACRO ast_index_files(index_path) AS TABLE
    SELECT DISTINCT file_path, COUNT(*) as node_count
    FROM read_parquet(index_path)
    GROUP BY file_path
    ORDER BY file_path;

-- Update index with new or modified files
CREATE OR REPLACE MACRO ast_update_index(file_pattern, file_type := NULL, modified_since := NULL) AS TABLE
    WITH 
    index_config AS (
        SELECT 
            COALESCE(file_type, REGEXP_EXTRACT(file_pattern, '\.([^.]+)$', 1)) as ftype,
            '.index-' || COALESCE(file_type, REGEXP_EXTRACT(file_pattern, '\.([^.]+)$', 1)) || '.parquet' as index_path
    ),
    existing_files AS (
        SELECT DISTINCT file_path 
        FROM read_parquet((SELECT index_path FROM index_config))
    ),
    -- Get new files matching the pattern
    new_files AS (
        SELECT DISTINCT file_path
        FROM read_ast(file_pattern, peek_mode := 'none')
        WHERE file_path NOT IN (SELECT file_path FROM existing_files)
    )
    SELECT 
        'New files to index' as status,
        COUNT(*) as file_count,
        LIST(file_path ORDER BY file_path) as files
    FROM new_files
    UNION ALL
    SELECT 
        'Existing indexed files' as status,
        COUNT(*) as file_count,
        NULL as files
    FROM existing_files;

-- Generate SQL to merge existing index with new files
CREATE OR REPLACE MACRO ast_generate_update_sql(file_pattern, file_type := NULL) AS (
    WITH config AS (
        SELECT 
            COALESCE(file_type, REGEXP_EXTRACT(file_pattern, '\.([^.]+)$', 1)) as ftype,
            '.index-' || COALESCE(file_type, REGEXP_EXTRACT(file_pattern, '\.([^.]+)$', 1)) || '.parquet' as index_path,
            '.index-' || COALESCE(file_type, REGEXP_EXTRACT(file_pattern, '\.([^.]+)$', 1)) || '.parquet.tmp' as temp_path
    )
    SELECT 
        'COPY (' || CHR(10) ||
        '    SELECT * FROM read_parquet(''' || index_path || ''')' || CHR(10) ||
        '    WHERE file_path NOT IN (' || CHR(10) ||
        '        SELECT DISTINCT file_path FROM read_ast(''' || file_pattern || ''', peek_mode := ''none'')' || CHR(10) ||
        '    )' || CHR(10) ||
        '    UNION ALL' || CHR(10) ||
        '    SELECT * FROM read_ast(''' || file_pattern || ''', peek_mode := ''none'')' || CHR(10) ||
        ') TO ''' || temp_path || ''' (FORMAT PARQUET, CODEC ''ZSTD'', COMPRESSION_LEVEL 22);' || CHR(10) ||
        '-- Then rename: mv ' || temp_path || ' ' || index_path as update_sql
    FROM config
);

-- =============================================================================
-- QUERY FUNCTIONS USING PARQUET INDEX
-- =============================================================================

-- 1) Get a summary of all functions in a file
CREATE OR REPLACE MACRO ast_file_functions(file_path, index_path) AS TABLE
    WITH 
    index_config AS (
        SELECT COALESCE(index_path, 
            '.index-' || REGEXP_EXTRACT(file_path, '\.([^.]+)$', 1) || '.parquet'
        ) as idx_path
    ),
    -- Step 1: Get all function declarators from the index
    func_declarators AS (
        SELECT 
            node_id,
            parent_id,
            start_line,
            end_line,
            children_count,
            descendant_count
        FROM read_parquet((SELECT idx_path FROM index_config))
        WHERE file_path = file_path
          AND type = 'function_declarator'
          AND semantic_type = 112
    ),
    -- Step 2: Get names from child nodes
    func_names AS (
        SELECT 
            d.node_id,
            d.start_line,
            d.end_line,
            d.parent_id,
            d.descendant_count,
            MAX(CASE 
                WHEN n.type IN ('identifier', 'qualified_identifier') THEN n.name
            END) as function_name
        FROM func_declarators d
        JOIN read_parquet((SELECT idx_path FROM index_config)) n 
            ON n.parent_id = d.node_id 
            AND n.file_path = file_path
        GROUP BY d.node_id, d.start_line, d.end_line, d.parent_id, d.descendant_count
    ),
    -- Step 3: Get parent context for full definition
    func_defs AS (
        SELECT 
            f.function_name,
            COALESCE(p.start_line, f.start_line) as start_line,
            COALESCE(p.end_line, f.end_line) as end_line,
            COALESCE(p.descendant_count, f.descendant_count) as complexity,
            p.type as definition_type
        FROM func_names f
        LEFT JOIN read_parquet((SELECT idx_path FROM index_config)) p 
            ON p.node_id = f.parent_id 
            AND p.file_path = file_path
    )
    SELECT 
        function_name,
        definition_type as type,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        complexity
    FROM func_defs
    WHERE function_name IS NOT NULL
    ORDER BY start_line;

-- 2) Find a specific function and get detailed analysis
CREATE OR REPLACE MACRO ast_find_function_detail(function_name, index_path := NULL, file_pattern := '%') AS TABLE
    WITH 
    -- Find all matching functions across indexed files
    matching_functions AS (
        SELECT DISTINCT
            i.file_path,
            i.node_id as declarator_id,
            i.parent_id,
            i.start_line,
            i.end_line,
            i.descendant_count
        FROM read_parquet(COALESCE(index_path, '.index-*.parquet')) i
        WHERE i.type = 'function_declarator'
          AND i.semantic_type = 112
          AND i.file_path LIKE file_pattern
          AND EXISTS (
              SELECT 1 
              FROM read_parquet(COALESCE(index_path, '.index-*.parquet')) n
              WHERE n.parent_id = i.node_id 
                AND n.file_path = i.file_path
                AND n.type IN ('identifier', 'qualified_identifier')
                AND n.name = function_name
          )
    ),
    -- Get full function details with parent context
    function_details AS (
        SELECT 
            f.file_path,
            function_name,
            p.type as definition_type,
            COALESCE(p.start_line, f.start_line) as start_line,
            COALESCE(p.end_line, f.end_line) as end_line,
            COALESCE(p.descendant_count, f.descendant_count) as complexity,
            -- Count parameters
            (SELECT COUNT(*) 
             FROM read_parquet(COALESCE(index_path, '.index-*.parquet')) params
             WHERE params.parent_id = f.declarator_id 
               AND params.file_path = f.file_path
               AND params.type = 'parameter_declaration') as param_count,
            -- Count local variables
            (SELECT COUNT(*) 
             FROM read_parquet(COALESCE(index_path, '.index-*.parquet')) vars
             WHERE vars.node_id > f.declarator_id 
               AND vars.node_id < f.declarator_id + COALESCE(p.descendant_count, f.descendant_count)
               AND vars.file_path = f.file_path
               AND vars.type IN ('variable_declaration', 'const_declaration', 'let_declaration')) as local_vars,
            -- Count function calls
            (SELECT COUNT(*) 
             FROM read_parquet(COALESCE(index_path, '.index-*.parquet')) calls
             WHERE calls.node_id > f.declarator_id 
               AND calls.node_id < f.declarator_id + COALESCE(p.descendant_count, f.descendant_count)
               AND calls.file_path = f.file_path
               AND calls.type = 'call_expression') as calls_made
        FROM matching_functions f
        LEFT JOIN read_parquet(COALESCE(index_path, '.index-*.parquet')) p 
            ON p.node_id = f.parent_id 
            AND p.file_path = f.file_path
    )
    SELECT 
        file_path,
        function_name,
        definition_type,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        complexity,
        param_count,
        local_vars,
        calls_made,
        ROUND(complexity::DOUBLE / GREATEST(line_count, 1), 2) as complexity_per_line
    FROM function_details
    ORDER BY file_path, start_line;

-- 3) Get the actual source code definition of a file
CREATE OR REPLACE MACRO ast_get_file_source(file_path, start_line := 1, end_line := NULL) AS (
    SELECT 
        file_path,
        start_line,
        COALESCE(end_line, (SELECT MAX(end_line) FROM read_parquet('.index-*.parquet') WHERE file_path = file_path)) as end_line,
        'sed -n "' || start_line || ',' || COALESCE(end_line, '$') || 'p" ' || file_path as extract_command
);

-- Get source for a specific function
CREATE OR REPLACE MACRO ast_get_function_source(function_name, file_path := NULL, index_path := NULL) AS TABLE
    WITH func_location AS (
        SELECT 
            file_path,
            function_name,
            start_line,
            end_line
        FROM ast_find_function_detail(function_name, index_path, COALESCE(file_path, '%'))
        LIMIT 1
    )
    SELECT 
        file_path,
        function_name,
        start_line,
        end_line,
        'sed -n "' || start_line || ',' || end_line || 'p" ' || file_path as extract_command,
        '# Extract with: ' || extract_command as usage_hint
    FROM func_location;

-- =============================================================================
-- INDEX STATISTICS AND MANAGEMENT
-- =============================================================================

-- Get index statistics
CREATE OR REPLACE MACRO ast_index_stats(index_path) AS TABLE
    SELECT 
        index_path as index_file,
        COUNT(DISTINCT file_path) as indexed_files,
        COUNT(*) as total_nodes,
        COUNT(*) FILTER (WHERE type = 'function_declarator' AND semantic_type = 112) as total_functions,
        COUNT(*) FILTER (WHERE type IN ('class_definition', 'class_declaration', 'struct_declaration')) as total_classes,
        ROUND(COUNT(*)::DOUBLE / COUNT(DISTINCT file_path), 2) as avg_nodes_per_file
    FROM read_parquet(index_path)
    GROUP BY index_path;

-- Find most complex functions in index
CREATE OR REPLACE MACRO ast_index_complex_functions(index_path := '.index-*.parquet', min_complexity := 100) AS TABLE
    WITH func_declarators AS (
        SELECT 
            file_path,
            node_id,
            parent_id,
            start_line,
            end_line,
            descendant_count
        FROM read_parquet(index_path)
        WHERE type = 'function_declarator'
          AND semantic_type = 112
          AND descendant_count >= min_complexity
    ),
    func_names AS (
        SELECT 
            d.file_path,
            d.start_line,
            d.end_line,
            d.descendant_count,
            MAX(CASE WHEN n.type IN ('identifier', 'qualified_identifier') THEN n.name END) as function_name
        FROM func_declarators d
        JOIN read_parquet(index_path) n ON n.parent_id = d.node_id AND n.file_path = d.file_path
        GROUP BY d.file_path, d.start_line, d.end_line, d.descendant_count
    )
    SELECT 
        file_path,
        function_name,
        start_line,
        end_line,
        descendant_count as complexity
    FROM func_names
    WHERE function_name IS NOT NULL
    ORDER BY complexity DESC;

-- =============================================================================
-- HELPER MACROS
-- =============================================================================

-- List all available indexes
CREATE OR REPLACE MACRO ast_list_indexes() AS TABLE
    SELECT 
        file as index_path,
        REGEXP_EXTRACT(file, '\.index-(.+)\.parquet$', 1) as file_type
    FROM (
        SELECT * FROM glob('.index-*.parquet')
    )
    ORDER BY file_type;

-- Quick function search across all indexes
CREATE OR REPLACE MACRO ast_quick_find(search_term, search_type) AS TABLE
    WITH all_indexes AS (
        SELECT file as index_path FROM glob('.index-*.parquet')
    ),
    search_results AS (
        SELECT 
            i.file_path,
            i.name,
            i.type,
            i.start_line,
            i.end_line,
            idx.index_path
        FROM all_indexes idx
        CROSS JOIN LATERAL (
            SELECT * FROM read_parquet(idx.index_path)
            WHERE name ILIKE '%' || search_term || '%'
              AND CASE 
                  WHEN search_type = 'function' THEN type = 'function_declarator' AND semantic_type = 112
                  WHEN search_type = 'class' THEN type IN ('class_definition', 'class_declaration', 'struct_declaration')
                  WHEN search_type = 'variable' THEN type IN ('variable_declaration', 'const_declaration', 'let_declaration')
                  ELSE true
              END
        ) i
    )
    SELECT 
        file_path,
        name,
        type,
        start_line,
        end_line,
        REGEXP_EXTRACT(index_path, '\.index-(.+)\.parquet$', 1) as language
    FROM search_results
    ORDER BY file_path, start_line;