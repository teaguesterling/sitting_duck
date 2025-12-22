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
| `is_assignment(st)` | Assignment ops | Assignment operations |
| `is_comparison(st)` | Comparison ops | Comparison operations |
| `is_arithmetic(st)` | Arithmetic ops | Arithmetic operations |
| `is_logical(st)` | Logical ops | Logical operations |

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

## File Utility Functions

Functions for reading source code lines.

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

## See Also

- [Core Functions](core-functions.md) - Main parsing functions (`read_ast`, `parse_ast`)
- [Semantic Types](semantic-types.md) - Complete semantic type reference
- [Output Schema](output-schema.md) - AST column definitions
