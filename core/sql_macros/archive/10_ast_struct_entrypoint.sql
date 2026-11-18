-- AST Struct Entrypoint
-- Single macro that provides the cleanest possible API

-- ===================================
-- PRIMARY ENTRYPOINT
-- ===================================

-- The main entry point - creates a clean AST table for analysis
CREATE OR REPLACE MACRO ast(file_path, language := NULL) AS (
    WITH loaded_ast AS (
        SELECT ast_load(file_path, language) as ast_data
    )
    SELECT 
        ast_data,
        ast_data.file_path as file,
        ast_data.language as lang,
        length(ast_data.nodes) as node_count,
        (SELECT max(node.depth) FROM ast_nodes(ast_data)) as max_depth
    FROM loaded_ast
);

-- ===================================
-- FLUENT QUERY INTERFACE  
-- ===================================

-- Chain-able query interface for exploring ASTs
-- Usage: SELECT * FROM ast_query('file.js') WHERE ...

CREATE OR REPLACE MACRO ast_query(file_path, language := NULL) AS (
    SELECT 
        node.node_id as id,
        node.type,
        node.name,
        node.depth,
        node.start_line as line,
        node.end_line,
        node.children_count as children,
        node.descendant_count as descendants,
        node.parent_id as parent,
        node.sibling_index as sibling,
        length(node.source_text) as source_length,
        node.source_text as source
    FROM ast_nodes(ast_load(file_path, language))
);

-- ===================================
-- QUICK ANALYSIS SHORTCUTS
-- ===================================

-- Get all functions in a file with one call
CREATE OR REPLACE MACRO ast_functions(file_path, language := NULL) AS (
    SELECT 
        node.name as function_name,
        node.start_line as line,
        node.descendant_count as complexity,
        node.source_text as signature
    FROM ast_nodes(ast_load(file_path, language))
    WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')
      AND node.name IS NOT NULL
    ORDER BY node.start_line
);

-- Get all classes in a file with one call  
CREATE OR REPLACE MACRO ast_classes(file_path, language := NULL) AS (
    SELECT 
        node.name as class_name,
        node.start_line as line,
        node.descendant_count as complexity,
        (SELECT count(*) 
         FROM ast_children(ast_load(file_path, language), node.node_id) child
         WHERE child.node.type IN ('method_definition', 'function_definition')) as method_count
    FROM ast_nodes(ast_load(file_path, language))
    WHERE node.type IN ('class_declaration', 'class_definition')
      AND node.name IS NOT NULL
    ORDER BY node.start_line
);

-- Get file overview in one call
CREATE OR REPLACE MACRO ast_overview(file_path, language := NULL) AS (
    WITH ast_data AS (SELECT ast_load(file_path, language) as ast)
    SELECT 
        ast.file_path as file,
        ast.language,
        length(ast.nodes) as total_nodes,
        (SELECT max(node.depth) FROM ast_nodes(ast)) as max_depth,
        (SELECT count(*) FROM ast_nodes(ast) WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')) as functions,
        (SELECT count(*) FROM ast_nodes(ast) WHERE node.type IN ('class_declaration', 'class_definition')) as classes,
        (SELECT count(*) FROM ast_nodes(ast) WHERE node.children_count = 0) as leaf_nodes
    FROM ast_data
);

-- ===================================
-- DEVELOPER SHORTCUTS
-- ===================================

-- Find all identifiers (variable names, etc.)
CREATE OR REPLACE MACRO ast_identifiers(file_path, language := NULL) AS (
    SELECT DISTINCT 
        node.name as identifier,
        count(*) as usage_count,
        min(node.start_line) as first_line,
        max(node.start_line) as last_line
    FROM ast_nodes(ast_load(file_path, language))
    WHERE node.type = 'identifier' 
      AND node.name IS NOT NULL 
      AND node.name != ''
    GROUP BY node.name
    ORDER BY usage_count DESC
);

-- Get imports/dependencies
CREATE OR REPLACE MACRO ast_imports(file_path, language := NULL) AS (
    SELECT 
        node.source_text as import_statement,
        node.start_line as line
    FROM ast_nodes(ast_load(file_path, language))
    WHERE node.type IN ('import_statement', 'import_from_statement', 'preproc_include')
    ORDER BY node.start_line
);

-- ===================================
-- BATCH OPERATIONS  
-- ===================================

-- Analyze multiple files at once
CREATE OR REPLACE MACRO ast_batch_overview(file_pattern) AS (
    -- This would need to be implemented with a table function
    -- that can glob files, but shows the API design
    SELECT 
        'Use with read_ast and glob patterns' as note,
        'ast_batch_overview implementation pending' as status
);

-- ===================================
-- EXAMPLES AND TEMPLATES
-- ===================================

-- Example: Find all public methods in a class
CREATE OR REPLACE MACRO ast_example_public_methods(file_path, class_name, language := NULL) AS (
    WITH class_node AS (
        SELECT node.node_id
        FROM ast_nodes(ast_load(file_path, language))
        WHERE node.type IN ('class_declaration', 'class_definition')
          AND node.name = class_name
    )
    SELECT 
        method.node.name as method_name,
        method.node.start_line as line,
        method.node.source_text as signature
    FROM ast_subtree(ast_load(file_path, language), (SELECT node_id FROM class_node)) method
    WHERE method.node.type IN ('method_definition', 'function_definition')
      AND method.node.name IS NOT NULL
      AND (method.node.source_text LIKE '%public%' OR method.node.source_text NOT LIKE '%private%')
);

-- Example: Complexity analysis  
CREATE OR REPLACE MACRO ast_example_complexity(file_path, language := NULL) AS (
    SELECT 
        'function' as element_type,
        node.name as element_name,
        node.descendant_count as complexity_score,
        CASE 
            WHEN node.descendant_count > 100 THEN 'very_high'
            WHEN node.descendant_count > 50 THEN 'high'
            WHEN node.descendant_count > 20 THEN 'medium'
            ELSE 'low'
        END as complexity_level
    FROM ast_nodes(ast_load(file_path, language))
    WHERE node.type IN ('function_declaration', 'function_definition', 'method_definition')
      AND node.name IS NOT NULL
    ORDER BY node.descendant_count DESC
);