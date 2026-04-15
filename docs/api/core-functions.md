# Core Functions

Complete reference for Sitting Duck's main functions.

## `read_ast()`

Parse source code files and return AST data as a table.

### Signatures

```sql
-- Single file
read_ast(file_path VARCHAR) -> TABLE

-- Single file with language
read_ast(file_path VARCHAR, language VARCHAR) -> TABLE

-- File array
read_ast(file_patterns LIST(VARCHAR)) -> TABLE

-- File array with language
read_ast(file_patterns LIST(VARCHAR), language VARCHAR) -> TABLE
```

### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `file_path` | VARCHAR | required | File path or glob pattern |
| `file_patterns` | LIST(VARCHAR) | required | Array of file paths or glob patterns |
| `language` | VARCHAR | `'auto'` | Language name or `'auto'` for detection |
| `ignore_errors` | BOOLEAN | `false` | Continue on parse errors |
| `context` | VARCHAR | `'native'` | `'none'`, `'node_types_only'`, `'normalized'`, `'native'` |
| `source` | VARCHAR | `'lines'` | `'none'`, `'path'`, `'lines_only'`, `'lines'`, `'full'` |
| `structure` | VARCHAR | `'full'` | `'none'`, `'minimal'`, `'full'` |
| `peek` | ANY | `'smart'` | `'none'`, `'smart'`, `'full'`, or integer size |
| `peek_size` | INTEGER | `120` | Custom peek size in characters |
| `peek_mode` | VARCHAR | `'smart'` | Peek extraction mode |
| `batch_size` | INTEGER | - | Batch size for streaming |

### Output Schema (19 columns default, 21 with `source := 'full'`)

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | BIGINT | Unique node identifier |
| `type` | VARCHAR | Tree-sitter AST node type |
| `semantic_type` | SEMANTIC_TYPE | Universal semantic category |
| `flags` | UTINYINT | Node property flags |
| `name` | VARCHAR | Extracted identifier name |
| `signature_type` | VARCHAR | Type/return type information |
| `parameters` | STRUCT[] | Function parameters (name and type) |
| `modifiers` | VARCHAR[] | Access modifiers and keywords |
| `annotations` | VARCHAR | Decorator/annotation text |
| `qualified_name` | LIST&lt;STRUCT&gt; | Scope path as segment list of `{semantic_type, name, index}`. Use `ast_qualified_name_as_string()` to render as `C[User] F[__init__]` for display. See [Output Schema](output-schema.md#qualified_name) for full details. |
| `scope` | STRUCT | Scope info bundle: `{current, function, class, module, stack}`. `current` is the nearest enclosing scope's node_id; `function`/`class`/`module` are precomputed shortcuts to the nearest ancestor of that kind (so `a.scope.function` gives "what function is this inside?" as a single field read, no range join needed). `stack` is a typed `LIST<STRUCT<id, kind SEMANTIC_TYPE>>` populated only on scope boundary nodes. See [Scope structure](#scope-structure) below. |
| `file_path` | VARCHAR | Source file path |
| `language` | VARCHAR | Detected language |
| `start_line` | UINTEGER | Starting line (1-based) |
| `end_line` | UINTEGER | Ending line (1-based) |
| `start_column` | UINTEGER | Starting column (**only with `source := 'full'`**) |
| `end_column` | UINTEGER | Ending column (**only with `source := 'full'`**) |
| `parent_id` | BIGINT | Parent node ID (NULL for root) |
| `depth` | UINTEGER | Tree depth (0 for root) |
| `sibling_index` | UINTEGER | Position among siblings |
| `children_count` | UINTEGER | Direct children count |
| `descendant_count` | UINTEGER | Total descendants |
| `peek` | VARCHAR | Source code snippet |

### Scope structure

The `scope` column is a STRUCT with five fields, letting common scope
questions resolve to a single field read instead of a range join:

```
scope: STRUCT<
    current   BIGINT,   -- every node: nearest enclosing scope's node_id
    function  BIGINT,   -- every node: nearest function ancestor's node_id
    class     BIGINT,   -- every node: nearest class/struct/trait ancestor
    module    BIGINT,   -- every node: nearest module/namespace ancestor
    stack     LIST<STRUCT<id BIGINT, kind SEMANTIC_TYPE>>
                        -- scope nodes only: full ancestor chain with kinds
>
```

Common access patterns:

```sql
-- "what function is this inside?"
SELECT name FROM nodes WHERE scope.function = <target_function_node_id>;

-- "all module-level definitions"
SELECT * FROM nodes WHERE scope.current IS NULL OR scope.current <= 0;

-- "all calls inside any try block (kind-filtered scope walk)"
SELECT * FROM nodes c
WHERE c.semantic_type = 'COMPUTATION_CALL'
  AND list_filter(c.scope.stack, s -> s.kind = 'ERROR_TRY') != [];
```

Replaces the pre-v1.7.4 `scope_id` and `scope_stack` columns:
- `scope_id` → `scope.current`
- `scope_stack` → `scope.stack` (now typed; project `.id` for the scalar node_id)

See [Output Schema](output-schema.md) for detailed column documentation and parameter effects.

### Examples

```sql
-- Basic usage
SELECT * FROM read_ast('main.py');

-- With language override
SELECT * FROM read_ast('script.txt', 'python');

-- Multiple files
SELECT * FROM read_ast(['src/**/*.py', 'lib/**/*.js']);

-- With options
SELECT * FROM read_ast(
    'src/**/*.py',
    context := 'native',
    ignore_errors := true,
    peek := 200
);
```

---

## `parse_ast()`

Parse source code from a string.

### Signature

```sql
parse_ast(source_code VARCHAR, language VARCHAR) -> TABLE
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `source_code` | VARCHAR | Yes | Source code to parse |
| `language` | VARCHAR | Yes | Programming language |

### Output Schema

Same as `read_ast()` but with synthetic `file_path`.

### Examples

```sql
-- Parse Python code
SELECT * FROM parse_ast('def hello(): pass', 'python');

-- Parse JavaScript
SELECT type, name FROM parse_ast('function greet() { return "hi"; }', 'javascript');

-- Find specific constructs
SELECT name, start_line
FROM parse_ast('
class MyClass:
    def method1(self):
        pass
    def method2(self):
        pass
', 'python')
WHERE type = 'function_definition';
```

---

## `parse_ast_list()`

Parse source code from a string and return the AST as a `LIST<STRUCT>` — the scalar equivalent of `parse_ast()`. Useful inside CTEs, lateral joins, and scalar expressions where a table function isn't allowed.

### Signature

```sql
parse_ast_list(source_code VARCHAR, language VARCHAR) -> LIST<STRUCT(...)>
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `source_code` | VARCHAR | Yes | Source code to parse |
| `language` | VARCHAR | Yes | Programming language |

Returns `NULL` on invalid input language; other parse errors are raised as `InvalidInputException`.

### Output

A `LIST(STRUCT(...))` whose element struct type matches the default flat schema of `read_ast()` (type, name, semantic_type, qualified_name, start_line, end_line, ...).

### Examples

```sql
-- Count nodes in a snippet
SELECT length(parse_ast_list('def hello(): pass', 'python'));
-- → 6

-- Use inside a CTE / scalar context where parse_ast() table function isn't allowed
WITH ast_nodes AS (
    SELECT unnest(parse_ast_list('x = 1; y = 2', 'python')) AS node
)
SELECT node.type, node.name FROM ast_nodes WHERE node.name IS NOT NULL;

-- Per-row parsing of a column (lateral-friendly)
SELECT file.path, length(parse_ast_list(file.content, 'python')) AS node_count
FROM my_files file;
```

Introduced in v1.7.0 specifically to unblock CTE/lateral-join use cases that can't call table functions. Internally used by `ast_select`'s pattern-matching pseudo-classes (`:match`, `:contains`).

---

## `parse_ast_list_table()`

Table-macro wrapper around `parse_ast_list()`. Presents the same row-shaped interface as `parse_ast()` (so it can be used as a drop-in replacement in `FROM` clauses) while internally calling `parse_ast_list()` so it accepts column-valued arguments.

### Signature

```sql
parse_ast_list_table(source_code, language) -> TABLE
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `source_code` | VARCHAR | Source code to parse. May be a column reference or expression (unlike `parse_ast()`, which requires a literal). |
| `language` | VARCHAR | Programming language |

### Output

Same row shape as `parse_ast()`: `node_id`, `parent_id`, `type`, `name`, `start_line`, `end_line`, `depth`, `sibling_index`, `descendant_count`, `semantic_type`, `flags`, `peek`, and the other flat schema columns.

### Example

```sql
-- Drop-in replacement for parse_ast() in FROM clauses
SELECT node_id, type, name
FROM parse_ast_list_table('.fn#main {}', 'css')
WHERE type = 'class_selector';
```

### Why this exists

`parse_ast()` is a table function and, like all DuckDB table functions, its arguments must be literals at bind time. `parse_ast_list_table()` is the macro-level escape hatch: it wraps the scalar `parse_ast_list()` inside a table-shaped relation, which allows downstream SQL macros to substitute column-valued arguments at macro-expansion time and still produce a row-shaped result.

`ast_select()`'s internal `sel` CTE uses `parse_ast_list_table` rather than `parse_ast` for exactly this reason — it's the plumbing that (together with the upstream DuckDB planner fix tracked in `tracker/bugs/007`) enables per-row selector dispatch for features like `ast_select_rules()`.

---

## `ast_supported_languages()`

List all supported languages.

### Signature

```sql
ast_supported_languages() -> TABLE
```

### Output Schema

| Column | Type | Description |
|--------|------|-------------|
| `language` | VARCHAR | Language identifier |
| `display_name` | VARCHAR | Human-readable name |
| `extensions` | VARCHAR[] | Supported file extensions |

### Example

```sql
SELECT language, extensions
FROM ast_supported_languages()
ORDER BY language;
```

---

## Error Handling

### Invalid File Path

```sql
SELECT * FROM read_ast('nonexistent.py');
-- Error: File not found: nonexistent.py

-- Use ignore_errors to skip
SELECT * FROM read_ast('nonexistent.py', ignore_errors := true);
-- Returns empty result set
```

### Empty Array

```sql
SELECT * FROM read_ast([]);
-- Error: File pattern list cannot be empty
```

### NULL Values

```sql
SELECT * FROM read_ast(['file.py', NULL]);
-- Error: File pattern list cannot contain NULL values
```

### Parse Errors

```sql
-- With ignore_errors, parse errors create ERROR nodes
SELECT * FROM read_ast('broken.py', ignore_errors := true)
WHERE type = 'ERROR';
```

## Next Steps

- [Utility Functions](utility-functions.md) - Predicates, file utilities, and helper functions
- [Parameters Reference](parameters.md) - Detailed parameter documentation
- [Output Schema](output-schema.md) - Column details
- [Semantic Types](semantic-types.md) - Type system reference
