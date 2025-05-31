-- AST To Functions
-- Transform AST data to other formats (breaks out of AST monad)

-- ===================================
-- BASIC TRANSFORMATIONS
-- ===================================

-- Extract all names as array
CREATE OR REPLACE MACRO ast_to_names(ast, type := NULL) AS (
    [
        node.name
        for node in ast.nodes
        if node.name IS NOT NULL 
           AND node.name != ''
           AND (type IS NULL OR node.type = type)
    ]
);

-- Extract all types as array
CREATE OR REPLACE MACRO ast_to_types(ast) AS (
    list_distinct([node.type for node in ast.nodes])
);

-- Extract source text snippets
CREATE OR REPLACE MACRO ast_to_source(ast, type := NULL) AS (
    [
        node.source_text
        for node in ast.nodes
        if node.source_text IS NOT NULL
           AND node.source_text != ''
           AND (type IS NULL OR node.type = type)
    ]
);

-- Convert to location table
CREATE OR REPLACE MACRO ast_to_locations(ast) AS (
    [
        struct_pack(
            node_id := node.node_id,
            type := node.type,
            name := node.name,
            file := ast.source.file_path,
            start_line := node.start_line,
            end_line := node.end_line,
            start_column := node.start_column,
            end_column := node.end_column
        )
        for node in ast.nodes
    ]
);

-- ===================================
-- STATISTICAL TRANSFORMATIONS
-- ===================================

-- Type frequency table
CREATE OR REPLACE MACRO ast_to_type_stats(ast) AS (
    WITH type_counts AS (
        SELECT 
            node.type,
            count(*) as count
        FROM (SELECT unnest(ast.nodes) as node)
        GROUP BY node.type
    )
    SELECT 
        type,
        count,
        round(count * 100.0 / sum(count) OVER (), 2) as percentage
    FROM type_counts
    ORDER BY count DESC
);

-- Depth distribution
CREATE OR REPLACE MACRO ast_to_depth_stats(ast) AS (
    WITH depth_counts AS (
        SELECT 
            node.depth,
            count(*) as count
        FROM (SELECT unnest(ast.nodes) as node)
        GROUP BY node.depth
    )
    SELECT 
        depth,
        count,
        round(count * 100.0 / sum(count) OVER (), 2) as percentage
    FROM depth_counts
    ORDER BY depth
);

-- Complexity metrics
CREATE OR REPLACE MACRO ast_to_complexity_metrics(ast) AS (
    [
        struct_pack(
            name := node.name,
            type := node.type,
            line := node.start_line,
            descendants := node.descendant_count,
            children := node.children_count,
            lines := node.end_line - node.start_line + 1,
            complexity_score := node.descendant_count * 1.0 / GREATEST(node.end_line - node.start_line + 1, 1)
        )
        for node in ast.nodes
        if node.name IS NOT NULL AND node.children_count > 0
    ]
);

-- ===================================
-- CODE ANALYSIS TRANSFORMATIONS
-- ===================================

-- Function signatures
CREATE OR REPLACE MACRO ast_to_signatures(ast) AS (
    [
        struct_pack(
            name := node.name,
            type := node.type,
            line := node.start_line,
            signature := trim(split_part(node.source_text, chr(10), 1))
        )
        for node in ast.nodes
        if node.type IN ('function_declaration', 'function_definition', 'method_definition')
           AND node.name IS NOT NULL
    ]
);

-- Import/dependency list
CREATE OR REPLACE MACRO ast_to_dependencies(ast) AS (
    [
        struct_pack(
            line := node.start_line,
            statement := trim(node.source_text),
            type := node.type
        )
        for node in ast.nodes
        if node.type IN ('import_statement', 'import_from_statement', 'preproc_include')
    ]
);

-- Call graph edges (caller -> callee relationships)
CREATE OR REPLACE MACRO ast_to_call_edges(ast) AS (
    WITH function_scopes AS (
        SELECT 
            node.node_id,
            node.name as function_name,
            node.descendant_count,
            row_number() OVER (ORDER BY node.node_id) - 1 as pos
        FROM (SELECT unnest(ast.nodes) as node)
        WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')
          AND node.name IS NOT NULL
    ),
    indexed_nodes AS (
        SELECT 
            node.*,
            row_number() OVER (ORDER BY node.node_id) - 1 as pos
        FROM (SELECT unnest(ast.nodes) as node)
    )
    SELECT DISTINCT
        f.function_name as caller,
        c.name as callee
    FROM function_scopes f
    JOIN indexed_nodes c 
        ON c.pos BETWEEN f.pos AND f.pos + f.descendant_count
        AND c.type IN ('call_expression', 'function_call')
        AND c.name IS NOT NULL
);

-- ===================================
-- SUMMARY TRANSFORMATIONS
-- ===================================

-- Overall AST summary
CREATE OR REPLACE MACRO ast_to_summary(ast) AS (
    SELECT 
        ast.file_path,
        ast.language,
        length(ast.nodes) as total_nodes,
        (SELECT max(node.depth) FROM unnest(ast.nodes) as node) as max_depth,
        (SELECT count(*) FROM unnest(ast.nodes) as node WHERE node.children_count = 0) as leaf_count,
        (SELECT count(DISTINCT node.type) FROM unnest(ast.nodes) as node) as unique_types,
        (SELECT count(*) FROM unnest(ast.nodes) as node WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')) as function_count,
        (SELECT count(*) FROM unnest(ast.nodes) as node WHERE node.type IN ('class_declaration', 'class_definition')) as class_count
);