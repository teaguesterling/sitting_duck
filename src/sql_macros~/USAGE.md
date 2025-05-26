# SQL Macro Usage Guide

## Single vs Multiple Values

DuckDB SQL macros are text substitution, not true functions. To support both single values and lists, we provide separate macros:

### Type Filtering
```sql
-- Single type
SELECT ast_find_type(nodes, 'function_definition') FROM read_ast_objects('file.py', 'python');

-- Multiple types  
SELECT ast_find_types(nodes, ['function_definition', 'class_definition']) FROM read_ast_objects('file.py', 'python');
```

### Depth Filtering
```sql
-- Single depth
SELECT ast_at_depth(nodes, 2) FROM read_ast_objects('file.py', 'python');

-- Multiple depths
SELECT ast_at_depths(nodes, [2, 3, 4]) FROM read_ast_objects('file.py', 'python');
```

### Line Filtering
```sql
-- Single line
SELECT ast_at_line(nodes, 42) FROM read_ast_objects('file.py', 'python');

-- Multiple lines
SELECT ast_at_lines(nodes, [10, 20, 30]) FROM read_ast_objects('file.py', 'python');
```

## Why This Approach?

DuckDB macros don't have runtime type detection like PostgreSQL's `pg_typeof`. Instead of complex type handling, we provide clear, explicit macros for each use case. This is:

- **Simpler**: No complex CASE statements or type conversions
- **Clearer**: The macro name indicates what it accepts
- **More performant**: No runtime type checking overhead
- **More reliable**: No type conversion errors

## Migration from v1

If you have code using the overloaded approach, update it:

```sql
-- Old (doesn't work)
ast_find_type(nodes, ['function_definition', 'class_definition'])

-- New (works)
ast_find_types(nodes, ['function_definition', 'class_definition'])
```