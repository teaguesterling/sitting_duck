# AST API v2 Proposal: Structure-Preserving Macros

## Problem Statement

Current macros return only JSON arrays of nodes, discarding important metadata (file_path, language, node_count, max_depth). This makes it difficult to:
1. Chain operations while preserving context
2. Track file/line information through transformations
3. Build comprehensive code analysis tables

## Proposed Solution: Dual Macro Types

### 1. Structure-Preserving Macros (`ast_*`)

These return the full AST object structure, enabling chaining:

```sql
-- Current structure from read_ast_objects:
-- {
--   file_path: VARCHAR,
--   language: VARCHAR, 
--   node_count: INTEGER,
--   max_depth: INTEGER,
--   nodes: JSON
-- }

-- Structure-preserving macros return same shape:
CREATE OR REPLACE MACRO ast_at_depth(ast_obj, depth) AS (
    STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := json_array_length(filtered_nodes),
        max_depth := CASE 
            WHEN json_array_length(filtered_nodes) > 0 
            THEN depth 
            ELSE 0 
        END,
        nodes := filtered_nodes
    )
    FROM (
        SELECT (
            SELECT json_group_array(je.value)
            FROM json_each(ast_obj.nodes) AS je
            WHERE json_extract(je.value, '$.depth')::INTEGER = depth
        ) as filtered_nodes
    )
);

-- Enables chaining:
-- ast_obj.ast_at_depth(4).ast_filter_type('function_definition')
```

### 2. Extraction Macros (`ast_extract_*`)

These return simple datatypes for final results:

```sql
-- Returns VARCHAR[] instead of JSON
CREATE OR REPLACE MACRO ast_extract_function_names(ast_obj) AS (
    (SELECT ARRAY_AGG(json_extract_string(je.value, '$.name'))
     FROM json_each(ast_obj.nodes) AS je
     WHERE json_extract_string(je.value, '$.type') = 'function_definition'
       AND json_extract_string(je.value, '$.name') IS NOT NULL)
);

-- Returns structured table data
CREATE OR REPLACE MACRO ast_extract_entities(ast_obj) AS (
    SELECT 
        ast_obj.file_path as file,
        json_extract(je.value, '$.start.line')::INTEGER as start_line,
        json_extract(je.value, '$.end.line')::INTEGER as end_line,
        json_extract_string(je.value, '$.type') as entity_type,
        json_extract_string(je.value, '$.name') as entity_name,
        json_extract(je.value, '$.id')::INTEGER as node_id
    FROM json_each(ast_obj.nodes) AS je
    WHERE json_extract_string(je.value, '$.type') IN 
        ('function_definition', 'class_definition', 'module')
);
```

## Implementation Examples

### Example 1: Chained Filtering
```sql
WITH filtered_ast AS (
    SELECT ast_obj
        .ast_at_depth(4)
        .ast_filter_type('function_definition')
        .ast_filter_name_pattern('%test%') as filtered
    FROM read_ast_objects('src/tests.py', 'python') as ast_obj
)
SELECT 
    filtered.file_path,
    filtered.node_count as test_function_count,
    ast_extract_function_names(filtered) as test_functions
FROM filtered_ast;
```

### Example 2: Source Extraction with Context
```sql
-- New macro for source extraction with padding
CREATE OR REPLACE MACRO ast_extract_source(ast_obj, pad_lines DEFAULT 0) AS (
    SELECT 
        ast_obj.file_path,
        json_extract(je.value, '$.id')::INTEGER as node_id,
        json_extract_string(je.value, '$.type') as node_type,
        json_extract_string(je.value, '$.name') as node_name,
        GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines) as context_start,
        json_extract(je.value, '$.end.line')::INTEGER + pad_lines as context_end,
        -- Would need a UDF to actually read file lines
        get_file_lines(
            ast_obj.file_path, 
            GREATEST(1, json_extract(je.value, '$.start.line')::INTEGER - pad_lines),
            json_extract(je.value, '$.end.line')::INTEGER + pad_lines
        ) as source_with_context
    FROM json_each(ast_obj.nodes) AS je
);
```

### Example 3: Comprehensive Code Analysis Table
```sql
WITH ast_data AS (
    SELECT * FROM read_ast_objects('src/**/*.py', 'python')
),
entity_info AS (
    SELECT 
        file,
        start_line || '-' || end_line as lines,
        CASE 
            WHEN entity_type = 'class_definition' THEN entity_name
            WHEN entity_type = 'function_definition' THEN 
                -- Would need parent tracking for full path
                COALESCE(parent_class || '.', '') || entity_name
        END as entity,
        entity_type,
        node_id
    FROM ast_data, 
         TABLE(ast_extract_entities(ast_data))
    WHERE entity_type IN ('function_definition', 'class_definition')
),
-- These would come from other analysis tools
annotations AS (
    SELECT node_id, 
           'implementation_status: PARTIAL, risk: exposing sensitive data' as annotations
    FROM security_analysis
),
metrics AS (
    SELECT node_id,
           'complexity: 0.8, coverage: 70%' as metrics  
    FROM code_metrics
),
test_results AS (
    SELECT node_id,
           'failed: 2, skipped: 1, passing: 7' as test_results
    FROM test_runner
)
SELECT 
    e.file,
    e.lines,
    e.entity,
    '(params:str ...) -> Value' as signature, -- Would extract from AST
    a.annotations,
    m.metrics,
    t.test_results
FROM entity_info e
LEFT JOIN annotations a ON e.node_id = a.node_id
LEFT JOIN metrics m ON e.node_id = m.node_id
LEFT JOIN test_results t ON e.node_id = t.node_id;
```

## Proposed Macro Categorization

### Structure-Preserving (`ast_*`)
- `ast_filter_type(ast_obj, type)` - filter by node type
- `ast_filter_types(ast_obj, type_list)` - filter by multiple types
- `ast_at_depth(ast_obj, depth)` - nodes at specific depth
- `ast_at_line(ast_obj, line)` - nodes containing line
- `ast_children_of(ast_obj, node_id)` - direct children
- `ast_descendants_of(ast_obj, node_id)` - all descendants
- `ast_filter_name_pattern(ast_obj, pattern)` - filter by name pattern

### Extraction (`ast_extract_*`)
- `ast_extract_function_names(ast_obj)` -> VARCHAR[]
- `ast_extract_class_names(ast_obj)` -> VARCHAR[]
- `ast_extract_entities(ast_obj)` -> TABLE
- `ast_extract_source(ast_obj, pad_lines)` -> TABLE
- `ast_extract_summary(ast_obj)` -> STRUCT
- `ast_extract_type_counts(ast_obj)` -> MAP

### Convenience Aliases (for backward compatibility)
- `ast_function_names()` -> calls `ast_extract_function_names()`
- `ast_summary()` -> calls `ast_extract_summary()`

## Benefits

1. **Preserves Context**: File path and metadata flow through transformations
2. **Enables Chaining**: Natural method-like syntax for complex queries
3. **Type Safety**: Clear distinction between AST objects and extracted data
4. **Source Tracking**: Easy to get back to original source with context
5. **Extensible**: New structure-preserving operations can be added easily

## Implementation Priority

1. **Phase 1**: Implement core structure-preserving macros
   - `ast_filter_type`, `ast_at_depth`, `ast_at_line`
   
2. **Phase 2**: Add extraction macros with proper types
   - `ast_extract_*` returning appropriate SQL types
   
3. **Phase 3**: Add source extraction capability
   - `get_file_lines()` UDF
   - `ast_extract_source()` macro
   
4. **Phase 4**: Advanced navigation
   - Parent/child/sibling traversal
   - Path construction

This approach would make the AST extension significantly more powerful for real-world code analysis scenarios.