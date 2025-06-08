# Using json_each and json_tree with AST Data

## Overview

With DuckDB 1.3+, we have access to powerful JSON table functions that make querying AST data much more natural.

## json_each - Iterate Over Top-Level Nodes

`json_each` traverses the JSON array and returns one row for each element.

### Basic Usage

```sql
-- Get all nodes with their array index and type
SELECT 
    je.key as node_index,
    json_extract_string(je.value, '$.type') as node_type,
    json_extract_string(je.value, '$.name') as node_name
FROM read_ast_objects('file.py', 'python') AS ast,
     json_each(ast.nodes) AS je
WHERE json_extract_string(je.value, '$.name') IS NOT NULL;
```

### Finding Specific Node Types

```sql
-- Find all function definitions with line numbers
SELECT 
    json_extract_string(je.value, '$.name') as function_name,
    json_extract(je.value, '$.start.line') as start_line,
    json_extract(je.value, '$.end.line') as end_line
FROM read_ast_objects('file.py', 'python') AS ast,
     json_each(ast.nodes) AS je
WHERE json_extract_string(je.value, '$.type') = 'function_definition';
```

### Analyzing Node Distribution

```sql
-- Count nodes by type
SELECT 
    json_extract_string(je.value, '$.type') as node_type,
    COUNT(*) as count
FROM read_ast_objects('file.py', 'python') AS ast,
     json_each(ast.nodes) AS je
GROUP BY node_type
ORDER BY count DESC;
```

## json_tree - Deep Traversal of Individual Nodes

`json_tree` traverses a JSON structure in depth-first fashion, useful for exploring the internal structure of nodes.

### Exploring Node Structure

```sql
-- Examine the structure of the first function definition
WITH first_function AS (
    SELECT je.value as func_node
    FROM read_ast_objects('file.py', 'python') AS ast,
         json_each(ast.nodes) AS je
    WHERE json_extract_string(je.value, '$.type') = 'function_definition'
    LIMIT 1
)
SELECT 
    jt.key,
    jt.value,
    jt.type,
    jt.fullkey,
    jt.path
FROM first_function,
     json_tree(func_node) AS jt
WHERE jt.type NOT IN ('OBJECT', 'ARRAY');
```

### Finding All Values in Nested Structure

```sql
-- Extract all string values from the AST
SELECT DISTINCT jt.value
FROM read_ast_objects('file.py', 'python') AS ast,
     json_each(ast.nodes) AS je,
     json_tree(je.value) AS jt
WHERE jt.type = 'VARCHAR'
  AND jt.key IN ('name', 'content', 'value');
```

## Combined Patterns

### Parent-Child Relationships

```sql
-- Find functions and their immediate children
WITH functions AS (
    SELECT 
        je.key as func_index,
        je.value as func_node,
        json_extract_string(je.value, '$.name') as func_name,
        json_extract(je.value, '$.children') as child_indices
    FROM read_ast_objects('file.py', 'python') AS ast,
         json_each(ast.nodes) AS je
    WHERE json_extract_string(je.value, '$.type') = 'function_definition'
)
SELECT 
    f.func_name,
    child_je.key as child_position,
    json_extract_string(nodes_je.value, '$.type') as child_type
FROM functions f,
     json_each(f.child_indices) AS child_je,
     read_ast_objects('file.py', 'python') AS ast2,
     json_each(ast2.nodes) AS nodes_je
WHERE nodes_je.key = child_je.value::VARCHAR;
```

### Code Complexity Analysis

```sql
-- Analyze function complexity
WITH function_analysis AS (
    SELECT 
        json_extract_string(je.value, '$.name') as function_name,
        json_extract(je.value, '$.start.line') as start_line,
        json_extract(je.value, '$.end.line') as end_line,
        json_extract(je.value, '$.depth') as depth,
        json_array_length(json_extract(je.value, '$.children')) as direct_children
    FROM read_ast_objects('file.py', 'python') AS ast,
         json_each(ast.nodes) AS je
    WHERE json_extract_string(je.value, '$.type') = 'function_definition'
)
SELECT 
    function_name,
    (end_line - start_line + 1) as lines_of_code,
    direct_children,
    CASE 
        WHEN direct_children > 20 THEN 'Complex'
        WHEN direct_children > 10 THEN 'Medium'
        ELSE 'Simple'
    END as complexity
FROM function_analysis
ORDER BY direct_children DESC;
```

## Natural Querying with SQL Macros

Combine `json_each` with SQL macros for even more natural syntax:

```sql
-- Create a macro that uses json_each internally
CREATE OR REPLACE MACRO ast_functions_detailed(nodes) AS (
    SELECT 
        json_extract_string(je.value, '$.name') as name,
        json_extract(je.value, '$.start.line') as line,
        json_extract(je.value, '$.depth') as depth,
        je.value as full_node
    FROM json_each(nodes) AS je
    WHERE json_extract_string(je.value, '$.type') = 'function_definition'
);

-- Use with dot notation
SELECT * FROM read_ast_objects('file.py', 'python') AS ast, 
              ast.nodes.ast_functions_detailed();
```

## Performance Considerations

1. `json_each` is efficient for iterating over array elements
2. `json_tree` is more expensive as it does deep traversal
3. Use `json_each` when you only need top-level elements
4. Use `json_tree` when you need to explore nested structures

## Migration from unnest()

If you have queries using `unnest()`, they can be simplified:

```sql
-- Old approach
SELECT json_extract_string(node, '$.type') as type
FROM (SELECT unnest(json_extract(nodes, '$[*]')::JSON[]) as node
      FROM read_ast_objects('file.py', 'python'));

-- New approach with json_each
SELECT json_extract_string(je.value, '$.type') as type
FROM read_ast_objects('file.py', 'python') AS ast,
     json_each(ast.nodes) AS je;
```

The `json_each` approach is cleaner and provides additional metadata like array indices.