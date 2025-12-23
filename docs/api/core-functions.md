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

### Output Schema

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | BIGINT | Unique node identifier |
| `type` | VARCHAR | AST node type |
| `name` | VARCHAR | Extracted name (if applicable) |
| `file_path` | VARCHAR | Source file path |
| `language` | VARCHAR | Detected language |
| `start_line` | UINTEGER | Starting line (1-based) |
| `start_column` | UINTEGER | Starting column (1-based) |
| `end_line` | UINTEGER | Ending line (1-based) |
| `end_column` | UINTEGER | Ending column (1-based) |
| `parent_id` | BIGINT | Parent node ID (NULL for root) |
| `depth` | UINTEGER | Tree depth (0 for root) |
| `sibling_index` | UINTEGER | Position among siblings |
| `children_count` | UINTEGER | Direct children count |
| `descendant_count` | UINTEGER | Total descendants |
| `peek` | VARCHAR | Source code snippet |
| `semantic_type` | SEMANTIC_TYPE | Semantic category |

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
