# AST as JSON Design

## Overview
Use DuckDB's native JSON type for AST representation, leveraging built-in JSON path queries.

## Structure
```json
{
  "file_path": "example.py",
  "language": "python",
  "stats": {
    "node_count": 150,
    "max_depth": 8,
    "has_syntax_errors": false
  },
  "nodes": [
    {
      "id": 0,
      "type": "module",
      "name": null,
      "parent_id": null,
      "children": [1, 2, 3],
      "start": {"line": 1, "column": 0},
      "end": {"line": 50, "column": 0}
    },
    {
      "id": 1,
      "type": "function_definition",
      "name": "main",
      "parent_id": 0,
      "children": [4, 5],
      "start": {"line": 1, "column": 0},
      "end": {"line": 10, "column": 0}
    }
  ],
  "index": {
    "functions": [1, 15, 27],
    "classes": [35, 67],
    "imports": [2, 3]
  }
}
```

## Query Examples
```sql
-- Get all function names using JSON path
SELECT 
    file_path,
    json_extract_string(ast, '$.language') as lang,
    json_extract(ast, '$.nodes[?(@.type=="function_definition")].name') as function_names
FROM read_ast_objects('*.py', 'python');

-- Count nodes by type
SELECT 
    file_path,
    json_group_object(
        json_extract_string(node, '$.type'),
        count(*)
    ) as type_counts
FROM read_ast_objects('*.py', 'python'),
     json_each(ast, '$.nodes') as node
GROUP BY file_path;

-- Find deep nesting
SELECT 
    file_path,
    json_extract(ast, '$.stats.max_depth') as max_depth
FROM read_ast_objects('*.py', 'python')
WHERE json_extract(ast, '$.stats.max_depth') > 10;

-- Extract with helper macros
CREATE MACRO ast_functions(ast_json) AS
    json_extract(ast_json, '$.nodes[?(@.type=="function_definition")]');

CREATE MACRO ast_node_count(ast_json) AS
    json_extract(ast_json, '$.stats.node_count');
```

## Benefits
1. **No custom type needed** - Works today with DuckDB's JSON support
2. **Flexible queries** - JSON path is powerful
3. **Easy integration** - Standard JSON tools work
4. **Performance** - DuckDB optimizes JSON operations
5. **Forward compatible** - Can migrate to custom type later

## Implementation Steps
1. Modify `read_ast_objects` to return JSON type instead of BLOB
2. Structure JSON with both flat nodes array and indexes
3. Create helper MACROs for common operations
4. Document JSON path patterns