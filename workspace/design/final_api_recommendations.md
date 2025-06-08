# Final API Design Recommendations

## Executive Summary

Combining all feedback, we should implement:
1. **Structure-preserving** macros (`ast_*`) that maintain metadata
2. **Extraction** macros (`ast_extract_*`) that return SQL types
3. **List parameters** for filtering operations
4. **Named parameters** with sensible defaults
5. **AI-friendly** shortcuts and discovery functions

## Unified API Design

### Core Macros with Enhanced Signatures

```sql
-- Structure-Preserving (return AST objects)
ast_filter_type(ast_obj, types)  -- types can be string or list
ast_filter_name(ast_obj, patterns, case_sensitive := false)
ast_at_depth(ast_obj, depths)  -- depths can be int or list
ast_at_line(ast_obj, lines, include_partial := true)

-- Extraction (return SQL types)
ast_extract_names(ast_obj, types := NULL, pattern := NULL) RETURNS VARCHAR[]
ast_extract_source(ast_obj, node_ids := NULL, pad_lines := 0) RETURNS TABLE
ast_extract_entities(ast_obj, types := NULL) RETURNS TABLE
ast_extract_summary(ast_obj) RETURNS STRUCT

-- Safe Variants (never return NULL)
ast_safe_filter_type(ast_obj, types) RETURNS AST_OBJECT
ast_safe_extract_names(ast_obj, types := NULL) RETURNS VARCHAR[]
```

### Implementation Examples

```sql
-- Example 1: List parameter handling
CREATE OR REPLACE MACRO ast_filter_type(ast_obj, types) AS (
    WITH type_list AS (
        SELECT 
            CASE 
                WHEN pg_typeof(types) = 'text' THEN [types]
                WHEN pg_typeof(types) = 'text[]' THEN types
                ELSE [types::text]
            END as types_array
    ),
    filtered_nodes AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je,
             type_list
        WHERE list_contains(types_array, json_extract_string(je.value, '$.type'))
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := json_array_length(filtered_nodes.nodes),
        max_depth := (
            SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
            FROM json_each(filtered_nodes.nodes) as n
        ),
        nodes := filtered_nodes.nodes
    )
    FROM filtered_nodes
);

-- Example 2: Named parameters with defaults
CREATE OR REPLACE MACRO ast_extract_source(
    ast_obj, 
    node_ids := NULL,
    pad_lines := 0
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
        get_file_lines(
            ast_obj.file_path,
            json_extract(je.value, '$.start.line')::INTEGER,
            json_extract(je.value, '$.end.line')::INTEGER
        ) as source_text,
        get_file_lines(
            ast_obj.file_path,
            GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines),
            json_extract(je.value, '$.end.line')::INTEGER + pad_lines
        ) as source_with_context
    FROM json_each(ast_obj.nodes) AS je
    WHERE node_ids IS NULL 
       OR list_contains(node_ids, json_extract(je.value, '$.id')::BIGINT)
);

-- Example 3: AI-friendly shortcuts
CREATE OR REPLACE MACRO ast_test_functions(
    ast_obj,
    patterns := ['test_*', '*_test', 'Test*']
) AS (
    ast_filter_type(ast_obj, ['function_definition', 'method_definition'])
        .ast_filter_name(patterns)
);
```

## Migration Strategy

### Phase 1: Enhance Current Macros (1 day)
```sql
-- Update existing macros to accept lists
-- Old: ast_find_type(nodes, 'function_definition')
-- New: ast_find_type(nodes, ['function_definition', 'method_definition'])

-- Add parameter names where helpful
-- Old: ast_at_line(nodes, 42)  
-- New: ast_at_line(nodes, line := 42)
```

### Phase 2: Add Structure-Preserving Versions (2 days)
```sql
-- Add new structure-preserving macros
CREATE OR REPLACE MACRO ast_filter_type(ast_obj, types) AS ...;
CREATE OR REPLACE MACRO ast_at_depth(ast_obj, depths) AS ...;

-- Keep old versions for compatibility
CREATE OR REPLACE MACRO ast_find_type(nodes, type) AS (
    ast_extract_nodes(ast_filter_type(
        STRUCT_PACK(nodes := nodes), 
        type
    ))
);
```

### Phase 3: Add AI-Friendly Features (1 day)
```sql
-- Discovery functions
CREATE OR REPLACE MACRO ast_available_types(ast_obj);
CREATE OR REPLACE MACRO ast_suggest_macro(intent);

-- Semantic shortcuts  
CREATE OR REPLACE MACRO ast_methods(ast_obj);
CREATE OR REPLACE MACRO ast_public_api(ast_obj);
```

## Usage Examples for AI Agents

### Simple Query
```sql
-- Find all functions and methods
SELECT ast_extract_names(
    ast_obj, 
    types := ['function_definition', 'method_definition']
) as all_functions
FROM read_ast_objects('app.py', 'python') as ast_obj;
```

### Complex Analysis
```sql
-- Analyze test coverage
WITH test_analysis AS (
    SELECT 
        ast_obj.file_path,
        ast_test_functions(ast_obj) as test_funcs,
        ast_filter_type(ast_obj, 'function_definition') as all_funcs
    FROM read_ast_objects('**/*.py', 'python') as ast_obj
)
SELECT 
    file_path,
    ast_extract_summary(all_funcs).function_count as total_functions,
    ast_extract_summary(test_funcs).function_count as test_functions,
    ROUND(100.0 * ast_extract_summary(test_funcs).function_count / 
          NULLIF(ast_extract_summary(all_funcs).function_count, 0), 2) as test_percentage
FROM test_analysis
ORDER BY test_percentage DESC;
```

### Source Extraction
```sql
-- Get complex functions with source
SELECT *
FROM read_ast_objects('src/**/*.py', 'python') as ast_obj,
     TABLE(ast_extract_source(
         ast_filter_type(ast_obj, 'function_definition'),
         pad_lines := 3
     )) as src
WHERE src.line_count > 50
ORDER BY src.line_count DESC;
```

## Benefits Summary

1. **For Humans**: Clear, readable queries with named parameters
2. **For AI Agents**: Flexible inputs, helpful errors, discovery functions
3. **For Performance**: Structure preservation reduces redundant parsing
4. **For Maintenance**: Consistent patterns, clear migration path

This design creates an API that is both powerful and approachable, suitable for both human developers and AI agents.