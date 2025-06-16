# DuckDB AST Extension API Reference

## Overview

The DuckDB AST Extension provides powerful tools for parsing, analyzing, and querying Abstract Syntax Trees (ASTs) from source code. It supports multiple programming languages and offers both table functions for bulk operations and scalar functions for detailed analysis.

## Table of Contents

1. [Core Parsing Functions](#core-parsing-functions)
2. [Semantic Type System](#semantic-type-system)
3. [AST Query Functions](#ast-query-functions)
4. [Language Support](#language-support)
5. [Data Types](#data-types)
6. [Performance Notes](#performance-notes)

## Core Parsing Functions

### Table Functions

#### `read_ast(file_path, language)`
Reads and parses files matching the given path pattern, returning a flattened table of AST nodes.

**Parameters:**
- `file_path` (VARCHAR): File path or glob pattern (e.g., `'src/*.py'`, `'**/*.js'`)
- `language` (VARCHAR, optional): Target language. If omitted, auto-detected from file extension

**Returns:** Table with columns:
- `node_id` (BIGINT): Unique node identifier
- `type` (VARCHAR): Raw node type from parser
- `name` (VARCHAR): Extracted node name (e.g., function name)
- `file_path` (VARCHAR): Source file path
- `start_line`, `end_line` (INTEGER): Line numbers
- `start_column`, `end_column` (INTEGER): Column positions
- `parent_id` (BIGINT): Parent node ID (-1 for root)
- `depth` (INTEGER): Tree depth
- `sibling_index` (INTEGER): Position among siblings
- `children_count` (INTEGER): Direct children count
- `descendant_count` (INTEGER): Total descendant count
- `peek` (VARCHAR): Source code preview
- `semantic_type` (TINYINT): Semantic type code
- `universal_flags` (TINYINT): Node flags
- `arity_bin` (TINYINT): Arity classification

**Example:**
```sql
SELECT * FROM read_ast('src/*.py', 'python');
SELECT * FROM read_ast('**/*.js');  -- Auto-detect JavaScript
```

#### `read_ast_objects(file_path, language)`
Like `read_ast` but returns a single column containing AST structs, preserving the tree structure.

**Parameters:** Same as `read_ast`

**Returns:** Table with single column:
- `ast` (STRUCT): Complete AST structure with `nodes` array and `source` metadata

**Example:**
```sql
SELECT ast.nodes[1].name FROM read_ast_objects('main.py');
```

### Scalar Functions

#### `parse_ast(source_code, language)`
Parses a string of source code, returning a flattened table of AST nodes.

**Parameters:**
- `source_code` (VARCHAR): Source code to parse
- `language` (VARCHAR): Programming language

**Returns:** Same table structure as `read_ast`

**Example:**
```sql
SELECT * FROM parse_ast('def hello(): return "world"', 'python');
```

#### `parse_ast_objects(source_code, language)`
Parses source code and returns an AST struct.

**Parameters:** Same as `parse_ast`

**Returns:** Single AST struct value

**Example:**
```sql
SELECT parse_ast_objects('class Foo {}', 'javascript').nodes[1].type;
```

## Semantic Type System

The extension uses an 8-bit encoding system to classify AST nodes semantically, making cross-language analysis possible.

### Type Hierarchy

1. **Super Kinds** (highest level):
   - `DATA_STRUCTURE` - Literals, names, patterns, types
   - `COMPUTATION` - Operations, calls, transformations, definitions
   - `CONTROL_EFFECTS` - Execution, flow control, error handling
   - `META_EXTERNAL` - Metadata, imports/exports, parser-specific

2. **Kinds** (categories within super kinds):
   - Examples: `LITERAL`, `NAME`, `OPERATOR`, `DEFINITION`, `FLOW_CONTROL`

3. **Semantic Types** (specific node types):
   - Examples: `DEFINITION_FUNCTION`, `COMPUTATION_CALL`, `FLOW_CONDITIONAL`

### Semantic Type Functions

#### `semantic_type_to_string(type_code)`
Converts a semantic type code to its human-readable name.

**Parameters:**
- `type_code` (TINYINT): Semantic type code

**Returns:** VARCHAR - Type name (e.g., 'DEFINITION_FUNCTION')

**Example:**
```sql
SELECT semantic_type_to_string(112);  -- Returns 'DEFINITION_FUNCTION'
```

#### `semantic_type_code(type_name)`
Converts a semantic type name to its numeric code.

**Parameters:**
- `type_name` (VARCHAR): Semantic type name

**Returns:** UTINYINT - Type code (NULL if invalid name)

**Example:**
```sql
SELECT semantic_type_code('DEFINITION_FUNCTION');  -- Returns 112
```

#### `get_super_kind(type_code)`
Gets the super kind name for a semantic type.

**Parameters:**
- `type_code` (TINYINT): Semantic type code

**Returns:** VARCHAR - Super kind name

**Example:**
```sql
SELECT get_super_kind(112);  -- Returns 'COMPUTATION'
```

#### `get_kind(type_code)`
Gets the kind name for a semantic type.

**Parameters:**
- `type_code` (TINYINT): Semantic type code

**Returns:** VARCHAR - Kind name

**Example:**
```sql
SELECT get_kind(112);  -- Returns 'DEFINITION'
```

#### `kind_code(kind_name)`
Converts a kind name to its numeric code.

**Parameters:**
- `kind_name` (VARCHAR): Kind name

**Returns:** UTINYINT - Kind code (NULL if invalid name)

**Example:**
```sql
SELECT kind_code('DEFINITION');  -- Returns 112
```

### Predicate Functions

#### `is_definition(type_code)`
Checks if a semantic type represents any kind of definition.

**Parameters:**
- `type_code` (TINYINT): Semantic type code

**Returns:** BOOLEAN

**Example:**
```sql
SELECT * FROM ast WHERE is_definition(semantic_type);
```

#### `is_call(type_code)`
Checks if a semantic type represents a function/method call.

**Parameters:**
- `type_code` (TINYINT): Semantic type code

**Returns:** BOOLEAN

#### `is_control_flow(type_code)`
Checks if a semantic type represents control flow (if, loop, etc.).

**Parameters:**
- `type_code` (TINYINT): Semantic type code

**Returns:** BOOLEAN

#### `is_identifier(type_code)`
Checks if a semantic type represents an identifier or name.

**Parameters:**
- `type_code` (TINYINT): Semantic type code

**Returns:** BOOLEAN

#### `is_kind(type_code, kind_name)`
Checks if a semantic type belongs to a specific kind.

**Parameters:**
- `type_code` (TINYINT): Semantic type code
- `kind_name` (VARCHAR): Kind name to check

**Returns:** BOOLEAN

**Example:**
```sql
SELECT * FROM ast WHERE is_kind(semantic_type, 'DEFINITION');
```

#### `is_semantic_type(type_code, pattern)`
Flexible pattern matching for semantic types.

**Parameters:**
- `type_code` (TINYINT): Semantic type code
- `pattern` (VARCHAR): Pattern to match (e.g., 'FUNCTION', 'CLASS', 'LITERAL')

**Returns:** BOOLEAN

### Collection Functions

#### `get_searchable_types()`
Returns a list of semantic types commonly used for code search and indexing.

**Parameters:** None

**Returns:** LIST<TINYINT> - Array of semantic type codes

**Example:**
```sql
SELECT * FROM ast 
WHERE semantic_type = ANY(get_searchable_types());
```

## AST Query Functions

### SQL Macros (Embedded)

The extension includes powerful SQL macros for AST manipulation:

#### Tree Navigation
- `ast_children(ast, parent_id)` - Get direct children of a node
- `ast_descendants(ast, parent_id)` - Get all descendants
- `ast_ancestors(ast, node_id)` - Get ancestor chain
- `ast_siblings(ast, node_id)` - Get sibling nodes

#### Node Search
- `ast_find(ast, type_pattern)` - Find nodes by type pattern
- `ast_find_by_name(ast, name_pattern)` - Find nodes by name
- `ast_find_functions(ast)` - Find all function definitions
- `ast_find_classes(ast)` - Find all class definitions

#### Tree Extraction
- `ast_get_functions(ast)` - Extract function subtrees
- `ast_get_classes(ast)` - Extract class subtrees
- `ast_get_imports(ast)` - Extract import statements

## Language Support

### Supported Languages

| Language | File Extensions | Auto-Detection |
|----------|----------------|----------------|
| Python | `.py` | ✓ |
| JavaScript | `.js`, `.mjs`, `.cjs` | ✓ |
| TypeScript | `.ts`, `.tsx` | ✓ |
| C++ | `.cpp`, `.cc`, `.cxx`, `.hpp`, `.h` | ✓ |
| C | `.c`, `.h` | ✓ |
| Java | `.java` | ✓ |
| Go | `.go` | ✓ |
| Ruby | `.rb` | ✓ |
| SQL | `.sql` | ✓ |
| HTML | `.html`, `.htm` | ✓ |
| CSS | `.css` | ✓ |
| Markdown | `.md` | ✓ |

### Language Functions

#### `ast_supported_languages()`
Returns a table of all supported languages with their metadata.

**Returns:** Table with columns:
- `language` (VARCHAR): Language identifier
- `aliases` (LIST<VARCHAR>): Alternative names
- `extensions` (LIST<VARCHAR>): File extensions

## Data Types

### AST Struct

The AST struct type contains:
```
STRUCT(
    nodes: LIST<STRUCT(
        node_id: BIGINT,
        type: VARCHAR,
        name: VARCHAR,
        start_line: INTEGER,
        end_line: INTEGER,
        start_column: INTEGER,
        end_column: INTEGER,
        parent_id: BIGINT,
        depth: INTEGER,
        sibling_index: INTEGER,
        children_count: INTEGER,
        descendant_count: INTEGER,
        peek: VARCHAR,
        semantic_type: TINYINT,
        universal_flags: TINYINT,
        arity_bin: TINYINT
    )>,
    source: STRUCT(
        file_path: VARCHAR,
        language: VARCHAR
    )
)
```

## Performance Notes

### Optimization Tips

1. **Use Semantic Types for Filtering**
   ```sql
   -- Fast: Uses semantic type index
   WHERE semantic_type = 112
   
   -- Slower: String comparison
   WHERE type = 'function_definition'
   ```

2. **Leverage Searchable Types**
   ```sql
   -- Index only searchable nodes
   CREATE TABLE code_index AS
   SELECT * FROM ast 
   WHERE semantic_type = ANY(get_searchable_types());
   ```

3. **Use Object Format for Tree Operations**
   ```sql
   -- Better for tree navigation
   SELECT ast_children(ast, 0) 
   FROM read_ast_objects('file.py');
   ```

4. **Parse Once, Query Many**
   ```sql
   -- Parse files into a table
   CREATE TABLE project_ast AS 
   SELECT * FROM read_ast('src/**/*.py');
   
   -- Then query efficiently
   SELECT * FROM project_ast WHERE is_definition(semantic_type);
   ```

### Performance Characteristics

- **Parsing Speed**: ~31ms for 5,000+ nodes (typical for large files)
- **Memory Usage**: Linear with AST size
- **Query Performance**: Depends on selectivity and indexes

## Common Patterns

### Find All Definitions
```sql
SELECT name, semantic_type_to_string(semantic_type) as type
FROM read_ast('**/*.py')
WHERE is_definition(semantic_type);
```

### Build a Symbol Index
```sql
CREATE TABLE symbols AS
SELECT DISTINCT name, semantic_type, file_path, start_line
FROM read_ast('**/*.*')
WHERE is_definition(semantic_type) OR is_identifier(semantic_type);
```

### Cross-Language Analysis
```sql
-- Find all function definitions across languages
SELECT 
    file_path,
    COUNT(*) as function_count
FROM read_ast('**/*.*')
WHERE semantic_type = semantic_type_code('DEFINITION_FUNCTION')
GROUP BY file_path;
```

### Extract Documentation
```sql
-- Find all comments and docstrings
SELECT peek as documentation
FROM read_ast('**/*.py')
WHERE semantic_type = semantic_type_code('METADATA_COMMENT');
```

## Error Handling

- Invalid file paths return empty results (unless using `ignore_errors=false`)
- Unsupported languages throw `InvalidInputException`
- Malformed source code may result in partial ASTs
- Use `TRY_CAST` for safe type conversions in queries