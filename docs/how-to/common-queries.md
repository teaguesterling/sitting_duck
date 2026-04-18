# Common Queries

Recipes for everyday code-analysis tasks with Sitting Duck.
All examples use `test/data/python/sample_app.py` unless noted otherwise.

---

## Finding Definitions

List all function definitions in a file.

```sql
SELECT name, start_line, end_line
FROM read_ast('test/data/python/sample_app.py')
WHERE semantic_type = 'DEFINITION_FUNCTION';
```

The same query with `ast_select` -- CSS pseudo-classes map directly to semantic types.

```sql
SELECT name, start_line, end_line
FROM ast_select('test/data/python/sample_app.py', ':definition:function');
```

List class definitions, using `descendant_count` as a rough complexity metric.

```sql
SELECT name, start_line, descendant_count
FROM read_ast('test/data/python/sample_app.py')
WHERE semantic_type = 'DEFINITION_CLASS';
```

Find a specific definition by name with the `#name` selector.

```sql
SELECT * FROM ast_select('test/data/python/sample_app.py',
    'function_definition#validate_email');
```

All named definitions (functions, classes, variables) in one query.

```sql
SELECT name, semantic_type, start_line
FROM read_ast('test/data/python/sample_app.py')
WHERE semantic_type LIKE 'DEFINITION_%'
  AND name IS NOT NULL;
```

---

## Navigating Structure

Get the direct children of a class using raw SQL.

```sql
WITH cls AS (
    SELECT node_id FROM read_ast('test/data/python/sample_app.py')
    WHERE type = 'class_definition' AND name = 'UserService'
)
SELECT type, name, start_line
FROM read_ast('test/data/python/sample_app.py')
WHERE parent_id = (SELECT node_id FROM cls);
```

The `>` child combinator in `ast_select` does the same thing.

```sql
SELECT name FROM ast_select('test/data/python/sample_app.py',
    'class_definition#UserService > function_definition');
```

Find all functions inside a class at any nesting depth (space = descendant combinator).

```sql
SELECT name FROM ast_select('test/data/python/sample_app.py',
    'class_definition#DatabaseConnection function_definition');
```

Find deeply nested code -- high `depth` values often signal hard-to-read code.

```sql
SELECT type, name, depth, start_line
FROM read_ast('test/data/python/sample_app.py')
WHERE depth > 5
ORDER BY depth DESC;
```

---

## Scope Queries

Find what function each call expression lives inside -- `scope.function` gives the enclosing function's `node_id` without any joins.

```sql
SELECT name, scope.function AS enclosing_fn
FROM read_ast('test/data/python/sample_app.py')
WHERE semantic_type = 'COMPUTATION_CALL';
```

Separate methods from top-level functions using `scope.class`.

```sql
-- Methods (inside a class)
SELECT name, scope.class
FROM read_ast('test/data/python/sample_app.py')
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND scope.class IS NOT NULL;

-- Top-level functions only (not inside a class or another function)
SELECT name, start_line
FROM read_ast('test/data/python/sample_app.py')
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND scope.class IS NULL
  AND scope.function IS NULL;
```

---

## Source Text

View function signatures via the `peek` column (~64 chars by default).

```sql
SELECT name, peek
FROM read_ast('test/data/python/sample_app.py')
WHERE semantic_type = 'DEFINITION_FUNCTION';
```

Increase `peek_size` for full function bodies, or use `peek_mode := 'smart'` for the most informative portion.

```sql
SELECT name, peek
FROM read_ast('test/data/python/sample_app.py', peek_size := 500)
WHERE semantic_type = 'DEFINITION_FUNCTION';

SELECT name, peek
FROM read_ast('test/data/python/sample_app.py', peek_mode := 'smart')
WHERE semantic_type = 'DEFINITION_CLASS';
```

---

## Cross-File Queries

Scan all Python files under a directory with a glob pattern.

```sql
SELECT file_path, name, start_line
FROM read_ast('src/**/*.py')
WHERE semantic_type = 'DEFINITION_FUNCTION';
```

The same with `ast_select`.

```sql
SELECT file_path, name
FROM ast_select('src/**/*.py', ':definition:class');
```

Count definitions per file to find the densest files in your codebase.

```sql
SELECT file_path, COUNT(*) AS defs
FROM read_ast('src/**/*.py')
WHERE semantic_type LIKE 'DEFINITION_%'
GROUP BY file_path
ORDER BY defs DESC;
```

Use `ignore_errors` to skip files that fail to parse instead of aborting the query.

```sql
SELECT file_path, COUNT(*) AS nodes
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY file_path;
```

---

## Further Reading

- [Function Reference](../reference/functions.md) -- full signatures for `read_ast`, `ast_select`, and all macros
- [CSS Selectors](../reference/css-selectors.md) -- complete selector syntax (combinators, pseudo-classes, attributes)
- [Output Schema](../reference/output-schema.md) -- every column in the AST output, including `scope` fields
