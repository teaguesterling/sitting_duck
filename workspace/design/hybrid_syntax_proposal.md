# Hybrid Syntax Proposal: Combining Macros with Struct Results

## Concept

While we can't use dot notation to *call* macros, we can design macros that return structs/JSON objects that support dot notation for accessing their fields.

## Example Design

```sql
-- Macro returns a struct with multiple "methods" as fields
CREATE OR REPLACE MACRO ast_node_info(node) AS
    {
        'type': json_extract_string(node, '$.type'),
        'name': json_extract_string(node, '$.name'),
        'children': json_extract(node, '$.children'),
        'is_function': json_extract_string(node, '$.type') = 'function_definition',
        'line_range': {
            'start': json_extract(node, '$.start.line'),
            'end': json_extract(node, '$.end.line')
        }
    };

-- Usage: Call macro, then use dot notation on result
SELECT 
    ast_node_info(je.value).type as node_type,
    ast_node_info(je.value).is_function as is_func,
    ast_node_info(je.value).line_range.start as start_line
FROM read_ast_objects('test.py', 'python') AS ast,
     json_each(ast.nodes) AS je
WHERE ast_node_info(je.value).is_function;
```

## Benefits

1. **Intuitive access**: Once you have the struct, dot notation works naturally
2. **Grouped functionality**: Related operations are bundled together
3. **Type safety**: Struct fields have defined types
4. **Performance**: The macro is called once, then fields are accessed

## Implementation Pattern

```sql
-- Instead of multiple macros:
ast_node_type(node)
ast_node_name(node)
ast_node_children(node)
ast_is_function(node)

-- One macro with struct result:
ast_info(node).type
ast_info(node).name
ast_info(node).children
ast_info(node).is_function
```

## Limitations

1. Still can't do `node.ast_info()` - must use `ast_info(node)`
2. Potential performance impact if struct is built but only one field is used
3. More complex macro definitions

## Recommendation

For the most commonly used operations, provide both:
1. Simple macros for direct access (e.g., `ast_find_type()`)
2. Struct-returning macros for rich object access (e.g., `ast_info()`)