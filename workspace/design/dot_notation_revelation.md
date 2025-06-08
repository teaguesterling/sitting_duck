# Dot Notation with DuckDB Macros: The Parenthesis Solution

## Major Discovery

Dot notation DOES work with DuckDB macros when the left-hand side is parenthesized!

## Syntax Rules

### ✅ Works: Parenthesized Dot Notation
```sql
-- Direct value
SELECT (5).double_value();

-- Column reference  
SELECT (column_name).macro_name(args) FROM table;

-- Expression result
SELECT ((a + b)).macro_name(args) FROM table;

-- Chained calls (single parentheses work!)
SELECT (nodes).ast_find_type('function').some_other_macro();
```

### ❌ Fails: Unparenthesized Dot Notation
```sql
-- These all fail
SELECT 5.double_value();
SELECT column_name.macro_name(args) FROM table;
SELECT nodes.ast_find_type('function') FROM table;
```

## Why This Works

The parentheses force DuckDB to:
1. First evaluate the expression inside the parentheses
2. Then apply the dot notation as a method-like syntax on the result
3. Transform it internally to the macro call: `macro_name(expression, other_args)`

## Practical Examples

### Example 1: Simple Transformation
```sql
-- Both are equivalent:
SELECT (value).double_value() FROM data;
SELECT double_value(value) FROM data;
```

### Example 2: AST Queries with Natural Syntax
```sql
-- Natural method-like syntax
SELECT 
    (nodes).ast_find_type('function_definition'),
    (nodes).ast_function_names(),
    (nodes).ast_summary()
FROM read_ast_objects('file.py', 'python');

-- Equivalent function syntax
SELECT 
    ast_find_type(nodes, 'function_definition'),
    ast_function_names(nodes),
    ast_summary(nodes)
FROM read_ast_objects('file.py', 'python');
```

### Example 3: Chaining Operations
```sql
-- Create a macro that returns a struct
CREATE OR REPLACE MACRO analyze(data) AS {
    'filtered': filter_somehow(data),
    'counted': count_something(data),
    'summary': summarize(data)
};

-- Natural chaining with parentheses
SELECT 
    (data).analyze().get_field('filtered'),
    (data).analyze().get_field('summary')
FROM source_table;
```

## Design Implications

### 1. We CAN Support Both Syntaxes!
Users can choose their preferred style:
- Function-style: `ast_find_type(nodes, 'function')`
- Method-style: `(nodes).ast_find_type('function')`

### 2. Documentation Should Show Both
```sql
-- Find all functions - choose your style:
-- Option 1: Function syntax
SELECT ast_find_type(nodes, 'function_definition') FROM ast_table;

-- Option 2: Method syntax (requires parentheses)
SELECT (nodes).ast_find_type('function_definition') FROM ast_table;
```

### 3. Complex Expressions Need Extra Parentheses
```sql
-- When chaining or using complex expressions
SELECT ((je.value).node_info()).type
FROM ast_table, json_each(nodes) as je;
```

## Best Practices

1. **Be consistent**: Choose one style and stick with it in a query
2. **Use parentheses liberally**: When in doubt, add parentheses
3. **Document both styles**: Let users choose what feels natural
4. **Test both**: Ensure macros work correctly with both syntaxes

## Updated Test Strategy

We should update our tests to:
1. ✅ Show that parenthesized dot notation works
2. ✅ Show that unparenthesized dot notation fails  
3. ✅ Demonstrate equivalence between both syntaxes
4. ✅ Test chaining and complex expressions
5. ✅ Include examples in documentation