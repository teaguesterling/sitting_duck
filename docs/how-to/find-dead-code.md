# Find Dead Code

Identify functions and classes that are defined but never referenced elsewhere in the codebase.

## Quick approach: ast_dead_code macro

```sql
CREATE TABLE codebase AS SELECT * FROM read_ast('src/**/*.py');

SELECT name, definition_type, file_path, start_line
FROM ast_dead_code('codebase')
ORDER BY file_path, start_line;
```

The macro finds definitions whose `name` doesn't appear as an identifier anywhere else in the AST table. It excludes common false positives like `main`, `__init__`, and test fixtures.

## Manual approach: anti-join

For more control over what counts as "used":

```sql
CREATE TABLE codebase AS SELECT * FROM read_ast('src/**/*.py');

-- Definitions with no matching call site
SELECT d.name, d.file_path, d.start_line
FROM codebase d
LEFT JOIN codebase c
    ON c.name = d.name
    AND c.semantic_type = 'COMPUTATION_CALL'
    AND c.node_id != d.node_id
WHERE d.semantic_type = 'DEFINITION_FUNCTION'
  AND d.name IS NOT NULL
  AND c.node_id IS NULL
ORDER BY d.file_path, d.start_line;
```

## Variations

### Include class definitions

```sql
SELECT d.name, d.type, d.file_path
FROM codebase d
LEFT JOIN codebase ref
    ON ref.name = d.name
    AND ref.node_id != d.node_id
    AND ref.semantic_type != d.semantic_type
WHERE d.semantic_type IN ('DEFINITION_FUNCTION', 'DEFINITION_CLASS')
  AND d.name IS NOT NULL
  AND ref.node_id IS NULL;
```

### Exclude test files

```sql
SELECT name, file_path
FROM ast_dead_code('codebase')
WHERE file_path NOT LIKE '%test%'
  AND file_path NOT LIKE '%spec%';
```

### Cross-language dead code

```sql
CREATE TABLE all_code AS
SELECT * FROM read_ast(['src/**/*.py', 'src/**/*.js'], ignore_errors := true);

SELECT name, language, file_path
FROM ast_dead_code('all_code')
ORDER BY language, name;
```

### Using CSS selectors

```sql
-- Functions never called anywhere
SELECT name, file_path
FROM ast_select_from('codebase', '.func:not(:is-called)');
```

## Caveats

- **Dynamic dispatch**: `getattr(obj, name)()` won't show up as a call to `name`
- **Framework entry points**: Flask routes, Django views, pytest fixtures are "called" by the framework, not your code
- **Exports**: A function may be dead within this codebase but used by external consumers
- **Overloads**: Multiple functions with the same name will all appear "used" if any one is called

## See also

- [Analysis Macros](../reference/analysis-macros.md) — `ast_dead_code` reference
- [Call Graph Analysis](../tutorials/call-graphs.md) — finding callers and callees
- [CSS Pseudo-Classes](../reference/css-pseudo-classes.md) — `:is-called` pseudo-class
