# AST Macro Naming Convention

## Naming Rules

1. **`ast_*`** - Returns AST structure (preserves metadata)
2. **`ast_extract_*`** - Returns simple types (VARCHAR, INTEGER, TABLE)
3. **`ast_safe_*`** - Returns safe defaults instead of NULL

## Examples by Category

### Structure-Preserving Operations (`ast_*`)

| Macro | Input | Output | Description |
|-------|-------|--------|-------------|
| `ast_filter_type()` | AST object, type | AST object | Filters nodes, keeps metadata |
| `ast_at_depth()` | AST object, depth | AST object | Nodes at depth, keeps metadata |
| `ast_children_of()` | AST object, node_id | AST object | Child nodes, keeps metadata |
| `ast_filter_name_pattern()` | AST object, pattern | AST object | Pattern match, keeps metadata |

```sql
-- These can be chained because they return AST objects
SELECT ast_obj
    .ast_filter_type('function_definition')
    .ast_at_depth_range(2, 4)
    .ast_filter_name_pattern('%test%') as filtered
FROM read_ast_objects('file.py', 'python') as ast_obj;
```

### Extraction Operations (`ast_extract_*`)

| Macro | Input | Output | Description |
|-------|-------|--------|-------------|
| `ast_extract_function_names()` | AST object | VARCHAR[] | Array of function names |
| `ast_extract_entities()` | AST object | TABLE | Table of entities with details |
| `ast_extract_summary()` | AST object | STRUCT | Summary statistics |
| `ast_extract_source()` | AST object, padding | TABLE | Source code with context |

```sql
-- These return final results, not AST objects
SELECT 
    ast_extract_function_names(filtered) as names,
    ast_extract_summary(filtered).function_count as count
FROM (
    SELECT ast_filter_type(ast_obj, 'function_definition') as filtered
    FROM read_ast_objects('file.py', 'python') as ast_obj
);
```

### Safe Operations (`ast_safe_*`)

| Macro | Input | Output | Description |
|-------|-------|--------|-------------|
| `ast_safe_filter_type()` | AST object, type | AST object | Empty AST if no matches |
| `ast_safe_extract_names()` | AST object | VARCHAR[] | Empty array if no names |

```sql
-- Never returns NULL, always valid result
SELECT 
    ast_safe_extract_function_names(ast_obj) as functions  -- [] if none
FROM read_ast_objects('empty.py', 'python') as ast_obj;
```

## Migration Examples

### Old API (v1)
```sql
-- Returns JSON, loses context
SELECT ast_function_names(nodes) as funcs
FROM read_ast_objects('file.py', 'python');
```

### New API (v2)
```sql
-- Option 1: Direct extraction (similar to v1)
SELECT ast_extract_function_names(ast_obj) as funcs
FROM read_ast_objects('file.py', 'python') as ast_obj;

-- Option 2: With filtering (preserves context)
SELECT 
    filtered.file_path,
    ast_extract_function_names(filtered) as funcs
FROM read_ast_objects('file.py', 'python') as ast_obj,
     LATERAL ast_filter_type(ast_obj, 'function_definition') as filtered;
```

## Backward Compatibility

Keep simplified aliases for common operations:
```sql
-- These would be aliases to extraction functions
ast_function_names(nodes) -> ast_extract_function_names(STRUCT_PACK(nodes := nodes))
ast_summary(nodes) -> ast_extract_summary(STRUCT_PACK(nodes := nodes))
```

This allows existing queries to continue working while encouraging the new patterns.