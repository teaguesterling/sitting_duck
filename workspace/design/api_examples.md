# AST API Usage Examples

## Three API Styles

### 1. Traditional Function Style
```sql
SELECT 
    ast_find_type(nodes, 'function_definition'),
    ast_function_names(nodes),
    ast_summary(nodes)
FROM read_ast_objects('file.py', 'python');
```

### 2. Method Style (with parentheses)
```sql
SELECT 
    (nodes).ast_find_type('function_definition'),
    (nodes).ast_function_names(),
    (nodes).ast_summary()
FROM read_ast_objects('file.py', 'python');
```

### 3. Entrypoint Style (recommended)
```sql
SELECT 
    ast(nodes).find_type('function_definition'),
    ast(nodes).function_names(),
    ast(nodes).summary()
FROM read_ast_objects('file.py', 'python');
```

## Benefits of the Entrypoint Style

### Type Safety & Normalization
```sql
-- All of these work the same way:
ast(nodes)                    -- Raw JSON array
ast(result)                   -- Struct with nodes field
ast('[]')                     -- String JSON
ast(NULL)                     -- Null input → empty array
ast(single_node_object)       -- Single object → wrapped in array
```

### Natural Chaining
```sql
-- Find functions, extract names, count them
SELECT ast(nodes)
    .find_type('function_definition')
    .extract_names()
    .count_elements()
FROM read_ast_objects('file.py', 'python');

-- Multi-step filtering and analysis
SELECT ast(nodes)
    .where_depth(1)                    -- Top-level nodes
    .where_type('function_definition')  -- Only functions
    .first_element()                   -- Get first one
FROM read_ast_objects('file.py', 'python');
```

### Complex Queries
```sql
-- Analyze code structure
WITH analysis AS (
    SELECT 
        ast(nodes).find_type('function_definition') as functions,
        ast(nodes).find_type('class_definition') as classes,
        ast(nodes).identifiers() as identifiers,
        ast(nodes).summary() as summary
    FROM read_ast_objects('file.py', 'python')
)
SELECT 
    functions.count_elements() as function_count,
    classes.extract_names() as class_names,
    identifiers.count_elements() as identifier_count,
    json_extract(summary, '$.max_depth') as max_depth
FROM analysis;
```

### Working with Individual Nodes
```sql
-- Process each function individually
SELECT 
    json_extract_string(je.value, '$.name') as func_name,
    ast(je.value).where_type('identifier').count_elements() as id_count
FROM read_ast_objects('file.py', 'python') AS ast_result,
     json_each(ast(ast_result.nodes).find_type('function_definition')) AS je;
```

## Migration Guide

### From Traditional to Entrypoint Style

```sql
-- Old style:
SELECT ast_find_type(nodes, ['function_definition', 'class_definition'])
FROM read_ast_objects('file.py', 'python');

-- New style:
SELECT ast(nodes).find_type(['function_definition', 'class_definition'])
FROM read_ast_objects('file.py', 'python');
```

### Chaining Operations

```sql
-- Old style (nested functions):
SELECT json_array_length(ast_function_names(ast_find_type(nodes, 'function_definition')))
FROM read_ast_objects('file.py', 'python');

-- New style (natural chaining):
SELECT ast(nodes).find_type('function_definition').extract_names().count_elements()
FROM read_ast_objects('file.py', 'python');
```

## Best Practices

1. **Use ast() as your starting point** for all AST operations
2. **Chain operations naturally** instead of nesting function calls
3. **Leverage type normalization** - ast() handles different input types
4. **Use descriptive method names** like `count_elements()` instead of `json_array_length()`
5. **Combine with standard SQL** for complex analysis

## Performance Notes

- The `ast()` normalization is lightweight
- Chaining doesn't create intermediate results until needed
- Use `LIMIT` and `WHERE` clauses to filter early when possible
- Consider creating views for complex repeated queries