-- AST Navigator: Comprehensive macro library for semantic code navigation
-- Load the AST extension
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- =============================================================================
-- CORE AST DISCOVERY FUNCTIONS
-- =============================================================================

-- Tree-based function extraction that correctly extracts C++ function names
CREATE OR REPLACE MACRO ast_find_functions(file_pattern) AS TABLE
    WITH 
    -- Step 1: Get all function declarators (these have the names we need)
    func_declarators AS (
        SELECT 
            file_path,
            node_id,
            parent_id,
            start_line,
            end_line,
            children_count,
            descendant_count,
            depth
        FROM read_ast(file_pattern)
        WHERE type = 'function_declarator'
          AND semantic_type = 112
    ),
    -- Step 2: Get immediate children of declarators to extract names
    declarator_info AS (
        SELECT 
            d.file_path,
            d.node_id,
            d.start_line,
            d.end_line,
            d.parent_id,
            d.descendant_count,
            -- Extract name from identifier or qualified_identifier child
            MAX(CASE 
                WHEN c.type = 'identifier' THEN c.name
                WHEN c.type = 'qualified_identifier' THEN c.name
            END) as full_name,
            -- Count parameters
            SUM(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count
        FROM func_declarators d
        JOIN read_ast(file_pattern) c ON c.parent_id = d.node_id AND c.file_path = d.file_path
        GROUP BY d.file_path, d.node_id, d.start_line, d.end_line, d.parent_id, d.descendant_count
    ),
    -- Step 3: Get parent info for definitions
    definition_info AS (
        SELECT 
            di.file_path,
            di.node_id,
            di.full_name,
            di.start_line,
            di.end_line,
            di.param_count,
            di.descendant_count,
            p.type as parent_type,
            p.start_line as def_start_line,
            p.end_line as def_end_line,
            p.descendant_count as def_complexity,
            p.peek as function_peek
        FROM declarator_info di
        LEFT JOIN read_ast(file_pattern) p ON p.node_id = di.parent_id AND p.file_path = di.file_path
    ),
    -- Step 4: Parse the function names
    parsed_functions AS (
        SELECT 
            file_path,
            full_name,
            -- Extract function name
            CASE 
                WHEN full_name LIKE '%::%' THEN 
                    REGEXP_EXTRACT(full_name, '::([^:]+)$', 1)
                ELSE full_name
            END as function_name,
            -- Extract class name
            CASE 
                WHEN full_name LIKE '%::%' THEN 
                    REGEXP_EXTRACT(full_name, '^(.+)::[^:]+$', 1)
                ELSE NULL
            END as class_name,
            -- Determine function type
            CASE 
                WHEN full_name LIKE '%::%' THEN 'method'
                ELSE 'function'
            END as function_type,
            -- Use definition boundaries if available, else declarator
            COALESCE(def_start_line, start_line) as start_line,
            COALESCE(def_end_line, end_line) as end_line,
            -- Use definition complexity if available
            COALESCE(def_complexity, descendant_count) as complexity,
            param_count,
            parent_type,
            function_peek
        FROM definition_info
    )
    SELECT 
        file_path,
        function_name,
        class_name,
        function_type,
        parent_type as type,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        complexity as descendant_count,
        param_count as children_count,
        function_peek as peek
    FROM parsed_functions
    WHERE function_name IS NOT NULL 
      AND function_name <> ''
      AND function_name NOT IN ('con', 'db_wrapper')  -- Filter constructor calls
    ORDER BY file_path, start_line;

-- Find functions with name filter - enhanced tree-based version
CREATE OR REPLACE MACRO ast_find_functions_named(file_pattern, name_pattern) AS TABLE
    WITH 
    -- Step 1: Get all function declarators (these have the names we need)
    func_declarators AS (
        SELECT 
            file_path,
            node_id,
            parent_id,
            start_line,
            end_line,
            children_count,
            descendant_count,
            depth
        FROM read_ast(file_pattern)
        WHERE type = 'function_declarator'
          AND semantic_type = 112
    ),
    -- Step 2: Get immediate children of declarators to extract names
    declarator_info AS (
        SELECT 
            d.file_path,
            d.node_id,
            d.start_line,
            d.end_line,
            d.parent_id,
            d.descendant_count,
            -- Extract name from identifier or qualified_identifier child
            MAX(CASE 
                WHEN c.type = 'identifier' THEN c.name
                WHEN c.type = 'qualified_identifier' THEN c.name
            END) as full_name,
            -- Count parameters
            SUM(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count
        FROM func_declarators d
        JOIN read_ast(file_pattern) c ON c.parent_id = d.node_id AND c.file_path = d.file_path
        GROUP BY d.file_path, d.node_id, d.start_line, d.end_line, d.parent_id, d.descendant_count
    ),
    -- Step 3: Get parent info for definitions
    definition_info AS (
        SELECT 
            di.file_path,
            di.node_id,
            di.full_name,
            di.start_line,
            di.end_line,
            di.param_count,
            di.descendant_count,
            p.type as parent_type,
            p.start_line as def_start_line,
            p.end_line as def_end_line,
            p.descendant_count as def_complexity
        FROM declarator_info di
        LEFT JOIN read_ast(file_pattern) p ON p.node_id = di.parent_id AND p.file_path = di.file_path
    ),
    -- Step 4: Parse the function names
    parsed_functions AS (
        SELECT 
            file_path,
            full_name,
            -- Extract function name
            CASE 
                WHEN full_name LIKE '%::%' THEN 
                    REGEXP_EXTRACT(full_name, '::([^:]+)$', 1)
                ELSE full_name
            END as function_name,
            -- Extract class name
            CASE 
                WHEN full_name LIKE '%::%' THEN 
                    REGEXP_EXTRACT(full_name, '^(.+)::[^:]+$', 1)
                ELSE NULL
            END as class_name,
            -- Use definition boundaries if available, else declarator
            COALESCE(def_start_line, start_line) as start_line,
            COALESCE(def_end_line, end_line) as end_line,
            -- Use definition complexity if available
            COALESCE(def_complexity, descendant_count) as complexity,
            param_count
        FROM definition_info
    )
    SELECT 
        file_path,
        function_name,
        class_name,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        complexity as descendant_count,
        param_count as children_count
    FROM parsed_functions
    WHERE function_name IS NOT NULL 
      AND function_name <> ''
      AND function_name ILIKE name_pattern
    ORDER BY file_path, start_line;

-- Find all classes/types in codebase
CREATE OR REPLACE MACRO ast_find_classes(file_pattern) AS TABLE
    SELECT 
        file_path,
        name as class_name,
        type as node_type,
        start_line,
        end_line,
        children_count,
        descendant_count
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 240) = 112  -- Is DEFINITION
      AND type IN ('class_definition', 'class_declaration', 'struct_declaration', 'interface_declaration')
      AND name IS NOT NULL AND name != ''
    ORDER BY file_path, start_line;

-- Find classes with name filter
CREATE OR REPLACE MACRO ast_find_classes_named(file_pattern, name_pattern) AS TABLE
    SELECT 
        file_path,
        name as class_name,
        type as node_type,
        start_line,
        end_line,
        children_count,
        descendant_count
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 240) = 112  -- Is DEFINITION
      AND type IN ('class_definition', 'class_declaration', 'struct_declaration', 'interface_declaration')
      AND name ILIKE name_pattern
    ORDER BY file_path, start_line;

-- Find all variable/constant definitions
CREATE OR REPLACE MACRO ast_find_variables(file_pattern) AS TABLE
    SELECT 
        file_path,
        name as variable_name,
        type as node_type,
        start_line,
        end_line,
        depth
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 240) = 112  -- Is DEFINITION
      AND type IN ('variable_declaration', 'assignment', 'const_declaration', 'let_declaration', 'var_declaration')
      AND name IS NOT NULL AND name != ''
    ORDER BY file_path, start_line;

-- Find variables with name filter
CREATE OR REPLACE MACRO ast_find_variables_named(file_pattern, name_pattern) AS TABLE
    SELECT 
        file_path,
        name as variable_name,
        type as node_type,
        start_line,
        end_line,
        depth
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 240) = 112  -- Is DEFINITION
      AND type IN ('variable_declaration', 'assignment', 'const_declaration', 'let_declaration', 'var_declaration')
      AND name ILIKE name_pattern
    ORDER BY file_path, start_line;

-- =============================================================================
-- FUNCTION BODY EXTRACTION
-- =============================================================================

-- Get complete function body with context (requires file pattern)
CREATE OR REPLACE MACRO ast_get_function_body(search_name, file_pattern) AS TABLE
    SELECT 
        file_path,
        function_name,
        type as function_type,
        start_line,
        end_line,
        line_count,
        descendant_count,
        peek as preview,
        '-- Use: sed -n "' || start_line || ',' || end_line || 'p" ' || file_path as extract_command
    FROM ast_find_functions(file_pattern)
    WHERE function_name = search_name
    ORDER BY file_path, start_line;

-- Alias for backward compatibility
CREATE OR REPLACE MACRO ast_get_function_body_in(function_name, file_pattern) AS TABLE
    SELECT * FROM ast_get_function_body(function_name, file_pattern);

-- Get function signature and parameters
CREATE OR REPLACE MACRO ast_get_function_signature(function_name) AS TABLE
    WITH function_nodes AS (
        SELECT 
            file_path,
            node_id,
            name,
            start_line,
            type,
            peek
        FROM read_ast('%')
        WHERE (semantic_type & 240) = 112  -- Is DEFINITION
          AND type IN ('function_definition', 'function_declarator', 'method_definition')
          AND name = function_name
    ),
    parameters AS (
        SELECT 
            f.file_path,
            f.name as function_name,
            p.name as parameter_name,
            p.type as parameter_type,
            p.sibling_index
        FROM function_nodes f
        JOIN read_ast('%') p ON f.file_path = p.file_path 
        WHERE p.parent_id = f.node_id
          AND p.type IN ('parameter', 'parameter_declaration', 'formal_parameter', 'identifier')
          AND p.name IS NOT NULL
          AND p.name != ''
    )
    SELECT 
        f.file_path,
        f.function_name,
        f.type as function_type,
        f.start_line,
        STRING_AGG(p.parameter_name, ', ' ORDER BY p.sibling_index) as parameters,
        f.peek as signature_preview
    FROM function_nodes f
    LEFT JOIN parameters p ON f.file_path = p.file_path AND f.function_name = p.function_name
    GROUP BY f.file_path, f.function_name, f.type, f.start_line, f.peek
    ORDER BY f.file_path, f.start_line;

-- =============================================================================
-- CALL GRAPH AND REFERENCES
-- =============================================================================

-- Find function calls (where functions are invoked)
CREATE OR REPLACE MACRO ast_find_function_calls(function_name) AS TABLE
    SELECT DISTINCT
        file_path,
        name as called_function,
        start_line,
        type as call_type,
        depth,
        peek as context
    FROM read_ast('%')
    WHERE type IN ('call_expression', 'function_call', 'method_invocation', 'call')
      AND (name = function_name OR peek ILIKE '%' || function_name || '%')
    ORDER BY file_path, start_line;

-- Find function calls in specific files
CREATE OR REPLACE MACRO ast_find_function_calls_in(function_name, file_pattern) AS TABLE
    SELECT DISTINCT
        file_path,
        name as called_function,
        start_line,
        type as call_type,
        depth,
        peek as context
    FROM read_ast(file_pattern)
    WHERE type IN ('call_expression', 'function_call', 'method_invocation', 'call')
      AND (name = function_name OR peek ILIKE '%' || function_name || '%')
    ORDER BY file_path, start_line;

-- Find all identifiers matching a name (variables, function refs, etc.)
CREATE OR REPLACE MACRO ast_find_references(symbol_name) AS TABLE
    SELECT 
        file_path,
        name as symbol,
        type as node_type,
        start_line,
        end_line,
        depth,
        semantic_type,
        CASE 
            WHEN (semantic_type & 240) = 112 THEN 'DEFINITION'
            WHEN type IN ('call_expression', 'function_call') THEN 'CALL'
            WHEN type IN ('identifier', 'variable_name') THEN 'REFERENCE'
            ELSE 'OTHER'
        END as usage_type,
        peek as context
    FROM read_ast('%')
    WHERE name = symbol_name
       OR (type = 'identifier' AND peek = symbol_name)
    ORDER BY file_path, start_line;

-- Find references in specific files
CREATE OR REPLACE MACRO ast_find_references_in(symbol_name, file_pattern) AS TABLE
    SELECT 
        file_path,
        name as symbol,
        type as node_type,
        start_line,
        end_line,
        depth,
        semantic_type,
        CASE 
            WHEN (semantic_type & 240) = 112 THEN 'DEFINITION'
            WHEN type IN ('call_expression', 'function_call') THEN 'CALL'
            WHEN type IN ('identifier', 'variable_name') THEN 'REFERENCE'
            ELSE 'OTHER'
        END as usage_type,
        peek as context
    FROM read_ast(file_pattern)
    WHERE name = symbol_name
       OR (type = 'identifier' AND peek = symbol_name)
    ORDER BY file_path, start_line;

-- =============================================================================
-- CODE STRUCTURE AND HIERARCHY
-- =============================================================================

-- Get high-level code structure (classes, functions, major constructs)
CREATE OR REPLACE MACRO ast_get_structure(file_pattern) AS TABLE
    SELECT 
        file_path,
        name,
        type as node_type,
        start_line,
        end_line,
        depth,
        children_count,
        descendant_count,
        CASE 
            WHEN (semantic_type & 240) = 112 THEN 'DEFINITION'
            WHEN (semantic_type & 240) = 128 THEN 'EXECUTION'
            WHEN (semantic_type & 240) = 144 THEN 'FLOW_CONTROL'
            ELSE 'OTHER'
        END as semantic_category
    FROM read_ast(file_pattern)
    WHERE depth <= 3
      AND (
          (semantic_type & 240) = 112  -- DEFINITIONS
          OR type IN ('class_definition', 'function_definition', 'if_statement', 'for_statement', 'while_statement')
      )
      AND name IS NOT NULL
      AND name != ''
    ORDER BY file_path, depth, start_line;

-- Get structure with custom depth
CREATE OR REPLACE MACRO ast_get_structure_depth(file_pattern, max_depth) AS TABLE
    SELECT 
        file_path,
        name,
        type as node_type,
        start_line,
        end_line,
        depth,
        children_count,
        descendant_count,
        CASE 
            WHEN (semantic_type & 240) = 112 THEN 'DEFINITION'
            WHEN (semantic_type & 240) = 128 THEN 'EXECUTION'
            WHEN (semantic_type & 240) = 144 THEN 'FLOW_CONTROL'
            ELSE 'OTHER'
        END as semantic_category
    FROM read_ast(file_pattern)
    WHERE depth <= max_depth
      AND (
          (semantic_type & 240) = 112  -- DEFINITIONS
          OR type IN ('class_definition', 'function_definition', 'if_statement', 'for_statement', 'while_statement')
      )
      AND name IS NOT NULL
      AND name != ''
    ORDER BY file_path, depth, start_line;

-- Get import/include dependencies
CREATE OR REPLACE MACRO ast_get_imports(file_pattern) AS TABLE
    SELECT 
        file_path,
        name as imported_name,
        type as import_type,
        start_line,
        peek as import_statement
    FROM read_ast(file_pattern)
    WHERE type IN ('import_statement', 'import_from_statement', 'include_statement', 'preproc_include', 'import_declaration')
    ORDER BY file_path, start_line;

-- =============================================================================
-- CODE METRICS AND ANALYSIS
-- =============================================================================

-- Get complexity metrics per file
CREATE OR REPLACE MACRO ast_get_complexity(file_pattern) AS TABLE
    SELECT 
        file_path,
        COUNT(*) as total_nodes,
        COUNT(*) FILTER (WHERE (semantic_type & 240) = 112) as definitions,
        COUNT(*) FILTER (WHERE type IN ('function_definition', 'method_definition')) as functions,
        COUNT(*) FILTER (WHERE type IN ('class_definition', 'struct_declaration')) as classes,
        COUNT(*) FILTER (WHERE type IN ('if_statement', 'for_statement', 'while_statement', 'switch_statement')) as control_structures,
        MAX(depth) as max_nesting_depth,
        AVG(depth) as avg_depth
    FROM read_ast(file_pattern)
    GROUP BY file_path
    ORDER BY total_nodes DESC;

-- Find large/complex functions
CREATE OR REPLACE MACRO ast_find_complex_functions(file_pattern) AS TABLE
    SELECT 
        file_path,
        name as function_name,
        start_line,
        end_line,
        (end_line - start_line + 1) as line_count,
        children_count,
        descendant_count,
        ROUND(descendant_count::FLOAT / (end_line - start_line + 1), 2) as nodes_per_line
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 240) = 112  -- Is DEFINITION
      AND type IN ('function_definition', 'method_definition')
      AND (end_line - start_line + 1) >= 50
      AND children_count >= 20
    ORDER BY descendant_count DESC;

-- Find complex functions with custom thresholds
CREATE OR REPLACE MACRO ast_find_complex_functions_custom(file_pattern, min_lines, min_children) AS TABLE
    SELECT 
        file_path,
        name as function_name,
        start_line,
        end_line,
        (end_line - start_line + 1) as line_count,
        children_count,
        descendant_count,
        ROUND(descendant_count::FLOAT / (end_line - start_line + 1), 2) as nodes_per_line
    FROM read_ast(file_pattern)
    WHERE (semantic_type & 240) = 112  -- Is DEFINITION
      AND type IN ('function_definition', 'method_definition')
      AND (end_line - start_line + 1) >= min_lines
      AND children_count >= min_children
    ORDER BY descendant_count DESC;

-- =============================================================================
-- SEARCH AND PATTERN MATCHING
-- =============================================================================

-- Find error handling patterns
CREATE OR REPLACE MACRO ast_find_error_handling(file_pattern) AS TABLE
    SELECT 
        file_path,
        type as error_pattern,
        name,
        start_line,
        end_line,
        peek as context
    FROM read_ast(file_pattern)
    WHERE type IN ('try_statement', 'catch_clause', 'except_clause', 'finally_clause', 'throw_statement', 'raise_statement')
       OR (type = 'call_expression' AND name IN ('assert', 'panic', 'abort', 'exit'))
       OR peek ILIKE '%error%' OR peek ILIKE '%exception%'
    ORDER BY file_path, start_line;

-- =============================================================================
-- UTILITIES
-- =============================================================================

-- Quick file overview - top-level constructs only
CREATE OR REPLACE MACRO ast_file_summary(file_path) AS TABLE
    SELECT 
        type as construct_type,
        name,
        start_line,
        (end_line - start_line + 1) as lines,
        CASE 
            WHEN (semantic_type & 240) = 112 THEN 'DEFINITION'
            WHEN type IN ('import_statement', 'import_from_statement', 'include_statement') THEN 'IMPORT'
            ELSE 'OTHER'
        END as category
    FROM read_ast(file_path)
    WHERE depth <= 2
      AND (
          (semantic_type & 240) = 112  -- DEFINITIONS
          OR type IN ('import_statement', 'import_from_statement', 'include_statement', 'preproc_include')
      )
      AND name IS NOT NULL
      AND name != ''
    ORDER BY start_line;

-- List all available languages and their file extensions
CREATE OR REPLACE MACRO ast_supported_languages() AS TABLE
    SELECT * FROM (VALUES 
        ('python', '.py'),
        ('javascript', '.js,.mjs,.cjs,.ts,.tsx'),
        ('cpp', '.cpp,.cxx,.cc,.c,.hpp,.hxx,.h')
    ) as t(language, extensions);

-- =============================================================================
-- CONVENIENCE FUNCTIONS FOR COMMON SEARCHES
-- =============================================================================

-- Find all main functions
CREATE OR REPLACE MACRO ast_find_main_functions(file_pattern) AS TABLE
    SELECT * FROM ast_find_functions_named(file_pattern, 'main');

-- Find all constructors and destructors
CREATE OR REPLACE MACRO ast_find_constructors(file_pattern) AS TABLE
    SELECT 
        file_path,
        name as function_name,
        type as node_type,
        start_line,
        end_line
    FROM read_ast(file_pattern)
    WHERE type IN ('constructor_definition', 'destructor_definition', '__init__', '__del__')
       OR (type = 'function_definition' AND (name = '__init__' OR name = '__del__'))
    ORDER BY file_path, start_line;