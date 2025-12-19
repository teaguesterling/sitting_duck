# Semantic Types

The semantic type system enables cross-language code analysis with a universal taxonomy.

## Overview

Every AST node has a `semantic_type` that represents its universal meaning, independent of the source language. This allows you to write queries that work across Python, JavaScript, Java, and all 27 supported languages.

## Semantic Type Values

### Quick Reference

| Code | Type | Description |
|------|------|-------------|
| 240 | DEFINITION_FUNCTION | Function/method definitions |
| 248 | DEFINITION_CLASS | Class/struct/interface definitions |
| 244 | DEFINITION_VARIABLE | Variable declarations |
| 208 | COMPUTATION_CALL | Function/method calls |
| 144 | FLOW_CONDITIONAL | If/switch statements |
| 148 | FLOW_LOOP | For/while loops |
| 152 | FLOW_JUMP | Return/break/continue |
| 48 | EXTERNAL_IMPORT | Import statements |
| 32 | METADATA_COMMENT | Comments |
| 80 | NAME_IDENTIFIER | Identifiers |

### Helper Functions

```sql
-- Convert code to readable name
SELECT semantic_type_to_string(240);  -- 'DEFINITION_FUNCTION'

-- Check categories
SELECT is_definition(240);     -- true
SELECT is_call(208);          -- true
SELECT is_control_flow(144);  -- true
SELECT is_identifier(80);     -- true

-- Get hierarchy info
SELECT get_super_kind(240);   -- Super kind code
SELECT get_kind(240);         -- Kind code
```

## Using Semantic Types

### Find All Functions

```sql
-- Works across all languages
SELECT name, file_path, language, start_line
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
WHERE semantic_type = 240  -- DEFINITION_FUNCTION
ORDER BY file_path, start_line;
```

### Find All Classes

```sql
SELECT name, file_path, language
FROM read_ast(['**/*.py', '**/*.java', '**/*.cpp'], ignore_errors := true)
WHERE semantic_type = 248  -- DEFINITION_CLASS
ORDER BY file_path;
```

### Count by Semantic Type

```sql
SELECT
    semantic_type_to_string(semantic_type) as type_name,
    COUNT(*) as count
FROM read_ast('src/**/*.py')
GROUP BY semantic_type
ORDER BY count DESC
LIMIT 10;
```

## Semantic Type Hierarchy

The 8-bit encoding follows this structure:

```
[ss kk tt ll]
 │  │  │  └── Language-specific bits (0-3)
 │  │  └───── Super Type (4-7)
 │  └──────── Kind (8-11)
 └─────────── Super Kind (12-15)
```

### Super Kinds (Top Level)

| Super Kind | Range | Description |
|------------|-------|-------------|
| META_EXTERNAL | 0x00-0x3F | Metadata, imports, parser specifics |
| DATA_STRUCTURE | 0x40-0x7F | Literals, names, types |
| CONTROL_EFFECTS | 0x80-0xBF | Control flow, execution |
| COMPUTATION | 0xC0-0xFF | Operations, definitions |

### Detailed Categories

#### META_EXTERNAL (0-63)

| Kind | Super Types | Description |
|------|-------------|-------------|
| PARSER_SPECIFIC | CONSTRUCT, DELIMITER, PUNCTUATION, SYNTAX | Language-specific syntax |
| METADATA | COMMENT, ANNOTATION, DIRECTIVE, DEBUG | Documentation and metadata |
| EXTERNAL | IMPORT, EXPORT, FOREIGN, EMBED | External references |

#### DATA_STRUCTURE (64-127)

| Kind | Super Types | Description |
|------|-------------|-------------|
| LITERAL | NUMBER, STRING, ATOMIC, STRUCTURED | Values |
| NAME | IDENTIFIER, QUALIFIED, SCOPED, ATTRIBUTE | Names |
| PATTERN | DESTRUCTURE, COLLECT, TEMPLATE, MATCH | Patterns |
| TYPE | PRIMITIVE, COMPOSITE, REFERENCE, GENERIC | Type info |

#### CONTROL_EFFECTS (128-191)

| Kind | Super Types | Description |
|------|-------------|-------------|
| EXECUTION | STATEMENT, DECLARATION, INVOCATION, MUTATION | Execution |
| FLOW_CONTROL | CONDITIONAL, LOOP, JUMP, SYNC | Control flow |
| ERROR_HANDLING | TRY, CATCH, THROW, FINALLY | Exceptions |
| ORGANIZATION | BLOCK, LIST, SECTION, CONTAINER | Structure |

#### COMPUTATION (192-255)

| Kind | Super Types | Description |
|------|-------------|-------------|
| OPERATOR | ARITHMETIC, LOGICAL, COMPARISON, ASSIGNMENT | Operators |
| COMPUTATION_NODE | CALL, ACCESS, EXPRESSION, LAMBDA | Computations |
| TRANSFORM | QUERY, ITERATION, PROJECTION, AGGREGATION | Transforms |
| DEFINITION | FUNCTION, VARIABLE, CLASS, MODULE | Definitions |

## Cross-Language Analysis

### Compare Languages

```sql
SELECT
    language,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as classes,
    COUNT(CASE WHEN semantic_type = 148 THEN 1 END) as loops
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
GROUP BY language;
```

### Find Common Patterns

```sql
-- Semantic types used across multiple languages
SELECT
    semantic_type_to_string(semantic_type) as type_name,
    COUNT(DISTINCT language) as language_count,
    COUNT(*) as total_occurrences
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
GROUP BY semantic_type
HAVING COUNT(DISTINCT language) > 1
ORDER BY total_occurrences DESC;
```

### Complexity by Language

```sql
SELECT
    language,
    AVG(descendant_count) as avg_function_complexity,
    MAX(descendant_count) as max_function_complexity
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
WHERE semantic_type = 240  -- Functions only
GROUP BY language;
```

## Semantic Refinements

Many semantic types include refinements for more specific categorization:

### Function Refinements

| Refinement | Description |
|------------|-------------|
| `REGULAR` | Standard function |
| `LAMBDA` | Anonymous/arrow function |
| `CONSTRUCTOR` | Class constructor |
| `GETTER` / `SETTER` | Property accessors |
| `ASYNC` | Async functions |

### Variable Refinements

| Refinement | Description |
|------------|-------------|
| `MUTABLE` | Mutable (`let`, `var`) |
| `IMMUTABLE` | Constant (`const`, `final`) |
| `PARAMETER` | Function parameter |
| `FIELD` | Class/struct field |

### Class Refinements

| Refinement | Description |
|------------|-------------|
| `REGULAR` | Standard class |
| `ABSTRACT` | Abstract/interface |
| `ENUM` | Enumeration |
| `STRUCT` | Struct type |

## Universal Flags

Each node also has a `flags` field with orthogonal properties:

| Flag | Value | Description |
|------|-------|-------------|
| `IS_CONSTRUCT` | 0x01 | Semantic construct (not punctuation) |
| `IS_EMBODIED` | 0x02 | Has body (definition vs declaration) |

### Using Flag Helpers

```sql
-- Distinguish definitions from declarations
SELECT name, type,
    CASE WHEN is_embodied(flags) THEN 'Definition'
         ELSE 'Declaration' END as kind
FROM read_ast('**/*.cpp', ignore_errors := true)
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND name IS NOT NULL;

-- Find only functions with implementations
SELECT name, file_path
FROM read_ast('src/**/*.cpp')
WHERE semantic_type = 'DEFINITION_FUNCTION' AND has_body(flags);

-- Using is_definition() helper for all definition types
SELECT name, file_path
FROM read_ast('src/**/*.cpp')
WHERE is_definition(semantic_type) AND has_body(flags);
```

## Filtering Patterns

### By Super Kind

```sql
-- All definitions (functions, classes, variables)
SELECT * FROM read_ast('file.py')
WHERE (semantic_type & 0xC0) = 0xC0;  -- COMPUTATION super kind

-- All control flow
SELECT * FROM read_ast('file.py')
WHERE (semantic_type & 0xC0) = 0x80;  -- CONTROL_EFFECTS super kind
```

### By Kind

```sql
-- All flow control (conditionals, loops, jumps)
SELECT * FROM read_ast('file.py')
WHERE (semantic_type & 0xF0) = 144;  -- FLOW_CONTROL kind
```

### Using Helper Functions

```sql
-- More readable filtering
SELECT * FROM read_ast('file.py')
WHERE is_definition(semantic_type) AND name IS NOT NULL;

SELECT * FROM read_ast('file.py')
WHERE is_control_flow(semantic_type);
```

## Next Steps

- [Context Extraction](context-extraction.md) - Native semantic analysis
- [Cross-Language Analysis](cross-language.md) - Comparing codebases
- [API Reference](../api/semantic-types.md) - Complete type reference
