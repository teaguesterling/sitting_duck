# Sitting Duck API Reference

**Complete reference for the Sitting Duck DuckDB extension - accurate documentation for the current implementation.**

## Table of Contents

1. [Core Table Functions](#core-table-functions)
2. [Scalar Functions](#scalar-functions)
3. [Utility Functions](#utility-functions)
4. [Semantic Type System](#semantic-type-system)
5. [Language Support](#language-support)
6. [Common Usage Patterns](#common-usage-patterns)

---

## Core Table Functions

### `read_ast(file_patterns, [language], [options...])`

**Main parsing function** - Reads source code files and returns a flattened AST table.

**Parameters:**
- `file_patterns` (VARCHAR | LIST(VARCHAR)): File path(s) or glob pattern(s)
  - Single file: `'script.py'`
  - Single pattern: `'src/**/*.py'`
  - **Array of patterns (NEW!)**: `['src/**/*.py', 'lib/**/*.js', 'main.cpp']`
  - Cross-language: `'**/*.{py,js,cpp}'` or `['**/*.py', '**/*.js', '**/*.cpp']`
- `language` (VARCHAR, optional): Language override (auto-detected from extension if omitted)
- `ignore_errors` (BOOLEAN, optional): Continue processing when encountering syntax errors (default: false)
- `peek_size` (INTEGER, optional): Number of characters to include in peek field (default: 120)
- `peek_mode` (VARCHAR, optional): How to extract peek text - 'auto', 'chars', 'lines' (default: 'auto')

**Returns:** Table with complete AST node data

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | BIGINT | Unique node identifier |
| `type` | VARCHAR | Language-specific AST node type |
| `name` | VARCHAR | Node name/identifier (NULL if not applicable) |
| `file_path` | VARCHAR | Source file path |
| `language` | VARCHAR | Detected programming language |
| `start_line` | UINTEGER | Starting line number (1-based) |
| `start_column` | UINTEGER | Starting column (1-based) |
| `end_line` | UINTEGER | Ending line number (1-based) |
| `end_column` | UINTEGER | Ending column (1-based) |
| `parent_id` | BIGINT | Parent node ID (NULL for root) |
| `depth` | UINTEGER | Tree depth (0 for root) |
| `sibling_index` | UINTEGER | Position among siblings (0-based) |
| `children_count` | UINTEGER | Number of direct children |
| `descendant_count` | UINTEGER | Total descendants (complexity metric) |
| `peek` | VARCHAR | Source code snippet for this node |
| `semantic_type` | UTINYINT | Universal semantic category (0-255) |
| `flags` | UTINYINT | Universal semantic flags (IS_CONSTRUCT, IS_EMBODIED) |
| `arity_bin` | UTINYINT | Binned arity for analysis |

**Examples:**
```sql
-- Parse single file with auto-detection
SELECT * FROM read_ast('main.py');

-- Parse multiple files with glob pattern
SELECT file_path, COUNT(*) as nodes
FROM read_ast('src/**/*.py', ignore_errors := true)
GROUP BY file_path;

-- Parse multiple patterns using DuckDB-style arrays (NEW!)
SELECT file_path, language, COUNT(*) as nodes
FROM read_ast([
    'src/**/*.py',    -- All Python files in src/
    'lib/**/*.js',    -- All JavaScript files in lib/
    'main.cpp',       -- Specific C++ file
    'tests/**/*.ts'   -- All TypeScript test files
], ignore_errors := true)
GROUP BY file_path, language
ORDER BY nodes DESC;

-- Mixed files and patterns with language override
SELECT file_path, name, start_line
FROM read_ast(['**/*.py', '**/*.js'], 'auto', ignore_errors := true)
WHERE semantic_type = 240; -- DEFINITION_FUNCTION

-- Array with explicit language (applies to all files)
SELECT * FROM read_ast(['script1.py', 'script2.py'], 'python');

-- Find cross-language patterns using arrays
SELECT language, COUNT(*) as import_count
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
WHERE semantic_type = 48; -- EXTERNAL_IMPORT
GROUP BY language;
```

### `parse_ast(source_code, language)`

**String parsing function** - Parses source code string directly.

**Parameters:**
- `source_code` (VARCHAR): Source code to parse
- `language` (VARCHAR): Programming language (required)

**Returns:** Same schema as `read_ast()` but with synthetic file_path

**Example:**
```sql
SELECT node_id, type, name, semantic_type
FROM parse_ast('def hello(): return "world"', 'python')
WHERE semantic_type = 240; -- Functions only
```

### DuckDB-Consistent Array Interface

**Pattern arrays follow DuckDB conventions** (like `read_csv`, `read_parquet`) for maximum consistency.

#### Function Overloads
```sql
-- Single pattern (VARCHAR)
SELECT * FROM read_ast('main.py');
SELECT * FROM read_ast('main.py', 'python');

-- Pattern array (LIST(VARCHAR)) - NEW!
SELECT * FROM read_ast(['src/**/*.py', 'lib/**/*.js']);
SELECT * FROM read_ast(['**/*.py'], 'python');
```

#### Array Features
- **File deduplication**: Multiple patterns matching the same file return it only once
- **Sorted output**: Results are consistently ordered by file path (DuckDB convention)
- **Error handling**: Use `ignore_errors := true` for robust multi-pattern processing
- **Parameter validation**: Clear error messages for invalid inputs

#### Array Error Handling
```sql
-- Empty array error
SELECT * FROM read_ast([]);
-- Error: File pattern list cannot be empty

-- NULL values error  
SELECT * FROM read_ast(['file.py', NULL]);
-- Error: File pattern list cannot contain NULL values

-- Wrong type error
SELECT * FROM read_ast(123);
-- Error: File patterns must be VARCHAR or LIST(VARCHAR)

-- Handle mixed valid/invalid patterns gracefully
SELECT COUNT(*) FROM read_ast([
    'valid_file.py',
    'nonexistent_file.py'
], ignore_errors := true);
-- Processes valid files, skips invalid ones
```

---


## Utility Functions

### `ast_supported_languages()`

**Language metadata** - Returns information about supported languages.

**Returns:** Table with:
- `language` (VARCHAR): Language identifier
- `display_name` (VARCHAR): Human-readable name
- `extensions` (VARCHAR[]): Supported file extensions

**Example:**
```sql
SELECT language, display_name, extensions
FROM ast_supported_languages()
ORDER BY language;
```

---

## Semantic Type System

### Overview

The extension uses an **8-bit semantic taxonomy** for cross-language analysis. Each node has a `semantic_type` field (0-255) that provides universal semantic meaning regardless of the source language.

### Semantic Type Functions

#### `semantic_type_to_string(type_code)`
Converts semantic type code to human-readable name.

```sql
SELECT semantic_type_to_string(115);  -- Returns: 'DEFINITION_FUNCTION'
```

#### `get_super_kind(type_code)`
Returns the super kind (top-level category) for a semantic type.

```sql
SELECT get_super_kind(115);  -- Returns super kind for functions
```

#### `get_kind(type_code)`
Returns the kind (subcategory) for a semantic type.

```sql
SELECT get_kind(115);  -- Returns kind for functions
```

#### `is_definition(type_code)`
Checks if a semantic type represents a definition.

```sql
SELECT name FROM read_ast('main.py')
WHERE is_definition(semantic_type) AND name IS NOT NULL;
```

#### `is_call(type_code)`
Checks if a semantic type represents a function/method call.

```sql
SELECT COUNT(*) as call_count
FROM read_ast('script.js')
WHERE is_call(semantic_type);
```

#### `is_control_flow(type_code)`
Checks if a semantic type represents control flow.

```sql
SELECT type, COUNT(*) as control_flow_count
FROM read_ast('**/*.py', ignore_errors := true)
WHERE is_control_flow(semantic_type)
GROUP BY type;
```

#### `is_identifier(type_code)`
Checks if a semantic type represents an identifier.

```sql
SELECT DISTINCT name
FROM read_ast('main.py')
WHERE is_identifier(semantic_type) AND name IS NOT NULL;
```

### Complete Semantic Type Hierarchy

The 8-bit encoding follows this pattern: `[ss kk tt ll]`
- **ss (bits 6-7)**: Super Kind (4 major categories)
- **kk (bits 4-5)**: Kind (16 subcategories)  
- **tt (bits 2-3)**: Super Type (4 variants per kind)
- **ll (bits 0-1)**: Language-specific (reserved for future use)

#### Super Kinds and Kinds

| Super Kind | Code | Kinds | Description |
|------------|------|-------|-------------|
| **META_EXTERNAL** | 0x00-0x3F | PARSER_SPECIFIC, RESERVED, METADATA, EXTERNAL | Meta-information and language specifics |
| **DATA_STRUCTURE** | 0x40-0x7F | LITERAL, NAME, PATTERN, TYPE | Data representation and identification |
| **CONTROL_EFFECTS** | 0x80-0xBF | EXECUTION, FLOW_CONTROL, ERROR_HANDLING, ORGANIZATION | Program flow and effects |
| **COMPUTATION** | 0xC0-0xFF | OPERATOR, COMPUTATION_NODE, TRANSFORM, DEFINITION | Operations and definitions |

#### Detailed Type Reference

**META_EXTERNAL (0x00-0x3F)**

| Kind | Super Type | Code | Examples | Python | JavaScript | Java | C++ |
|------|------------|------|----------|--------|------------|------|-----|
| **PARSER_SPECIFIC** | CONSTRUCT | 0 | Unique constructs | `yield`, `with` | `yield*`, destructuring | annotations | templates, RAII |
| | DELIMITER | 4 | Language delimiters | `(`, `)`, `[`, `]` | `(`, `)`, `[`, `]` | `(`, `)`, `[`, `]` | `(`, `)`, `[`, `]` |
| | PUNCTUATION | 8 | Language punctuation | `:`, `,`, `;` | `:`, `,`, `;` | `:`, `,`, `;` | `:`, `,`, `;` |
| | SYNTAX | 12 | Syntax elements | indentation, newlines | braces, semicolons | braces, semicolons | braces, semicolons |
| **RESERVED** | FUTURE1 | 16 | Reserved for future | - | - | - | - |
| | FUTURE2 | 20 | Reserved for future | - | - | - | - |
| | FUTURE3 | 24 | Reserved for future | - | - | - | - |
| | FUTURE4 | 28 | Reserved for future | - | - | - | - |
| **METADATA** | COMMENT | 32 | Documentation | `# comment`, `"""docstring"""` | `// comment`, `/* comment */` | `// comment`, `/** javadoc */` | `// comment`, `/* comment */` |
| | ANNOTATION | 36 | Decorators/attributes | `@decorator` | `@decorator` | `@Annotation` | `[[attribute]]` |
| | DIRECTIVE | 40 | Preprocessor | `# type: ignore` | `// @ts-ignore` | N/A | `#include`, `#define` |
| | DEBUG | 44 | Debug information | breakpoints, traces | debugger statements | debug annotations | debug macros |
| **EXTERNAL** | IMPORT | 48 | Import statements | `import os`, `from x import y` | `import fs from 'fs'` | `import java.util.*` | `#include <iostream>` |
| | EXPORT | 52 | Export statements | N/A (implicit) | `export default`, `export {x}` | `public class` | `extern` |
| | FOREIGN | 56 | Foreign interfaces | `ctypes`, `cffi` | `WebAssembly` | `native` methods | `extern "C"` |
| | EMBED | 60 | Embedded code | SQL in strings | template literals | annotations | inline assembly |

**DATA_STRUCTURE (0x40-0x7F)**

| Kind | Super Type | Code | Examples | Python | JavaScript | Java | C++ |
|------|------------|------|----------|--------|------------|------|-----|
| **LITERAL** | NUMBER | 64 | Numeric values | `42`, `3.14` | `42`, `3.14` | `42`, `3.14f` | `42`, `3.14` |
| | STRING | 68 | String/text values | `"hello"`, `'world'` | `"hello"`, `'world'` | `"hello"` | `"hello"` |
| | ATOMIC | 72 | Boolean/null values | `True`, `False`, `None` | `true`, `false`, `null` | `true`, `false`, `null` | `true`, `false`, `nullptr` |
| | STRUCTURED | 76 | Collection literals | `[1,2,3]`, `{'a':1}` | `[1,2,3]`, `{a:1}` | `{1,2,3}` | `{1,2,3}` |
| **NAME** | IDENTIFIER | 80 | Simple names | `variable`, `func` | `variable`, `func` | `variable`, `method` | `variable`, `function` |
| | QUALIFIED | 84 | Dotted names | `obj.method` | `obj.method` | `obj.method` | `obj.method` |
| | SCOPED | 88 | Scope references | `self`, `@instance_var` | `this`, `super` | `this`, `super` | `this`, `::global` |
| | ATTRIBUTE | 92 | Decorative metadata | `@decorator` | `@decorator` | `@Override` | `[[attribute]]` |
| **PATTERN** | DESTRUCTURE | 96 | Breaking apart | `[a, b] = tuple`, `{x, y} = obj` | `{a, b} = obj`, `[x, ...rest]` | structured bindings | `auto [x, y] = pair` |
| | COLLECT | 100 | Gathering together | `*args`, `first, *rest = list` | `...args`, `[...array]` | varargs `Type...` | variadic templates |
| | TEMPLATE | 104 | Parameterized patterns | f-strings `f"{name}"` | template literals | generics `List<T>` | template parameters |
| | MATCH | 108 | Structured entities | regex `r'\d+'`, match patterns | regex `/\d+/`, destructure patterns | selector patterns | template specialization |
| **TYPE** | PRIMITIVE | 112 | Basic types | `int`, `str`, `bool` | `number`, `string` | `int`, `String` | `int`, `char` |
| | COMPOSITE | 116 | Complex types | `List[int]`, tuples | arrays, objects | `List<T>`, arrays | `std::vector<T>` |
| | REFERENCE | 120 | Type references | type hints | type annotations | class references | pointer types |
| | GENERIC | 124 | Generic types | `TypeVar`, `Generic` | generic interfaces | `<T>` parameters | template types |

**CONTROL_EFFECTS (0x80-0xBF)**

| Kind | Super Type | Code | Examples | Python | JavaScript | Java | C++ |
|------|------------|------|----------|--------|------------|------|-----|
| **EXECUTION** | STATEMENT | 128 | Expression statements | Expression as statement | Expression as statement | Expression as statement | Expression as statement |
| | DECLARATION | 132 | Declarations | `global`, `nonlocal` | `var`, `let`, `const` | field declarations | variable declarations |
| | INVOCATION | 136 | Function calls (effects) | `print()`, `obj.mutate()` | `console.log()`, `obj.mutate()` | `System.out.println()` | `std::cout << x` |
| | MUTATION | 140 | State changes | assignments, mutations | assignments, mutations | field updates | pointer operations |
| **FLOW_CONTROL** | CONDITIONAL | 144 | Conditional flow | `if`, `elif`, `else` | `if`, `else if`, `else` | `if`, `else if`, `switch` | `if`, `else if`, `switch` |
| | LOOP | 148 | Iteration | `for`, `while` | `for`, `while` | `for`, `while` | `for`, `while` |
| | JUMP | 152 | Control jumps | `break`, `continue`, `return` | `break`, `continue`, `return` | `break`, `continue`, `return` | `break`, `continue`, `return`, `goto` |
| | SYNC | 156 | Concurrency control | `async`, `await` | `async`, `await` | `synchronized` | `std::async` |
| **ERROR_HANDLING** | TRY | 160 | Try blocks | `try:` | `try {` | `try {` | `try {` |
| | CATCH | 164 | Exception handling | `except Exception:` | `catch (e) {` | `catch (Exception e) {` | `catch (std::exception& e) {` |
| | THROW | 168 | Exception throwing | `raise Exception()` | `throw new Error()` | `throw new Exception()` | `throw std::runtime_error()` |
| | FINALLY | 172 | Cleanup blocks | `finally:` | `finally {` | `finally {` | RAII destructors |
| **ORGANIZATION** | BLOCK | 176 | Code blocks | `if: block` | `{ block }` | `{ block }` | `{ block }` |
| | LIST | 180 | Parameter/argument lists | `(a, b, c)` | `(a, b, c)` | `(int a, int b)` | `(int a, int b)` |
| | SECTION | 184 | Code sections | modules, classes | modules, namespaces | packages, classes | namespaces, classes |
| | CONTAINER | 188 | Containers | comprehensions | array literals | collections | containers |

**COMPUTATION (0xC0-0xFF)**

| Kind | Super Type | Code | Examples | Python | JavaScript | Java | C++ |
|------|------------|------|----------|--------|------------|------|-----|
| **OPERATOR** | ARITHMETIC | 192 | Math operators | `+`, `-`, `*`, `**` | `+`, `-`, `*`, `**` | `+`, `-`, `*` | `+`, `-`, `*` |
| | LOGICAL | 196 | Logic operators | `and`, `or`, `not` | `&&`, `\|\|`, `!` | `&&`, `\|\|`, `!` | `&&`, `\|\|`, `!` |
| | COMPARISON | 200 | Comparison ops | `==`, `!=`, `in` | `===`, `!==`, `in` | `==`, `!=` | `==`, `!=` |
| | ASSIGNMENT | 204 | Assignment ops | `=`, `+=`, `:=` | `=`, `+=` | `=`, `+=` | `=`, `+=` |
| **COMPUTATION_NODE** | CALL | 208 | Function calls | `func()`, `obj.method()` | `func()`, `obj.method()` | `func()`, `obj.method()` | `func()`, `obj->method()` |
| | ACCESS | 212 | Member access | `obj.attr`, `arr[0]` | `obj.attr`, `arr[0]` | `obj.field`, `arr[0]` | `obj.member`, `arr[0]` |
| | EXPRESSION | 216 | Complex expressions | `a + b * c` | `a + b * c` | `a + b * c` | `a + b * c` |
| | LAMBDA | 220 | Anonymous functions | `lambda x: x+1` | `x => x+1` | `x -> x+1` | `[](int x){return x+1;}` |
| **TRANSFORM** | QUERY | 224 | Data queries | comprehensions | filter/map chains | stream operations | ranges/algorithms |
| | ITERATION | 228 | Iteration transforms | `map`, `filter` | `map`, `filter` | stream methods | STL algorithms |
| | PROJECTION | 232 | Data projection | dict comprehensions | object destructuring | field selection | member access |
| | AGGREGATION | 236 | Data aggregation | `sum`, `reduce` | `reduce`, `aggregate` | collectors | accumulate |
| **DEFINITION** | FUNCTION | 240 | Function definitions | `def func():` | `function func(){}` | `void func(){}` | `void func(){}` |
| | VARIABLE | 244 | Variable definitions | `x = 5` | `let x = 5` | `int x = 5` | `int x = 5` |
| | CLASS | 248 | Class definitions | `class MyClass:` | `class MyClass{}` | `class MyClass{}` | `class MyClass{}` |
| | MODULE | 252 | Module definitions | `import sys` | `import fs from 'fs'` | `package com.example` | `namespace example` |

#### Quick Reference - Most Common Types

| Code | Type | Description | Use Case |
|------|------|-------------|----------|
| 240 | DEFINITION_FUNCTION | Function definitions across all languages | API extraction, complexity analysis |
| 248 | DEFINITION_CLASS | Class/struct/interface definitions | Architecture analysis, OOP patterns |
| 208 | COMPUTATION_CALL | Function/method calls | Dependency analysis, call graphs |
| 144 | FLOW_CONDITIONAL | If/switch/match statements | Complexity metrics, control flow |
| 148 | FLOW_LOOP | For/while/do-while loops | Complexity metrics, performance analysis |
| 80 | NAME_IDENTIFIER | Simple identifiers and names | Variable/function name analysis, symbol extraction |
| 32 | METADATA_COMMENT | Comments and documentation | Documentation coverage, code quality |
| 48 | EXTERNAL_IMPORT | Import/include statements | Dependency mapping, module analysis |

### Universal Flags

In addition to semantic types, each node has a `flags` field that captures **orthogonal properties** - characteristics that can apply across multiple semantic types and languages.

#### Flag Values

| Flag | Bit | Value | Description | Examples |
|------|-----|-------|-------------|----------|
| **IS_CONSTRUCT** | 0 | 0x01 | Semantic language construct (not just punctuation/token) | `def`, `class`, `if`, keywords, comments |
| **IS_EMBODIED** | 1 | 0x02 | Has body/implementation (definition vs declaration) | `function_definition` vs `function_declarator` |
| **RESERVED** | 2-7 | 0x04-0x80 | Reserved for future use | (Available for new orthogonal properties) |

#### Flag Helper Functions

```sql
-- is_construct(flags) -> BOOLEAN
-- Returns true if node is a semantic language construct
SELECT is_construct(flags) FROM read_ast('file.py');

-- is_embodied(flags) -> BOOLEAN
-- Returns true if node has a body/implementation
SELECT is_embodied(flags) FROM read_ast('file.cpp');

-- has_body(flags) -> BOOLEAN
-- Alias for is_embodied - more intuitive name
SELECT has_body(flags) FROM read_ast('file.cpp');
```

#### Flag Usage Examples

```sql
-- Find all semantic constructs (not punctuation/tokens)
SELECT DISTINCT type, peek, language
FROM read_ast('**/*.*', ignore_errors := true)
WHERE is_construct(flags)
ORDER BY language, type;

-- Distinguish function definitions from declarations
SELECT
    type,
    name,
    CASE WHEN is_embodied(flags) THEN 'Definition (has body)'
         ELSE 'Declaration (no body)' END as node_kind
FROM read_ast('**/*.cpp', ignore_errors := true)
WHERE semantic_type = 240  -- DEFINITION_FUNCTION
  AND name IS NOT NULL
ORDER BY file_path, start_line;

-- Count definitions vs declarations
SELECT
    CASE WHEN has_body(flags) THEN 'definition' ELSE 'declaration' END as kind,
    COUNT(*) as count
FROM read_ast('src/**/*.cpp', ignore_errors := true)
WHERE type IN ('function_definition', 'function_declarator')
GROUP BY kind;
```

#### Combining Semantic Types and Flags

```sql
-- Find only function definitions (with implementation), not declarations
SELECT file_path, name, start_line
FROM read_ast('**/*.cpp', ignore_errors := true)
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND has_body(flags)               -- Has implementation body
  AND name IS NOT NULL
ORDER BY file_path, start_line;

-- Find forward declarations only (no body)
SELECT file_path, name, type
FROM read_ast('**/*.{h,hpp}', ignore_errors := true)
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND NOT has_body(flags)           -- Declaration only
  AND name IS NOT NULL;

-- Analyze definitions vs declarations by file type
SELECT
    CASE WHEN file_path LIKE '%.h' OR file_path LIKE '%.hpp' THEN 'header' ELSE 'source' END as file_type,
    CASE WHEN is_embodied(flags) THEN 'definition' ELSE 'declaration' END as node_kind,
    COUNT(*) as count
FROM read_ast('**/*.{cpp,hpp,h}', ignore_errors := true)
WHERE semantic_type = 'DEFINITION_FUNCTION'
GROUP BY file_type, node_kind
ORDER BY file_type, node_kind;
```

#### Using Semantic Type Convenience Functions

```sql
-- Convert semantic type codes to readable names
SELECT 
    semantic_type,
    semantic_type_to_string(semantic_type) as type_name,
    get_super_kind(semantic_type) as super_kind,
    get_kind(semantic_type) as kind
FROM read_ast('main.py') 
LIMIT 5;

-- Find all function definitions using semantic predicates
SELECT file_path, name, start_line
FROM read_ast('**/*.py')
WHERE is_definition(semantic_type) AND is_call(semantic_type) = false;

-- Group by semantic categories
SELECT 
    get_super_kind(semantic_type) as category,
    semantic_type_to_string(semantic_type) as type_name,
    COUNT(*) as count
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY semantic_type
ORDER BY count DESC;

-- Find control flow complexity
SELECT 
    file_path,
    COUNT(*) as control_flow_nodes
FROM read_ast('**/*.py')
WHERE is_control_flow(semantic_type)
GROUP BY file_path
ORDER BY control_flow_nodes DESC;
```

---

## Language Support

Currently supports **27 programming languages** with full semantic analysis:

| Category | Languages | Extensions |
|----------|-----------|------------|
| **Web** | JavaScript | `.js`, `.jsx` |
| | TypeScript | `.ts`, `.tsx` |
| | HTML | `.html`, `.htm` |
| | CSS | `.css` |
| **Systems** | C | `.c`, `.h` |
| | C++ | `.cpp`, `.hpp`, `.cc`, `.cxx` |
| | Go | `.go` |
| | Rust | `.rs` |
| | Zig | `.zig` |
| **Scripting** | Python | `.py` |
| | Ruby | `.rb` |
| | PHP | `.php` |
| | Lua | `.lua` |
| | R | `.r`, `.R` |
| | Bash | `.sh`, `.bash` |
| **Enterprise** | Java | `.java` |
| | C# | `.cs` |
| | Kotlin | `.kt`, `.kts` |
| | Swift | `.swift` |
| **Mobile** | Dart | `.dart` |
| **Infrastructure** | HCL (Terraform) | `.hcl`, `.tf`, `.tfvars` |
| | JSON | `.json` |
| | TOML | `.toml` |
| | GraphQL | `.graphql`, `.gql` |
| **Documentation** | SQL | `.sql` |
| | Markdown | `.md`, `.markdown` |

### Language Detection

The extension automatically detects languages based on file extensions. For files without extensions or non-standard extensions, you can specify the language explicitly:

```sql
-- Auto-detection
SELECT * FROM read_ast('script.py');  -- Detects Python

-- Explicit language
SELECT * FROM read_ast('Makefile', 'bash');  -- Force specific language
```

---

## Common Usage Patterns

### Basic Code Analysis

```sql
-- Get overview of a codebase using glob patterns
SELECT 
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as total_nodes,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as classes
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;

-- Same analysis using DuckDB-style pattern arrays (NEW!)
SELECT 
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as total_nodes,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as classes
FROM read_ast([
    'src/**/*.py',     -- Python source
    'src/**/*.js',     -- JavaScript source  
    'src/**/*.ts',     -- TypeScript source
    'lib/**/*.cpp',    -- C++ libraries
    'include/**/*.hpp' -- C++ headers
], ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;
```

### Finding Specific Constructs

```sql
-- Find all function definitions with their complexity
SELECT 
    file_path,
    name,
    start_line,
    descendant_count as complexity_score
FROM read_ast('src/**/*.py', ignore_errors := true)
WHERE semantic_type = 240 AND name IS NOT NULL
ORDER BY complexity_score DESC;

-- Find all imports/includes across languages (pattern array approach)
SELECT 
    file_path,
    type,
    peek as import_statement,
    language
FROM read_ast([
    '**/*.py',    -- Python imports
    '**/*.js',    -- JavaScript imports  
    '**/*.ts',    -- TypeScript imports
    '**/*.java',  -- Java imports
    '**/*.cpp',   -- C++ includes
    '**/*.hpp'    -- C++ header includes
], ignore_errors := true)
WHERE semantic_type = 48  -- EXTERNAL_IMPORT (more precise than text matching)
ORDER BY file_path;
```

### Cross-Language Analysis

```sql
-- Compare function complexity across languages
SELECT 
    language,
    COUNT(*) as function_count,
    AVG(descendant_count) as avg_complexity,
    MAX(descendant_count) as max_complexity
FROM read_ast('**/*.*', ignore_errors := true)
WHERE semantic_type = 240
GROUP BY language
ORDER BY avg_complexity DESC;

-- Find similar patterns across languages using semantic types
SELECT 
    semantic_type,
    semantic_type_to_string(semantic_type) as semantic_name,
    COUNT(*) as occurrence_count,
    COUNT(DISTINCT language) as languages_used
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY semantic_type
HAVING COUNT(DISTINCT language) > 1
ORDER BY occurrence_count DESC;
```

### Performance and Filtering

```sql
-- Efficient filtering using semantic types
SELECT file_path, name, type
FROM read_ast('**/*.{py,js}', ignore_errors := true)
WHERE (semantic_type & 0xF0) = 112  -- All DEFINITION types
  AND name IS NOT NULL;

-- Find code complexity hotspots
SELECT 
    file_path,
    name,
    depth,
    descendant_count,
    start_line
FROM read_ast('**/*.*', ignore_errors := true)
WHERE semantic_type = 240  -- Functions only
  AND depth > 5  -- Deeply nested
  AND descendant_count > 100  -- Complex
ORDER BY descendant_count DESC;
```

### Error Handling and Robustness

```sql
-- Robust analysis of potentially problematic codebases
SELECT 
    language,
    COUNT(DISTINCT file_path) as processed_files,
    SUM(CASE WHEN name IS NOT NULL THEN 1 ELSE 0 END) as named_constructs
FROM read_ast('**/*.*', ignore_errors := true)
WHERE semantic_type IN (115, 119, 123)  -- Functions, classes, variables
GROUP BY language;

-- Check for files that might have parsing issues
WITH file_stats AS (
    SELECT 
        file_path,
        language,
        COUNT(*) as node_count,
        COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as function_count
    FROM read_ast('**/*.*', ignore_errors := true)
    GROUP BY file_path, language
)
SELECT file_path, language, node_count, function_count
FROM file_stats
WHERE node_count < 5  -- Suspiciously few nodes (might indicate parsing issues)
ORDER BY node_count;
```

---

## Performance Notes

- **Streaming**: Files are processed one-by-one, so results appear incrementally
- **Memory efficient**: Only one file's AST is in memory at a time
- **Pattern arrays**: File deduplication and sorting are handled efficiently at the C++ level
- **Glob patterns**: Use `**/*.ext` for recursive searches, arrays for precise control
- **Error handling**: Use `ignore_errors := true` for robust large-codebase analysis
- **Semantic filtering**: Use semantic types for efficient cross-language queries

### Array Performance Tips
```sql
-- Efficient: Specific patterns reduce file system traversal
SELECT * FROM read_ast(['src/**/*.py', 'lib/**/*.js']);

-- Less efficient: Broad patterns require more file system work
SELECT * FROM read_ast('**/*.*', ignore_errors := true);

-- Most efficient: Direct file lists when you know exactly what to parse
SELECT * FROM read_ast(['main.py', 'utils.js', 'config.cpp']);
```

---

*This API reference reflects the current implementation of the Sitting Duck extension. For conceptual information and AI agent usage patterns, see [AI_AGENT_GUIDE.md](AI_AGENT_GUIDE.md).*