# AST Helper Functions Design

## Current State
- `read_ast_objects()` returns: file_path, language, parse_time, node_count, max_depth, nodes (JSON)
- JSON structure: array of nodes with type, content, start_row, start_col, end_row, end_col, children

## Design Options

### 1. JSON Path Helper Functions
These work directly with the JSON column:

```sql
-- Find nodes by type
ast_find_by_type(json_nodes, type_name) -> JSON array of matching nodes

-- Get node children
ast_children(json_nodes, json_path) -> JSON array of children

-- Filter nodes
ast_filter(json_nodes, property, operator, value) -> JSON array of matching nodes

-- Extract specific properties
ast_extract(json_nodes, json_path) -> JSON array of extracted values
```

### 2. Table-Generating Functions
These return structured tables:

```sql
-- Language-specific extractors
ast_functions(file_path, language) -> TABLE(name, params, body_start, body_end)
ast_classes(file_path, language) -> TABLE(name, methods, attributes)
ast_imports(file_path, language) -> TABLE(module, names, alias)

-- Generic pattern matching
ast_match(file_path, language, pattern) -> TABLE(node_type, content, location, parent_path)
```

### 3. Navigation Functions
For traversing the tree:

```sql
-- Get parent of a node
ast_parent(json_nodes, node_path) -> JSON object

-- Get siblings
ast_siblings(json_nodes, node_path) -> JSON array

-- Get descendants matching criteria
ast_descendants(json_nodes, node_path, type_filter) -> JSON array
```

## Recommended Approach

Start with JSON helper functions because:
1. They're composable with existing JSON functions
2. They don't require pre-defining schemas
3. They work with our current hybrid design
4. Users can build custom queries on top

Example usage:
```sql
-- Find all function calls to 'print'
WITH ast_data AS (
    SELECT * FROM read_ast_objects('script.py', 'python')
)
SELECT 
    json_extract_string(func_node, '$.content') as function_name,
    json_extract_string(func_node, '$.start_row') as line_number
FROM (
    SELECT unnest(ast_find_by_type(nodes, 'call')) as func_node
    FROM ast_data
) 
WHERE json_extract_string(func_node, '$.children[0].content') = 'print';
```

## Implementation Plan

1. **Phase 1: Core JSON Helpers**
   - ast_find_by_type(nodes, type)
   - ast_filter(nodes, path, op, value)
   - ast_children(nodes, path)

2. **Phase 2: Navigation Helpers**  
   - ast_parent(nodes, path)
   - ast_siblings(nodes, path)
   - ast_descendants(nodes, path, filter)

3. **Phase 3: Table Functions**
   - ast_functions(file, lang)
   - ast_classes(file, lang)
   - ast_imports(file, lang)

4. **Phase 4: Pattern Matching**
   - ast_match(file, lang, pattern)
   - ast_extract_pattern(nodes, pattern)