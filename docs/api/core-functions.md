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
| `semantic_type` | VARCHAR | Semantic category |

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

## Semantic Type Functions

### `semantic_type_to_string()`

Convert semantic type code to name.

```sql
SELECT semantic_type_to_string(240);  -- 'DEFINITION_FUNCTION'
```

### `get_super_kind()`

Get the super kind for a semantic type.

```sql
SELECT get_super_kind(240);  -- Returns super kind code
```

### `get_kind()`

Get the kind for a semantic type.

```sql
SELECT get_kind(240);  -- Returns kind code
```

### `is_definition()`

Check if semantic type is a definition.

```sql
SELECT * FROM read_ast('file.py')
WHERE is_definition(semantic_type);
```

### `is_call()`

Check if semantic type is a function call.

```sql
SELECT * FROM read_ast('file.py')
WHERE is_call(semantic_type);
```

### `is_control_flow()`

Check if semantic type is control flow.

```sql
SELECT * FROM read_ast('file.py')
WHERE is_control_flow(semantic_type);
```

### `is_identifier()`

Check if semantic type is an identifier.

```sql
SELECT * FROM read_ast('file.py')
WHERE is_identifier(semantic_type);
```

### Specific Type Predicates

Convenience macros for common semantic type checks:

```sql
-- Definition types
is_function_definition(semantic_type)  -- Function definitions
is_class_definition(semantic_type)     -- Class definitions
is_variable_definition(semantic_type)  -- Variable definitions
is_module_definition(semantic_type)    -- Module definitions
is_type_definition(semantic_type)      -- Type definitions

-- Computation types
is_function_call(semantic_type)        -- Function/method calls
is_member_access(semantic_type)        -- Member/property access

-- Literal types
is_string_literal(semantic_type)       -- String literals
is_number_literal(semantic_type)       -- Number literals
is_boolean_literal(semantic_type)      -- Boolean literals
is_literal(semantic_type)              -- Any literal

-- Control flow types
is_conditional(semantic_type)          -- If/switch/match
is_loop(semantic_type)                 -- For/while/do
is_jump(semantic_type)                 -- Return/break/continue

-- Operator types
is_assignment(semantic_type)           -- Assignments
is_comparison(semantic_type)           -- Comparisons
is_arithmetic(semantic_type)           -- Arithmetic operations
is_logical(semantic_type)              -- Logical operations
```

---

## File Utility Functions

### `read_lines()`

Read all lines from a file as rows with line numbers.

```sql
SELECT * FROM read_lines('file.py');
-- Returns: line_number (BIGINT), line (VARCHAR)
```

### `read_lines_range()`

Read a specific line range from a file.

```sql
SELECT * FROM read_lines_range('file.py', 10, 25);
-- Returns lines 10-25
```

### `read_lines_context()`

Read lines around a specific line (context window).

```sql
SELECT * FROM read_lines_context('file.py', 50, 5);
-- Returns lines 45-55 (5 lines before and after line 50)
```

### `get_lines_text()`

Get a line range as a single newline-joined string.

```sql
SELECT get_lines_text('file.py', 10, 25) AS source;
-- Returns lines 10-25 as a single VARCHAR
```

### `get_line()`

Get a single line from a file.

```sql
SELECT get_line('file.py', 42) AS line;
```

### `ast_get_source()`

Extract source code for AST nodes using file_path, start_line, end_line.

```sql
-- Get source for all functions
SELECT
    name,
    ast_get_source(file_path, start_line, end_line) AS source_code
FROM read_ast('file.py')
WHERE is_function_definition(semantic_type);
```

### `ast_get_source_numbered()`

Extract source with line numbers prefixed.

```sql
SELECT ast_get_source_numbered('file.py', 10, 20) AS numbered_source;
-- Returns:
--   10: def my_function():
--   11:     x = 1
--   ...
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

- [Parameters Reference](parameters.md) - Detailed parameter documentation
- [Output Schema](output-schema.md) - Column details
- [Semantic Types](semantic-types.md) - Type system reference
