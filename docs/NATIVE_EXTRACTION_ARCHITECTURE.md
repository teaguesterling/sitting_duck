# Native Extraction Architecture

This document provides a comprehensive overview of the semantic extraction system used by the sitting_duck DuckDB extension to analyze source code ASTs.

## Overview

The native extraction system transforms raw tree-sitter AST nodes into semantically enriched records suitable for SQL analysis. This happens through a three-layer pipeline:

```
Tree-sitter AST → Semantic Classification → Name Extraction → Native Context
```

Each layer adds progressively richer information, controlled by the `context` parameter in `read_ast()`.

## Architecture Components

### 1. Language Configuration Files (`src/language_configs/*.def`)

Each supported language has a `.def` file that maps tree-sitter node types to semantic metadata using the `DEF_TYPE` macro:

```cpp
DEF_TYPE(raw_type, semantic_type, name_extraction, native_extraction, flags)
```

| Parameter | Description |
|-----------|-------------|
| `raw_type` | The tree-sitter node type string (e.g., `"function_definition"`) |
| `semantic_type` | Universal classification + optional refinement bits |
| `name_extraction` | Strategy for extracting the node's name |
| `native_extraction` | Strategy for rich context extraction |
| `flags` | Behavioral flags (IS_KEYWORD, IS_SYNTAX_ONLY, etc.) |

### 2. Semantic Types (`src/include/semantic_types.hpp`)

Universal 8-bit classification for AST nodes across all languages:

```
Bits 7-2: Base semantic category (e.g., DEFINITION_FUNCTION = 0x04)
Bits 1-0: Refinement within category (e.g., Function::LAMBDA = 0x01)
```

Categories include:
- **DEFINITION_*** - Declarations (functions, classes, variables, modules)
- **COMPUTATION_*** - Operations (calls, access, expressions)
- **LITERAL_*** - Values (strings, numbers, structured data)
- **FLOW_*** - Control flow (conditionals, loops, jumps)
- **OPERATOR_*** - Operators (arithmetic, logical, comparison)
- **ERROR_*** - Exception handling (try, catch, throw)
- **TYPE_*** - Type system (references, generics, primitives)
- **ORGANIZATION_*** - Structure (blocks, lists)
- **PARSER_*** - Syntax tokens (delimiters, punctuation)

### 3. Extraction Strategies (`src/include/node_config.hpp`)

#### Name Extraction (`ExtractionStrategy`)

How to extract the `name` column from a node:

| Strategy | Description | Example Use |
|----------|-------------|-------------|
| `NONE` | No name extraction | Operators, punctuation |
| `NODE_TEXT` | Use node's own text | Identifiers, literals |
| `FIND_IDENTIFIER` | Find child `identifier` node | Function definitions |
| `FIND_CALL_TARGET` | Extract call target name | Function calls |
| `FIND_ASSIGNMENT_TARGET` | Find target in assignment | Lambda expressions |
| `FIND_QUALIFIED_IDENTIFIER` | Extract name from qualified path | Scoped identifiers |
| `FIND_IN_DECLARATOR` | Find in declarator nodes | C/C++ declarations |
| `CUSTOM` | Language-specific logic | Complex patterns |

#### Native Extraction (`NativeExtractionStrategy`)

How to build the `native` column with rich context:

| Strategy | Description | Output Format |
|----------|-------------|---------------|
| `NONE` | No native extraction | NULL |
| `FUNCTION_WITH_PARAMS` | Function signature | `fn_name(p1: T1, p2: T2) -> R` |
| `FUNCTION_WITH_DECORATORS` | Function with annotations | `@decorator fn_name(...)` |
| `ARROW_FUNCTION` | Lambda/arrow function | `(params) => body` |
| `ASYNC_FUNCTION` | Async function | `async fn_name(...)` |
| `CLASS_WITH_INHERITANCE` | Class with bases | `class Name extends Base` |
| `CLASS_WITH_METHODS` | Class with signatures | `class Name { methods... }` |
| `VARIABLE_WITH_TYPE` | Typed variable | `name: Type = value` |
| `GENERIC_FUNCTION` | Generic function | `fn_name<T, U>(...)` |
| `METHOD_DEFINITION` | Method in class | `methodName(...)` |
| `CONSTRUCTOR_DEFINITION` | Constructor | `constructor(...)` |
| `INTERFACE_DEFINITION` | Interface/trait | `interface Name { ... }` |
| `ENUM_DEFINITION` | Enum type | `enum Name { A, B, C }` |
| `IMPORT_STATEMENT` | Import statement | `import { x } from 'y'` |
| `FUNCTION_CALL` | Call with args | `fn(arg1, arg2)` |

## Semantic Refinements

Refinements provide finer-grained classification within each semantic category using the 2 least significant bits:

### Function Refinements
```cpp
namespace Function {
    REGULAR = 0x00;     // Named functions, methods
    LAMBDA = 0x01;      // Anonymous functions, closures
    CONSTRUCTOR = 0x02; // Constructors, initializers
    ASYNC = 0x03;       // Async, generator functions
}
```

### Variable Refinements
```cpp
namespace Variable {
    MUTABLE = 0x00;     // var, let
    IMMUTABLE = 0x01;   // const, final, readonly
    PARAMETER = 0x02;   // Function parameters
    FIELD = 0x03;       // Class/struct fields
}
```

### Loop Refinements
```cpp
namespace Loop {
    COUNTER = 0x00;     // for(i=0; i<n; i++)
    ITERATOR = 0x01;    // for-in, for-of, foreach
    CONDITIONAL = 0x02; // while, until
    INFINITE = 0x03;    // loop, repeat
}
```

### String Refinements
```cpp
namespace String {
    LITERAL = 0x00;     // Basic quoted strings
    TEMPLATE = 0x01;    // Template strings, f-strings
    REGEX = 0x02;       // Regular expressions
    RAW = 0x03;         // Raw strings, here-docs
}
```

See `node_config.hpp` for the complete refinement taxonomy.

## Context Levels

The `context` parameter in `read_ast()` controls extraction depth:

| Level | `semantic_type` | `name` | `native` | Use Case |
|-------|-----------------|--------|----------|----------|
| `'none'` | NULL | NULL | NULL | Raw AST structure only |
| `'node_types_only'` | Populated | NULL | NULL | Semantic filtering |
| `'normalized'` | Populated | Populated | NULL | Name-based queries |
| `'native'` (default) | Populated | Populated | Populated | Full context extraction |

## Language-Specific Considerations

Each language's `.def` file documents language-specific patterns. Key considerations:

### Python
- No distinct `async_function_definition` - uses `function_definition` + `async` keyword child
- Comprehensions have distinct syntax (TRANSFORM_QUERY semantic type)
- Pattern matching (3.10+) uses PATTERN_* semantic types
- Decorators wrap definitions via `decorated_definition` nodes

### JavaScript/TypeScript
- Arrow functions use FIND_ASSIGNMENT_TARGET for naming
- TypeScript adds interfaces, type aliases, enums
- Namespaces/modules mapped to DEFINITION_MODULE

### Rust
- Traits map to DEFINITION_CLASS with ABSTRACT refinement
- Macros use Call::MACRO refinement
- Closures use Function::LAMBDA refinement
- Lifetimes handled in type annotations

### Go
- Goroutines (`go` keyword) use FLOW_SYNC
- Defer statements use FLOW_SYNC
- Multiple return values handled in native extraction
- Interfaces map to DEFINITION_CLASS

### Java
- Constructors get Function::CONSTRUCTOR refinement
- Annotations map to METADATA_ANNOTATION
- Access modifiers (public, private) are METADATA_ANNOTATION
- Lambda expressions use Function::LAMBDA

## Adding a New Language

1. Create `src/language_configs/<language>_types.def`
2. Add language to `src/language_registry.cpp`
3. Add tree-sitter grammar as submodule
4. Create test files in `test/data/<language>/`
5. Create refinements test in `test/sql/languages/<language>_refinements.test`

Use Python's `.def` file as a template - it includes comprehensive Doxygen documentation.

## File Organization

```
src/
├── include/
│   ├── node_config.hpp       # ExtractionStrategy, NativeExtractionStrategy, refinements
│   └── semantic_types.hpp    # Semantic type constants
├── language_configs/
│   ├── python_types.def      # Python node mappings (documented template)
│   ├── javascript_types.def  # JavaScript mappings
│   ├── typescript_types.def  # TypeScript mappings
│   ├── rust_types.def        # Rust mappings
│   ├── go_types.def          # Go mappings
│   ├── java_types.def        # Java mappings
│   └── ...                   # Other languages
└── language_registry.cpp     # Language registration

test/sql/languages/
├── python_refinements.test   # Executable refinement tests
├── rust_refinements.test
├── go_refinements.test
├── java_refinements.test
└── typescript_refinements.test

docs/
├── NATIVE_EXTRACTION_ARCHITECTURE.md  # This document
├── native_extraction_semantics.md     # Field semantics and query patterns
└── api/semantic-types.md              # API reference for semantic types
```

## Query Examples

### Filter by semantic type
```sql
SELECT * FROM read_ast('file.py', context := 'native')
WHERE semantic_type = semantic_type_id('DEFINITION_FUNCTION');
```

### Find all async functions
```sql
SELECT name, native FROM read_ast('file.py', context := 'native')
WHERE semantic_type = semantic_type_id('DEFINITION_FUNCTION')
  AND semantic_type & 0x03 = 3;  -- ASYNC refinement
```

### Count by semantic category
```sql
SELECT semantic_type_to_string(semantic_type) as sem_type, COUNT(*)
FROM read_ast('file.py', context := 'native')
WHERE semantic_type IS NOT NULL
GROUP BY semantic_type
ORDER BY COUNT(*) DESC;
```

### Cross-language function comparison
```sql
SELECT language, COUNT(*) as function_count
FROM read_ast(['*.py', '*.js', '*.go'], context := 'node_types_only')
WHERE semantic_type = semantic_type_id('DEFINITION_FUNCTION')
GROUP BY language;
```

## Related Documentation

- [native_extraction_semantics.md](native_extraction_semantics.md) - Detailed field semantics
- [api/semantic-types.md](api/semantic-types.md) - Complete semantic type reference
- Language `.def` files contain inline Doxygen documentation
