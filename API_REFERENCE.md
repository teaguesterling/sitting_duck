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

### `read_ast(file_pattern, [language], [options...])`

**Main parsing function** - Reads source code files and returns a flattened AST table.

**Parameters:**
- `file_pattern` (VARCHAR): File path or glob pattern
  - Single file: `'script.py'`
  - Multiple files: `'src/**/*.py'`
  - Cross-language: `'**/*.{py,js,cpp}'`
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

-- Parse multiple files with error handling
SELECT file_path, COUNT(*) as nodes
FROM read_ast('src/**/*.py', ignore_errors := true)
GROUP BY file_path;

-- Find all function definitions across languages
SELECT file_path, name, start_line, language
FROM read_ast('**/*.{py,js,cpp}', ignore_errors := true)
WHERE semantic_type = 115; -- DEFINITION_FUNCTION

-- Explicit language specification
SELECT * FROM read_ast('script', 'python');
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

---

## Scalar Functions

### `parse_ast_objects(source_code, language)`

**Object-based parsing** - Returns AST as a single struct value.

**Parameters:**
- `source_code` (VARCHAR): Source code to parse
- `language` (VARCHAR): Programming language

**Returns:** Single AST struct with nodes array and source metadata

**Example:**
```sql
SELECT parse_ast_objects('class Test: pass', 'python') as ast;
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

### Semantic Type Categories

The 8-bit encoding follows this pattern: `[ss kk tt ll]`
- **ss (bits 6-7)**: Super Kind (4 major categories)
- **kk (bits 4-5)**: Kind (16 subcategories)
- **tt (bits 2-3)**: Super Type (4 variants per kind)
- **ll (bits 0-1)**: Language-specific (reserved)

#### Major Categories:
- **DATA_STRUCTURE** (0x00-0x3F): Literals, names, patterns, types
- **COMPUTATION** (0x40-0x7F): Operators, definitions, transformations
- **CONTROL_EFFECTS** (0x80-0xBF): Flow control, error handling, organization
- **META_EXTERNAL** (0xC0-0xFF): Metadata, imports, parser-specific

#### Common Semantic Types:
```sql
-- Key semantic type constants
115  -- DEFINITION_FUNCTION (functions across all languages)
119  -- DEFINITION_CLASS (classes/structs/interfaces)
123  -- DEFINITION_VARIABLE (variable declarations)
127  -- DEFINITION_MODULE (modules/namespaces)
80   -- EXECUTION_INVOCATION (function calls)
136  -- FLOW_CONDITIONAL (if/switch statements)
132  -- FLOW_LOOP (for/while loops)
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
-- Get overview of a codebase
SELECT 
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as total_nodes,
    COUNT(CASE WHEN semantic_type = 115 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 119 THEN 1 END) as classes
FROM read_ast('**/*.*', ignore_errors := true)
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

-- Find all imports/includes across languages
SELECT 
    file_path,
    type,
    peek as import_statement,
    language
FROM read_ast('**/*.*', ignore_errors := true)
WHERE type ILIKE '%import%' OR type ILIKE '%include%'
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
- **Glob patterns**: Use `**/*.ext` for recursive directory searches
- **Error handling**: Use `ignore_errors := true` for robust large-codebase analysis
- **Semantic filtering**: Use semantic types for efficient cross-language queries

---

*This API reference reflects the current implementation of the Sitting Duck extension. For conceptual information and AI agent usage patterns, see [AI_AGENT_GUIDE.md](AI_AGENT_GUIDE.md).*