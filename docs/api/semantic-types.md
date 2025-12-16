# Semantic Types Reference

Complete reference for the semantic type system.

## Overview

The semantic type system uses an 8-bit encoding to classify AST nodes into universal categories that work across all 27 supported languages.

## Quick Reference

| Code | Name | Description |
|------|------|-------------|
| 32 | METADATA_COMMENT | Comments and documentation |
| 36 | METADATA_ANNOTATION | Decorators, annotations |
| 48 | EXTERNAL_IMPORT | Import/include statements |
| 52 | EXTERNAL_EXPORT | Export statements |
| 64 | LITERAL_NUMBER | Numeric values |
| 68 | LITERAL_STRING | String values |
| 72 | LITERAL_ATOMIC | Boolean, null |
| 80 | NAME_IDENTIFIER | Simple identifiers |
| 84 | NAME_QUALIFIED | Dotted names |
| 112 | TYPE_PRIMITIVE | Basic types |
| 144 | FLOW_CONDITIONAL | If/switch |
| 148 | FLOW_LOOP | For/while |
| 152 | FLOW_JUMP | Return/break/continue |
| 160 | ERROR_TRY | Try blocks |
| 164 | ERROR_CATCH | Catch blocks |
| 208 | COMPUTATION_CALL | Function calls |
| 212 | COMPUTATION_ACCESS | Member access |
| 220 | COMPUTATION_LAMBDA | Anonymous functions |
| 240 | DEFINITION_FUNCTION | Function definitions |
| 244 | DEFINITION_VARIABLE | Variable definitions |
| 248 | DEFINITION_CLASS | Class definitions |
| 252 | DEFINITION_MODULE | Module definitions |

## Encoding Structure

```
8-bit encoding: [ss kk tt ll]
  ss (bits 6-7): Super Kind (4 categories)
  kk (bits 4-5): Kind (16 subcategories)
  tt (bits 2-3): Super Type (4 per kind)
  ll (bits 0-1): Language-specific
```

## Super Kinds

### META_EXTERNAL (0x00-0x3F)

Metadata, parser constructs, and external references.

| Kind | Code Range | Description |
|------|------------|-------------|
| PARSER_SPECIFIC | 0-15 | Syntax, delimiters |
| RESERVED | 16-31 | Future use |
| METADATA | 32-47 | Comments, annotations |
| EXTERNAL | 48-63 | Imports, exports |

### DATA_STRUCTURE (0x40-0x7F)

Data representation and naming.

| Kind | Code Range | Description |
|------|------------|-------------|
| LITERAL | 64-79 | Values |
| NAME | 80-95 | Identifiers |
| PATTERN | 96-111 | Patterns |
| TYPE | 112-127 | Type info |

### CONTROL_EFFECTS (0x80-0xBF)

Program flow and execution.

| Kind | Code Range | Description |
|------|------------|-------------|
| EXECUTION | 128-143 | Statements |
| FLOW_CONTROL | 144-159 | Conditionals, loops |
| ERROR_HANDLING | 160-175 | Try/catch |
| ORGANIZATION | 176-191 | Blocks, structure |

### COMPUTATION (0xC0-0xFF)

Operations and definitions.

| Kind | Code Range | Description |
|------|------------|-------------|
| OPERATOR | 192-207 | Operators |
| COMPUTATION_NODE | 208-223 | Calls, access |
| TRANSFORM | 224-239 | Queries, iteration |
| DEFINITION | 240-255 | Functions, classes |

## Helper Functions

### `semantic_type_to_string(code)`

Convert code to name:

```sql
SELECT semantic_type_to_string(240);
-- Returns: 'DEFINITION_FUNCTION'
```

### `get_super_kind(code)`

Get super kind:

```sql
SELECT get_super_kind(240);
-- Returns: 3 (COMPUTATION)
```

### `get_kind(code)`

Get kind:

```sql
SELECT get_kind(240);
-- Returns: 15 (DEFINITION)
```

### `is_definition(code)`

Check if definition:

```sql
SELECT is_definition(240);  -- true
SELECT is_definition(208);  -- false
```

### `is_call(code)`

Check if function call:

```sql
SELECT is_call(208);  -- true
SELECT is_call(240);  -- false
```

### `is_control_flow(code)`

Check if control flow:

```sql
SELECT is_control_flow(144);  -- true (CONDITIONAL)
SELECT is_control_flow(148);  -- true (LOOP)
SELECT is_control_flow(152);  -- true (JUMP)
```

### `is_identifier(code)`

Check if identifier:

```sql
SELECT is_identifier(80);   -- true
SELECT is_identifier(84);   -- true
```

## Filtering Patterns

### By Exact Type

```sql
SELECT * FROM read_ast('file.py')
WHERE semantic_type = 240;  -- Functions only
```

### By Super Kind

```sql
-- All COMPUTATION types
SELECT * FROM read_ast('file.py')
WHERE semantic_type >= 192;
```

### By Kind

```sql
-- All DEFINITION types (240-255)
SELECT * FROM read_ast('file.py')
WHERE semantic_type >= 240;
```

### Using Helper Functions

```sql
-- All definitions
SELECT * FROM read_ast('file.py')
WHERE is_definition(semantic_type);

-- All control flow
SELECT * FROM read_ast('file.py')
WHERE is_control_flow(semantic_type);
```

## Semantic Refinements

Some types include refinements for more specific categorization.

### Function Refinements

| Refinement | Description |
|------------|-------------|
| `REGULAR` | Standard function |
| `LAMBDA` | Anonymous function |
| `CONSTRUCTOR` | Class constructor |
| `GETTER` | Property getter |
| `SETTER` | Property setter |
| `ASYNC` | Async function |

### Variable Refinements

| Refinement | Description |
|------------|-------------|
| `MUTABLE` | Mutable variable |
| `IMMUTABLE` | Constant |
| `PARAMETER` | Function parameter |
| `FIELD` | Class field |

### Class Refinements

| Refinement | Description |
|------------|-------------|
| `REGULAR` | Standard class |
| `ABSTRACT` | Abstract class/interface |
| `ENUM` | Enumeration |
| `STRUCT` | Struct type |

### Loop Refinements

| Refinement | Description |
|------------|-------------|
| `ITERATOR` | For/foreach loop |
| `CONDITIONAL` | While loop |
| `INFINITE` | Infinite loop |

### Conditional Refinements

| Refinement | Description |
|------------|-------------|
| `BINARY` | If/else |
| `MULTIWAY` | Switch/match |
| `TERNARY` | Ternary expression |

## Cross-Language Examples

### Functions Across Languages

```sql
-- Python: def, async def
-- JavaScript: function, arrow functions
-- Java: method_declaration
-- Go: function_declaration
-- All have semantic_type = 240

SELECT language, name, type
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
WHERE semantic_type = 240
ORDER BY language, name;
```

### Classes Across Languages

```sql
-- Python: class_definition
-- Java: class_declaration
-- TypeScript: class_declaration
-- C++: class_specifier
-- All have semantic_type = 248

SELECT language, name
FROM read_ast(['**/*.py', '**/*.java', '**/*.cpp'], ignore_errors := true)
WHERE semantic_type = 248;
```

## Next Steps

- [Core Functions](core-functions.md) - Function reference
- [Parameters](parameters.md) - Parameter reference
- [Cross-Language Analysis](../guide/cross-language.md) - Practical examples
