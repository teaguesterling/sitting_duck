# Systems Languages

C, C++, Go, Rust, and Zig support in Sitting Duck.

## Language Nuances

### Extraction Quality Summary

| Language | Functions | Classes | Calls | Variables | Body Detection | Overall |
|----------|-----------|---------|-------|-----------|----------------|---------|
| **C** | ⭐⭐ | ⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Basic |
| **C++** | ⭐⭐⭐ | ⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Good |
| **Go** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ | Very Good |
| **Rust** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Very Good |
| **Zig** | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Good |

### Implementation Notes

- **C++ Method Calls**: Supports both dot notation (`obj.method()`) and arrow notation (`ptr->method()`). The `qualified_name` field contains the full expression.
- **Go Package Calls**: Package-qualified calls like `fmt.Println` have empty `name` but full path in `qualified_name`.
- **Rust Trait Signatures**: Trait method signatures (without bodies) are marked with `IS_DECLARATION_ONLY` via `function_signature_item`.
- **Return Type Extraction**: Go and Rust provide excellent return type extraction including complex types (`*big.Int`, `Result<T, E>`).

### Known Limitations

- **C++ Templates**: Template declarations are parsed but template parameter extraction is limited.
- **C++ Classes**: Class extraction is basic (⭐) - inheritance and virtual methods need more work.
- **C Macros**: Preprocessor macros (`#define`) are captured but macro expansion is not performed.
- **Zig Comptime**: Comptime blocks are parsed but compile-time evaluation semantics are not captured.

### Body Detection

All systems languages have excellent body detection (⭐⭐⭐):
- C/C++: `compound_statement` bodies
- Go: `block` bodies
- Rust: `block` bodies, `function_signature_item` for trait method signatures
- Zig: `block` bodies

---

## C

**Extensions:** `.c`, `.h`
**Identifier:** `'c'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Functions |
| `declaration` | DEFINITION_VARIABLE | Declarations |
| `struct_specifier` | DEFINITION_CLASS | Structs |
| `enum_specifier` | DEFINITION_CLASS | Enums |
| `preproc_include` | EXTERNAL_IMPORT | Includes |
| `preproc_def` | METADATA_DIRECTIVE | Defines |

### Examples

```sql
-- Find all functions
SELECT name, start_line
FROM read_ast('**/*.c')
WHERE type = 'function_definition';

-- Find struct definitions
SELECT name, peek
FROM read_ast('**/*.h')
WHERE type = 'struct_specifier';

-- Find includes
SELECT peek
FROM read_ast('main.c')
WHERE type = 'preproc_include';
```

---

## C++

**Extensions:** `.cpp`, `.hpp`, `.cc`, `.cxx`, `.h`
**Identifier:** `'cpp'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Functions |
| `class_specifier` | DEFINITION_CLASS | Classes |
| `namespace_definition` | DEFINITION_MODULE | Namespaces |
| `template_declaration` | TYPE_GENERIC | Templates |

### Examples

```sql
-- Find classes
SELECT name, start_line, descendant_count as complexity
FROM read_ast('**/*.cpp')
WHERE type = 'class_specifier';

-- Find templates
SELECT peek
FROM read_ast('**/*.hpp')
WHERE type = 'template_declaration';

-- Find namespaces
SELECT name
FROM read_ast('**/*.cpp')
WHERE type = 'namespace_definition';
```

---

## Go

**Extensions:** `.go`
**Identifier:** `'go'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Functions |
| `method_declaration` | DEFINITION_FUNCTION | Methods |
| `type_declaration` | DEFINITION_CLASS | Type defs |
| `struct_type` | DEFINITION_CLASS | Structs |
| `interface_type` | DEFINITION_CLASS | Interfaces |
| `import_declaration` | EXTERNAL_IMPORT | Imports |

### Examples

```sql
-- Find exported functions (uppercase)
SELECT name, file_path
FROM read_ast('**/*.go')
WHERE type = 'function_declaration'
  AND name ~ '^[A-Z]';

-- Find interfaces
SELECT name, peek
FROM read_ast('**/*.go')
WHERE type = 'interface_type';

-- Find method receivers
SELECT name, native
FROM read_ast('**/*.go', context := 'native')
WHERE type = 'method_declaration';
```

---

## Rust

**Extensions:** `.rs`
**Identifier:** `'rust'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_item` | DEFINITION_FUNCTION | Functions |
| `impl_item` | DEFINITION_CLASS | Implementations |
| `struct_item` | DEFINITION_CLASS | Structs |
| `enum_item` | DEFINITION_CLASS | Enums |
| `trait_item` | DEFINITION_CLASS | Traits |
| `use_declaration` | EXTERNAL_IMPORT | Use statements |

### Examples

```sql
-- Find public functions
SELECT name, start_line
FROM read_ast('**/*.rs')
WHERE type = 'function_item'
  AND peek LIKE 'pub %';

-- Find traits
SELECT name
FROM read_ast('**/*.rs')
WHERE type = 'trait_item';

-- Find impl blocks
SELECT peek
FROM read_ast('src/**/*.rs')
WHERE type = 'impl_item';
```

---

## Zig

**Extensions:** `.zig`
**Identifier:** `'zig'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `fn_decl` | DEFINITION_FUNCTION | Functions |
| `var_decl` | DEFINITION_VARIABLE | Variables |
| `struct_declaration` | DEFINITION_CLASS | Structs |
| `enum_declaration` | DEFINITION_CLASS | Enums |

### Examples

```sql
-- Find functions
SELECT name, start_line
FROM read_ast('**/*.zig')
WHERE type = 'fn_decl';

-- Find structs
SELECT name, peek
FROM read_ast('**/*.zig')
WHERE type = 'struct_declaration';

-- Find comptime blocks
SELECT peek
FROM read_ast('**/*.zig')
WHERE peek LIKE '%comptime%';
```

## Cross-Systems Analysis

```sql
-- Compare systems language usage
SELECT
    language,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as types,
    COUNT(*) as total_nodes
FROM read_ast(['**/*.c', '**/*.cpp', '**/*.go', '**/*.rs', '**/*.zig'], ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;
```
