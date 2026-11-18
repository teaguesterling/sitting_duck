-- AST Struct Core API
-- Clean, modern macros designed for the new AST struct format
-- No backward compatibility with JSON-based POC code

-- ===================================
-- CORE AST STRUCT FUNCTIONS
-- ===================================

-- Load an AST from a file
CREATE OR REPLACE MACRO ast_load(file_path, language := NULL) AS (
    CASE 
        WHEN language IS NULL THEN (SELECT ast FROM read_ast(file_path))
        ELSE (SELECT ast FROM read_ast(file_path, language))
    END
);

-- Extract all nodes from an AST struct as a table
CREATE OR REPLACE MACRO ast_nodes(ast_struct) AS (
    SELECT unnest(ast_struct.nodes) as node
);

-- Get AST metadata
CREATE OR REPLACE MACRO ast_info(ast_struct) AS (
    SELECT 
        ast_struct.file_path,
        ast_struct.language,
        length(ast_struct.nodes) as node_count
);

-- ===================================
-- NODE FILTERING
-- ===================================

-- Find nodes by type
CREATE OR REPLACE MACRO ast_type(ast_struct, node_type) AS (
    SELECT node 
    FROM ast_nodes(ast_struct) 
    WHERE node.type = node_type
);

-- Find nodes by multiple types
CREATE OR REPLACE MACRO ast_types(ast_struct, type_list) AS (
    SELECT node 
    FROM ast_nodes(ast_struct) 
    WHERE list_contains(type_list, node.type)
);

-- Find nodes by depth
CREATE OR REPLACE MACRO ast_depth(ast_struct, target_depth) AS (
    SELECT node 
    FROM ast_nodes(ast_struct) 
    WHERE node.depth = target_depth
);

-- Find nodes with names (non-empty)
CREATE OR REPLACE MACRO ast_named(ast_struct) AS (
    SELECT node 
    FROM ast_nodes(ast_struct) 
    WHERE node.name IS NOT NULL AND node.name != ''
);

-- Find nodes by name pattern
CREATE OR REPLACE MACRO ast_pattern(ast_struct, pattern) AS (
    SELECT node 
    FROM ast_nodes(ast_struct) 
    WHERE node.name LIKE pattern
);

-- ===================================
-- NAVIGATION & RELATIONSHIPS
-- ===================================

-- Get children of a specific node
CREATE OR REPLACE MACRO ast_children(ast_struct, parent_node_id) AS (
    SELECT node 
    FROM ast_nodes(ast_struct) 
    WHERE node.parent_id = parent_node_id
);

-- Get parent of a specific node
CREATE OR REPLACE MACRO ast_parent(ast_struct, child_node_id) AS (
    SELECT node 
    FROM ast_nodes(ast_struct) 
    WHERE node.node_id = (
        SELECT parent_id 
        FROM ast_nodes(ast_struct) 
        WHERE node_id = child_node_id
    )
);

-- Get all descendants of a node (O(1) using descendant_count)
CREATE OR REPLACE MACRO ast_subtree(ast_struct, root_node_id) AS (
    WITH root_info AS (
        SELECT 
            node.descendant_count,
            row_number() OVER (ORDER BY node.node_id) - 1 as start_pos
        FROM ast_nodes(ast_struct)
        WHERE node.node_id = root_node_id
    ),
    indexed_nodes AS (
        SELECT 
            node.*,
            row_number() OVER (ORDER BY node.node_id) - 1 as position
        FROM ast_nodes(ast_struct)
    )
    SELECT n.node
    FROM indexed_nodes n, root_info r
    WHERE n.position BETWEEN r.start_pos AND r.start_pos + r.descendant_count
);

-- Get siblings of a node
CREATE OR REPLACE MACRO ast_siblings(ast_struct, target_node_id) AS (
    WITH target_parent AS (
        SELECT parent_id 
        FROM ast_nodes(ast_struct) 
        WHERE node_id = target_node_id
    )
    SELECT node 
    FROM ast_nodes(ast_struct), target_parent
    WHERE node.parent_id = target_parent.parent_id
      AND node.node_id != target_node_id
);

-- ===================================
-- EXTRACTION & TRANSFORMATION
-- ===================================

-- Extract all names of a specific type
CREATE OR REPLACE MACRO ast_names(ast_struct, node_type := NULL) AS (
    SELECT node.name
    FROM ast_nodes(ast_struct)
    WHERE node.name IS NOT NULL 
      AND node.name != ''
      AND (node_type IS NULL OR node.type = node_type)
);

-- Extract source text for nodes of a specific type
CREATE OR REPLACE MACRO ast_source(ast_struct, node_type) AS (
    SELECT node.source_text
    FROM ast_nodes(ast_struct)
    WHERE node.type = node_type
      AND node.source_text IS NOT NULL
      AND node.source_text != ''
);

-- Get line ranges for nodes
CREATE OR REPLACE MACRO ast_ranges(ast_struct, node_type := NULL) AS (
    SELECT 
        node.node_id,
        node.type,
        node.name,
        node.start_line,
        node.end_line,
        node.end_line - node.start_line + 1 as line_count
    FROM ast_nodes(ast_struct)
    WHERE node_type IS NULL OR node.type = node_type
);

-- ===================================
-- ANALYSIS & METRICS
-- ===================================

-- Get comprehensive AST statistics
CREATE OR REPLACE MACRO ast_stats(ast_struct) AS (
    WITH node_stats AS (
        SELECT 
            count(*) as total_nodes,
            count(DISTINCT node.type) as unique_types,
            max(node.depth) as max_depth,
            avg(node.children_count::FLOAT) as avg_children,
            max(node.children_count) as max_children,
            sum(CASE WHEN node.children_count = 0 THEN 1 ELSE 0 END) as leaf_nodes
        FROM ast_nodes(ast_struct)
    ),
    type_counts AS (
        SELECT 
            node.type,
            count(*) as count
        FROM ast_nodes(ast_struct)
        GROUP BY node.type
        ORDER BY count DESC
        LIMIT 10
    )
    SELECT 
        ast_struct.file_path,
        ast_struct.language,
        ns.*,
        list(struct_pack(type := tc.type, count := tc.count)) as top_types
    FROM node_stats ns, (SELECT array_agg(tc) as tc_array FROM type_counts tc) tca,
         unnest(tca.tc_array) as tc
    GROUP BY ast_struct.file_path, ast_struct.language, ns.total_nodes, ns.unique_types, 
             ns.max_depth, ns.avg_children, ns.max_children, ns.leaf_nodes
);

-- Get type distribution
CREATE OR REPLACE MACRO ast_type_counts(ast_struct) AS (
    SELECT 
        node.type,
        count(*) as count,
        round(count(*) * 100.0 / (SELECT count(*) FROM ast_nodes(ast_struct)), 2) as percentage
    FROM ast_nodes(ast_struct)
    GROUP BY node.type
    ORDER BY count DESC
);

-- Get depth distribution
CREATE OR REPLACE MACRO ast_depth_counts(ast_struct) AS (
    SELECT 
        node.depth,
        count(*) as count
    FROM ast_nodes(ast_struct)
    GROUP BY node.depth
    ORDER BY node.depth
);

-- ===================================
-- LANGUAGE-SPECIFIC HELPERS
-- ===================================

-- JavaScript/TypeScript specific functions
CREATE OR REPLACE MACRO ast_js_functions(ast_struct) AS (
    ast_names(ast_struct, 'function_declaration')
    UNION ALL
    ast_names(ast_struct, 'arrow_function')
    UNION ALL  
    ast_names(ast_struct, 'method_definition')
);

CREATE OR REPLACE MACRO ast_js_classes(ast_struct) AS (
    ast_names(ast_struct, 'class_declaration')
);

CREATE OR REPLACE MACRO ast_js_imports(ast_struct) AS (
    SELECT node.source_text
    FROM ast_nodes(ast_struct)
    WHERE node.type = 'import_statement'
);

-- Python specific functions
CREATE OR REPLACE MACRO ast_py_functions(ast_struct) AS (
    ast_names(ast_struct, 'function_definition')
);

CREATE OR REPLACE MACRO ast_py_classes(ast_struct) AS (
    ast_names(ast_struct, 'class_definition')
);

CREATE OR REPLACE MACRO ast_py_imports(ast_struct) AS (
    SELECT node.source_text
    FROM ast_nodes(ast_struct)
    WHERE node.type IN ('import_statement', 'import_from_statement')
);

-- C++ specific functions  
CREATE OR REPLACE MACRO ast_cpp_functions(ast_struct) AS (
    ast_names(ast_struct, 'function_definition')
    UNION ALL
    ast_names(ast_struct, 'function_declarator')
);

CREATE OR REPLACE MACRO ast_cpp_classes(ast_struct) AS (
    ast_names(ast_struct, 'class_specifier')
);

CREATE OR REPLACE MACRO ast_cpp_includes(ast_struct) AS (
    SELECT node.source_text
    FROM ast_nodes(ast_struct)
    WHERE node.type = 'preproc_include'
);