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
| `universal_flags` | UTINYINT | Additional semantic flags |
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
WHERE semantic_type = 115; -- DEFINITION_FUNCTION

-- Array with explicit language (applies to all files)
SELECT * FROM read_ast(['script1.py', 'script2.py'], 'python');

-- Find cross-language patterns using arrays
SELECT language, COUNT(*) as import_count
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
WHERE semantic_type = 208; -- EXTERNAL_IMPORT
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
WHERE semantic_type = 115; -- Functions only
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
| **DATA_STRUCTURE** | 0x00-0x3F | LITERAL, NAME, PATTERN, TYPE | Data representation and identification |
| **COMPUTATION** | 0x40-0x7F | OPERATOR, COMPUTATION_NODE, TRANSFORM, DEFINITION | Operations and definitions |
| **CONTROL_EFFECTS** | 0x80-0xBF | EXECUTION, FLOW_CONTROL, ERROR_HANDLING, ORGANIZATION | Program flow and effects |
| **META_EXTERNAL** | 0xC0-0xFF | METADATA, EXTERNAL, PARSER_SPECIFIC, RESERVED | Meta-information and language specifics |

#### Detailed Type Reference

**DATA_STRUCTURE (0x00-0x3F)**

| Kind | Super Type | Code | Examples | Python | JavaScript | Java | C++ |
|------|------------|------|----------|--------|------------|------|-----|
| **LITERAL** | NUMBER | 0 | Numeric values | `42`, `3.14` | `42`, `3.14` | `42`, `3.14f` | `42`, `3.14` |
| | STRING | 4 | String/text values | `"hello"`, `'world'` | `"hello"`, `'world'` | `"hello"` | `"hello"` |
| | ATOMIC | 8 | Boolean/null values | `True`, `False`, `None` | `true`, `false`, `null` | `true`, `false`, `null` | `true`, `false`, `nullptr` |
| | STRUCTURED | 12 | Collection literals | `[1,2,3]`, `{'a':1}` | `[1,2,3]`, `{a:1}` | `{1,2,3}` | `{1,2,3}` |
| **NAME** | KEYWORD | 16 | Language keywords | `def`, `class`, `if` | `function`, `class`, `if` | `public`, `class`, `if` | `class`, `struct`, `if` |
| | IDENTIFIER | 20 | Simple names | `variable`, `func` | `variable`, `func` | `variable`, `method` | `variable`, `function` |
| | QUALIFIED | 24 | Dotted names | `obj.method` | `obj.method` | `obj.method` | `obj.method` |
| | SCOPED | 28 | Scope references | `self`, `super` | `this`, `super` | `this`, `super` | `this`, `::global` |

**COMPUTATION (0x40-0x7F)**

| Kind | Super Type | Code | Examples | Python | JavaScript | Java | C++ |
|------|------------|------|----------|--------|------------|------|-----|
| **OPERATOR** | ARITHMETIC | 64 | Math operators | `+`, `-`, `*`, `**` | `+`, `-`, `*`, `**` | `+`, `-`, `*` | `+`, `-`, `*` |
| | LOGICAL | 68 | Logic operators | `and`, `or`, `not` | `&&`, `\|\|`, `!` | `&&`, `\|\|`, `!` | `&&`, `\|\|`, `!` |
| | COMPARISON | 72 | Comparison ops | `==`, `!=`, `in` | `===`, `!==`, `in` | `==`, `!=` | `==`, `!=` |
| | ASSIGNMENT | 76 | Assignment ops | `=`, `+=`, `:=` | `=`, `+=` | `=`, `+=` | `=`, `+=` |
| **COMPUTATION_NODE** | CALL | 80 | Function calls | `func()`, `obj.method()` | `func()`, `obj.method()` | `func()`, `obj.method()` | `func()`, `obj->method()` |
| | ACCESS | 84 | Member access | `obj.attr`, `arr[0]` | `obj.attr`, `arr[0]` | `obj.field`, `arr[0]` | `obj.member`, `arr[0]` |
| | EXPRESSION | 88 | Complex expressions | `a + b * c` | `a + b * c` | `a + b * c` | `a + b * c` |
| | LAMBDA | 92 | Anonymous functions | `lambda x: x+1` | `x => x+1` | `x -> x+1` | `[](int x){return x+1;}` |
| **DEFINITION** | FUNCTION | 112 | Function definitions | `def func():` | `function func(){}` | `void func(){}` | `void func(){}` |
| | VARIABLE | 116 | Variable definitions | `x = 5` | `let x = 5` | `int x = 5` | `int x = 5` |
| | CLASS | 120 | Class definitions | `class MyClass:` | `class MyClass{}` | `class MyClass{}` | `class MyClass{}` |
| | MODULE | 124 | Module definitions | `import sys` | `import fs from 'fs'` | `package com.example` | `namespace example` |

**CONTROL_EFFECTS (0x80-0xBF)**

| Kind | Super Type | Code | Examples | Python | JavaScript | Java | C++ |
|------|------------|------|----------|--------|------------|------|-----|
| **EXECUTION** | STATEMENT | 128 | Expression statements | Expression as statement | Expression as statement | Expression as statement | Expression as statement |
| | INVOCATION | 136 | Function calls (effects) | `print()`, `obj.mutate()` | `console.log()`, `obj.mutate()` | `System.out.println()` | `std::cout << x` |
| **FLOW_CONTROL** | CONDITIONAL | 144 | Conditional flow | `if`, `elif`, `else` | `if`, `else if`, `else` | `if`, `else if`, `switch` | `if`, `else if`, `switch` |
| | LOOP | 148 | Iteration | `for`, `while` | `for`, `while` | `for`, `while` | `for`, `while` |
| | JUMP | 152 | Control jumps | `break`, `continue`, `return` | `break`, `continue`, `return` | `break`, `continue`, `return` | `break`, `continue`, `return`, `goto` |
| | SYNC | 156 | Concurrency control | `async`, `await` | `async`, `await` | `synchronized` | `std::async` |
| **ERROR_HANDLING** | TRY | 160 | Try blocks | `try:` | `try {` | `try {` | `try {` |
| | CATCH | 164 | Exception handling | `except Exception:` | `catch (e) {` | `catch (Exception e) {` | `catch (std::exception& e) {` |
| | THROW | 168 | Exception throwing | `raise Exception()` | `throw new Error()` | `throw new Exception()` | `throw std::runtime_error()` |
| **ORGANIZATION** | BLOCK | 176 | Code blocks | `if: block` | `{ block }` | `{ block }` | `{ block }` |
| | LIST | 180 | Parameter/argument lists | `(a, b, c)` | `(a, b, c)` | `(int a, int b)` | `(int a, int b)` |

**META_EXTERNAL (0xC0-0xFF)**

| Kind | Super Type | Code | Examples | Python | JavaScript | Java | C++ |
|------|------------|------|----------|--------|------------|------|-----|
| **METADATA** | COMMENT | 192 | Documentation | `# comment`, `"""docstring"""` | `// comment`, `/* comment */` | `// comment`, `/** javadoc */` | `// comment`, `/* comment */` |
| | ANNOTATION | 196 | Decorators/attributes | `@decorator` | `@decorator` | `@Annotation` | `[[attribute]]` |
| | DIRECTIVE | 200 | Preprocessor | `# type: ignore` | `// @ts-ignore` | N/A | `#include`, `#define` |
| **EXTERNAL** | IMPORT | 208 | Import statements | `import os`, `from x import y` | `import fs from 'fs'` | `import java.util.*` | `#include <iostream>` |
| | EXPORT | 212 | Export statements | N/A (implicit) | `export default`, `export {x}` | `public class` | `extern` |
| **PARSER_SPECIFIC** | PUNCTUATION | 224 | Language punctuation | `:`, `,`, `;` | `:`, `,`, `;` | `:`, `,`, `;` | `:`, `,`, `;` |
| | CONSTRUCT | 236 | Unique constructs | `yield`, `with` | `yield*`, destructuring | annotations | templates, RAII |

#### Quick Reference - Most Common Types

| Code | Type | Description | Use Case |
|------|------|-------------|----------|
| 112 | DEFINITION_FUNCTION | Function definitions across all languages | API extraction, complexity analysis |
| 120 | DEFINITION_CLASS | Class/struct/interface definitions | Architecture analysis, OOP patterns |
| 136 | EXECUTION_INVOCATION | Function/method calls | Dependency analysis, call graphs |
| 144 | FLOW_CONDITIONAL | If/switch/match statements | Complexity metrics, control flow |
| 148 | FLOW_LOOP | For/while/do-while loops | Complexity metrics, performance analysis |
| 192 | METADATA_COMMENT | Comments and documentation | Documentation coverage, code quality |
| 208 | EXTERNAL_IMPORT | Import/include statements | Dependency mapping, module analysis |

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

Currently supports **12 programming languages** with full semantic analysis:

| Language | Extensions | Tree-sitter Grammar |
|----------|------------|-------------------|
| **Python** | `.py` | tree-sitter-python |
| **JavaScript** | `.js`, `.jsx` | tree-sitter-javascript |
| **TypeScript** | `.ts`, `.tsx` | tree-sitter-typescript |
| **C** | `.c`, `.h` | tree-sitter-c |
| **C++** | `.cpp`, `.hpp`, `.cc`, `.cxx`, `.h` | tree-sitter-cpp |
| **Java** | `.java` | tree-sitter-java |
| **Go** | `.go` | tree-sitter-go |
| **Ruby** | `.rb` | tree-sitter-ruby |
| **SQL** | `.sql` | tree-sitter-sql |
| **CSS** | `.css` | tree-sitter-css |
| **HTML** | `.html`, `.htm` | tree-sitter-html |
| **Markdown** | `.md`, `.markdown` | tree-sitter-markdown |

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
    COUNT(CASE WHEN semantic_type = 115 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 119 THEN 1 END) as classes
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;

-- Same analysis using DuckDB-style pattern arrays (NEW!)
SELECT 
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as total_nodes,
    COUNT(CASE WHEN semantic_type = 115 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 120 THEN 1 END) as classes
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
WHERE semantic_type = 115 AND name IS NOT NULL
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
WHERE semantic_type = 208  -- EXTERNAL_IMPORT (more precise than text matching)
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
WHERE semantic_type = 115
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
WHERE semantic_type = 115  -- Functions only
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
        COUNT(CASE WHEN semantic_type = 115 THEN 1 END) as function_count
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