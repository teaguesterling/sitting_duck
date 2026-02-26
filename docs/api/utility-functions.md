# Utility Functions

Helper functions for working with AST data and semantic types.

## Semantic Type Functions

### Conversion Functions

#### `semantic_type_to_string()`

Convert semantic type code to name.

```sql
SELECT semantic_type_to_string(240);  -- 'DEFINITION_FUNCTION'
SELECT semantic_type_to_string(248);  -- 'DEFINITION_CLASS'
SELECT semantic_type_to_string(208);  -- 'COMPUTATION_CALL'
```

#### `semantic_type_code()`

Convert semantic type name to code.

```sql
SELECT semantic_type_code('DEFINITION_FUNCTION');  -- 240
SELECT semantic_type_code('DEFINITION_CLASS');     -- 248
SELECT semantic_type_code('COMPUTATION_CALL');     -- 208
```

### Kind Functions

#### `get_super_kind()`

Get the super kind for a semantic type.

```sql
SELECT get_super_kind(240);  -- 'COMPUTATION'
SELECT get_super_kind(80);   -- 'DATA_STRUCTURE'
```

#### `get_kind()`

Get the kind for a semantic type.

```sql
SELECT get_kind(240);  -- 'DEFINITION'
SELECT get_kind(208);  -- 'COMPUTATION_NODE'
```

---

## Category Predicates

High-level predicates for filtering by semantic category.

### `is_definition()`

Check if semantic type is any definition (function, class, variable, etc.).

```sql
SELECT * FROM read_ast('file.py')
WHERE is_definition(semantic_type);
```

### `is_call()`

Check if semantic type is a function/method call.

```sql
SELECT * FROM read_ast('file.py')
WHERE is_call(semantic_type);
```

### `is_control_flow()`

Check if semantic type is control flow (conditionals, loops, jumps).

```sql
SELECT * FROM read_ast('file.py')
WHERE is_control_flow(semantic_type);
```

### `is_identifier()`

Check if semantic type is an identifier or name.

```sql
SELECT * FROM read_ast('file.py')
WHERE is_identifier(semantic_type);
```

### `is_literal()`

Check if semantic type is any literal value.

```sql
SELECT * FROM read_ast('file.py')
WHERE is_literal(semantic_type);
```

---

## Specific Type Predicates

Fine-grained predicates for filtering by specific semantic types.

### Definition Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_function_definition(st)` | `DEFINITION_FUNCTION` | Function/method definitions |
| `is_class_definition(st)` | `DEFINITION_CLASS` | Class/struct/interface definitions |
| `is_variable_definition(st)` | `DEFINITION_VARIABLE` | Variable declarations |
| `is_module_definition(st)` | `DEFINITION_MODULE` | Module/namespace definitions |
| `is_type_definition(st)` | `DEFINITION_TYPE` | Type alias definitions |

```sql
-- Find all function definitions
SELECT name, start_line FROM read_ast('src/**/*.py')
WHERE is_function_definition(semantic_type);

-- Find all classes
SELECT name, file_path FROM read_ast('src/**/*.java')
WHERE is_class_definition(semantic_type);
```

### Computation Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_function_call(st)` | `COMPUTATION_CALL` | Function/method calls |
| `is_member_access(st)` | `COMPUTATION_ACCESS` | Member/property access |

```sql
-- Find all function calls
SELECT name, qualified_name FROM read_ast('file.py', context := 'native')
WHERE is_function_call(semantic_type);
```

### Literal Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_string_literal(st)` | `LITERAL_STRING` | String literals |
| `is_number_literal(st)` | `LITERAL_NUMBER` | Numeric literals |
| `is_boolean_literal(st)` | `LITERAL_BOOLEAN` | Boolean literals |

```sql
-- Find all string literals
SELECT peek FROM read_ast('config.py')
WHERE is_string_literal(semantic_type);
```

### Control Flow Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_conditional(st)` | `FLOW_CONDITIONAL` | If/switch/match statements |
| `is_loop(st)` | `FLOW_LOOP` | For/while/do loops |
| `is_jump(st)` | `FLOW_JUMP` | Return/break/continue |

```sql
-- Find all loops
SELECT type, start_line FROM read_ast('file.py')
WHERE is_loop(semantic_type);
```

### Operator Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_assignment(st)` | `OPERATOR_ASSIGNMENT` | Assignment operations |
| `is_comparison(st)` | `OPERATOR_COMPARISON` | Comparison operations |
| `is_arithmetic(st)` | `OPERATOR_ARITHMETIC` | Arithmetic operations |
| `is_logical(st)` | `OPERATOR_LOGICAL` | Logical operations |

### External/Import Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_import(st)` | `EXTERNAL_IMPORT` | Import/require/use statements |
| `is_export(st)` | `EXTERNAL_EXPORT` | Export statements |
| `is_foreign(st)` | `EXTERNAL_FOREIGN` | FFI declarations |

```sql
-- Find all import statements
SELECT type, name FROM read_ast('file.py')
WHERE is_import(semantic_type);

-- Find all exports in a JavaScript module
SELECT name FROM read_ast('module.js')
WHERE is_export(semantic_type);
```

### Metadata Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_comment(st)` | `METADATA_COMMENT` | Comments |
| `is_annotation(st)` | `METADATA_ANNOTATION` | Decorators/annotations |
| `is_directive(st)` | `METADATA_DIRECTIVE` | Preprocessor directives |

```sql
-- Find all comments
SELECT peek FROM read_ast('file.py')
WHERE is_comment(semantic_type);

-- Find all Python decorators
SELECT name FROM read_ast('file.py')
WHERE is_annotation(semantic_type);
```

### Organization Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_block(st)` | `ORGANIZATION_BLOCK` | Block/scope structures |
| `is_list(st)` | `ORGANIZATION_LIST` | List/array/container structures |

### Type Predicates

| Function | Matches | Description |
|----------|---------|-------------|
| `is_type_primitive(st)` | `TYPE_PRIMITIVE` | Primitive types (int, string, bool) |
| `is_type_composite(st)` | `TYPE_COMPOSITE` | Composite types (struct, union, tuple) |
| `is_type_reference(st)` | `TYPE_REFERENCE` | Reference/pointer types |
| `is_type_generic(st)` | `TYPE_GENERIC` | Generic/template types |

```sql
-- Find all type annotations in TypeScript
SELECT type, name FROM read_ast('file.ts')
WHERE is_type_primitive(semantic_type)
   OR is_type_generic(semantic_type);
```

---

## Flag Predicates

Predicates for checking AST node flags.

### `is_declaration_only()`

Check if a definition is declaration-only (no body).

```sql
-- Find abstract methods (no implementation)
SELECT name FROM read_ast('Interface.java')
WHERE is_function_definition(semantic_type)
  AND is_declaration_only(flags);
```

### `is_syntax_only()`

Check if a node is pure syntax (keyword, punctuation).

```sql
-- Filter out syntax-only nodes
SELECT * FROM read_ast('file.py')
WHERE NOT is_syntax_only(flags);
```

### `has_body()` / `is_embodied()`

Check if a definition has a body (implementation).

```sql
-- Find implemented functions only
SELECT name FROM read_ast('file.py')
WHERE is_function_definition(semantic_type)
  AND has_body(flags);
```

### `is_construct()`

Check if a node is a semantic construct (not pure syntax).

```sql
SELECT * FROM read_ast('file.py')
WHERE is_construct(flags);
```

---

## Source Extraction Functions

Scalar functions for extracting source code from files. These are SQL macros that wrap DuckDB's `read_text()` function.

> **For full-featured line reading** (globs, line specs, context windows, lateral joins), use the [`duckdb_read_lines`](https://github.com/teaguesterling/duckdb_read_lines) extension.

#### `ast_get_source(file_path, start_line, end_line)`

Extract source code for an AST node as a single newline-joined string.

```sql
-- Get source for all functions
SELECT
    name,
    ast_get_source(file_path, start_line, end_line) AS source_code
FROM read_ast('file.py')
WHERE is_function_definition(semantic_type);

-- Get a specific line range
SELECT ast_get_source('file.py', 10, 25) AS source;
```

#### `ast_get_source_numbered(file_path, start_line, end_line)`

Extract source with line numbers prefixed (useful for display).

```sql
SELECT ast_get_source_numbered('file.py', 10, 13) AS numbered_source;
-- Returns:
--   10: def my_function():
--   11:     x = 1
--   12:     y = 2
--   13:     return x + y
```

#### `ast_get_source_line(file_path, line_num)`

Get a single line from a file.

```sql
SELECT ast_get_source_line('file.py', 42) AS line;
-- Returns the content of line 42
```

---

## String Utility Functions

Functions for efficient string pattern matching.

### `string_contains_any(str, patterns)`

Check if a string contains any of the patterns in a list (case-sensitive).

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | VARCHAR | The string to search in |
| `patterns` | VARCHAR[] | List of patterns to search for |

**Returns:** BOOLEAN - `true` if any pattern is found, `false` otherwise

```sql
-- Basic usage
SELECT string_contains_any('hello world', ['world']);  -- true
SELECT string_contains_any('hello world', ['foo']);    -- false

-- Multiple patterns
SELECT string_contains_any('hello world', ['foo', 'bar', 'world']);  -- true

-- Use with AST queries for security scanning
SELECT name FROM read_ast('file.py')
WHERE type = 'call'
  AND string_contains_any(peek, ['eval', 'exec', 'system']);
```

### `string_contains_any_i(str, patterns)`

Case-insensitive version of `string_contains_any`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `str` | VARCHAR | The string to search in |
| `patterns` | VARCHAR[] | List of patterns to search for |

**Returns:** BOOLEAN

```sql
SELECT string_contains_any_i('Hello World', ['hello']);  -- true
SELECT string_contains_any_i('HELLO WORLD', ['world']);  -- true
```

### `ast_peek_contains_any(peek, patterns)`

Alias for `string_contains_any`, named for use in AST queries.

```sql
-- Find dangerous function calls
SELECT name, peek FROM read_ast('src/**/*.py')
WHERE type = 'call'
  AND ast_peek_contains_any(peek, ['eval', 'exec', 'system', 'subprocess']);
```

### Comparison: Before and After

These functions replace verbose OR chains:

```sql
-- Before (verbose):
WHERE peek LIKE '%eval%'
   OR peek LIKE '%exec%'
   OR peek LIKE '%system%'
   OR peek LIKE '%subprocess%'

-- After (concise):
WHERE string_contains_any(peek, ['eval', 'exec', 'system', 'subprocess'])
```

### NULL Handling

- NULL string returns NULL
- NULL patterns list returns NULL
- NULL patterns within the list are skipped

```sql
SELECT string_contains_any(NULL, ['foo']);              -- NULL
SELECT string_contains_any('hello', NULL);              -- NULL
SELECT string_contains_any('hello', ['foo', NULL, 'hello']);  -- true (NULL skipped)
```

---

## See Also

- [Structural Analysis Macros](structural-analysis.md) - Tree navigation and code analysis macros
- [Core Functions](core-functions.md) - Main parsing functions (`read_ast`, `parse_ast`)
- [Semantic Types](semantic-types.md) - Complete semantic type reference
- [Output Schema](output-schema.md) - AST column definitions
