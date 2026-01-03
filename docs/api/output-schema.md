# Output Schema

Complete reference for `read_ast()` output columns.

## Quick Reference

**Default output (20 columns):**

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | BIGINT | Unique node identifier within file |
| `type` | VARCHAR | Tree-sitter AST node type |
| `semantic_type` | SEMANTIC_TYPE | Universal semantic category |
| `flags` | UTINYINT | Node property flags |
| `name` | VARCHAR | Extracted identifier name |
| `signature_type` | VARCHAR | Type/return type information |
| `parameters` | STRUCT[] | Function parameters with names and types |
| `modifiers` | VARCHAR[] | Access modifiers and keywords |
| `annotations` | VARCHAR | Decorator/annotation text |
| `qualified_name` | VARCHAR | Fully qualified name |
| `file_path` | VARCHAR | Source file path |
| `language` | VARCHAR | Programming language |
| `start_line` | UINTEGER | Starting line (1-based) |
| `end_line` | UINTEGER | Ending line (1-based) |
| `parent_id` | BIGINT | Parent node ID |
| `depth` | UINTEGER | Tree depth (0 for root) |
| `sibling_index` | UINTEGER | Position among siblings |
| `children_count` | UINTEGER | Direct child count |
| `descendant_count` | UINTEGER | Total descendant count |
| `peek` | VARCHAR | Source code snippet |

**Additional columns with `source := 'full'` (22 columns total):**

| Column | Type | Description |
|--------|------|-------------|
| `start_column` | UINTEGER | Starting column (1-based) |
| `end_column` | UINTEGER | Ending column (1-based) |

```sql
-- Get column positions with source := 'full'
SELECT name, start_line, start_column, end_line, end_column
FROM read_ast('example.py', source := 'full')
WHERE is_function_definition(semantic_type);
```

---

## Identity Columns

### `node_id`

**Type:** `BIGINT`

Unique identifier for each AST node within a file.

```sql
SELECT node_id, type, name
FROM read_ast('example.py')
ORDER BY node_id;
```

- Starts at 0 for the root node
- Sequential within each file
- Use with `parent_id` for tree traversal

---

### `type`

**Type:** `VARCHAR`

Language-specific AST node type from Tree-sitter.

```sql
-- Common types vary by language
SELECT DISTINCT type
FROM read_ast('example.py')
ORDER BY type;
```

**Python examples:** `function_definition`, `class_definition`, `identifier`
**JavaScript examples:** `function_declaration`, `class_declaration`, `identifier`

---

### `semantic_type`

**Type:** `SEMANTIC_TYPE` (custom logical type)

Universal semantic category. This is a custom DuckDB type that:
- **Displays as string** - Shows `DEFINITION_FUNCTION` instead of numeric code
- **Supports string comparison** - `WHERE semantic_type = 'DEFINITION_FUNCTION'`
- **Stores efficiently** - Underlying storage is UTINYINT for fast comparisons

```sql
-- Direct string comparison (natural syntax)
SELECT * FROM read_ast('example.py')
WHERE semantic_type = 'DEFINITION_FUNCTION';

-- Group by semantic type
SELECT semantic_type, COUNT(*)
FROM read_ast('example.py')
GROUP BY semantic_type
ORDER BY COUNT(*) DESC;
```

- Cross-language semantic classification
- See [Semantic Types](semantic-types.md) for values

---

### `flags`

**Type:** `UTINYINT`

Bitfield containing node properties. Use helper predicates to check flags:

| Flag | Predicate | Description |
|------|-----------|-------------|
| `IS_CONSTRUCT` | `is_construct(flags)` | Semantic construct (not punctuation) |
| `IS_EMBODIED` | `is_embodied(flags)` / `has_body(flags)` | Has implementation body |
| `IS_DECLARATION_ONLY` | `is_declaration_only(flags)` | Forward declaration without body |
| `IS_SYNTAX_ONLY` | `is_syntax_only(flags)` | Pure syntax token (keyword, punctuation) |

```sql
-- Find function definitions with implementations (not just declarations)
SELECT name, file_path
FROM read_ast('**/*.cpp', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND has_body(flags);

-- Find forward declarations only
SELECT name, file_path
FROM read_ast('**/*.h', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND is_declaration_only(flags);
```

---

## Name and Extraction Columns

### `name`

**Type:** `VARCHAR` (nullable)

Extracted identifier or name for the node.

```sql
SELECT name, type, start_line
FROM read_ast('example.py')
WHERE name IS NOT NULL;
```

- NULL for nodes without meaningful names
- Populated for definitions, identifiers, and named constructs
- Extraction depends on language-specific rules

---

### `signature_type`

**Type:** `VARCHAR` (nullable)

Type information extracted from the node. Meaning varies by semantic type:

| Semantic Type | `signature_type` Contains |
|---------------|---------------------------|
| `DEFINITION_FUNCTION` | Return type (`int`, `void`, `*big.Int`) |
| `DEFINITION_CLASS` | Class kind (`class`, `interface`, `trait`, `struct`) |
| `COMPUTATION_CALL` | Full call expression (`obj.method`, `pkg.func`) |
| `DEFINITION_VARIABLE` | Variable type |

```sql
-- Find functions with their return types
SELECT name, signature_type as return_type, start_line
FROM read_ast('src/**/*.go')
WHERE is_function_definition(semantic_type);

-- Find method calls by signature
SELECT file_path, start_line, signature_type
FROM read_ast('src/**/*.cpp')
WHERE is_function_call(semantic_type)
  AND signature_type LIKE '%.empty';
```

---

### `parameters`

**Type:** `STRUCT("name" VARCHAR, "type" VARCHAR)[]`

Function parameters as an array of structs containing parameter name and type.

```sql
-- List functions with their parameters
SELECT
    name,
    parameters,
    array_length(parameters) as param_count
FROM read_ast('example.py')
WHERE is_function_definition(semantic_type);

-- Find functions with specific parameter names
SELECT name, parameters
FROM read_ast('src/**/*.py')
WHERE is_function_definition(semantic_type)
  AND list_contains([p.name FOR p IN parameters], 'self');
```

- Empty array `[]` for functions with no parameters
- Parameter types may be NULL for dynamically-typed languages

---

### `modifiers`

**Type:** `VARCHAR[]`

Access modifiers, keywords, and other declarative attributes.

```sql
-- Find public static methods in Java
SELECT name, modifiers
FROM read_ast('src/**/*.java')
WHERE is_function_definition(semantic_type)
  AND list_contains(modifiers, 'public')
  AND list_contains(modifiers, 'static');

-- Find async functions
SELECT name, file_path
FROM read_ast('src/**/*.js')
WHERE is_function_definition(semantic_type)
  AND list_contains(modifiers, 'async');
```

Common modifier values by language:
- **Java:** `public`, `private`, `protected`, `static`, `final`, `abstract`
- **JavaScript:** `async`, `const`, `let`, `var`
- **Go:** `var`
- **Rust:** `pub`, `mut`, `async`

---

### `annotations`

**Type:** `VARCHAR` (nullable)

Decorator or annotation text for the node.

```sql
-- Find decorated Python functions
SELECT name, annotations
FROM read_ast('src/**/*.py')
WHERE is_function_definition(semantic_type)
  AND annotations IS NOT NULL;
```

---

### `qualified_name`

**Type:** `VARCHAR` (nullable)

Fully qualified name including namespace/module path.

```sql
SELECT qualified_name, name
FROM read_ast('src/**/*.java')
WHERE is_class_definition(semantic_type);
```

Note: This field is not fully populated for all languages yet.

---

## Source Location Columns

### `file_path`

**Type:** `VARCHAR`

Path to the source file.

```sql
SELECT DISTINCT file_path
FROM read_ast('src/**/*.py');
```

- Relative paths preserved from input
- Absolute paths if provided as input

---

### `language`

**Type:** `VARCHAR`

Detected or specified programming language.

```sql
SELECT language, COUNT(*) as node_count
FROM read_ast(['**/*.py', '**/*.js'], ignore_errors := true)
GROUP BY language;
```

---

### `start_line` / `end_line`

**Type:** `UINTEGER`

Line numbers (1-based).

```sql
SELECT name, start_line, end_line, end_line - start_line + 1 as line_count
FROM read_ast('example.py')
WHERE type = 'function_definition';
```

---

### `start_column` / `end_column`

**Type:** `UINTEGER`

Column positions (1-based). **Only available with `source := 'full'`.**

```sql
-- Must use source := 'full' to get column positions
SELECT name, start_line, start_column, end_line, end_column
FROM read_ast('example.py', source := 'full')
WHERE type = 'identifier';
```

---

## Tree Structure Columns

### `parent_id`

**Type:** `BIGINT` (nullable)

Node ID of the parent node.

```sql
-- Find children of a specific node
SELECT type, name
FROM read_ast('example.py')
WHERE parent_id = 5;
```

- NULL for root node
- Use for tree traversal

---

### `depth`

**Type:** `UINTEGER`

Tree depth (0 for root).

```sql
-- Find deeply nested code
SELECT type, name, depth
FROM read_ast('example.py')
WHERE depth > 5
ORDER BY depth DESC;
```

---

### `sibling_index`

**Type:** `UINTEGER`

Position among siblings (0-based).

```sql
SELECT type, sibling_index
FROM read_ast('example.py')
WHERE parent_id = 0
ORDER BY sibling_index;
```

---

### `children_count`

**Type:** `UINTEGER`

Number of direct child nodes.

```sql
SELECT type, name, children_count
FROM read_ast('example.py')
WHERE children_count > 10;
```

---

### `descendant_count`

**Type:** `UINTEGER`

Total number of descendant nodes. Useful as a complexity metric.

```sql
-- Find complex functions
SELECT name, descendant_count as complexity
FROM read_ast('example.py')
WHERE type = 'function_definition'
ORDER BY complexity DESC;
```

---

## Content Columns

### `peek`

**Type:** `VARCHAR` (nullable)

Source code snippet for the node.

```sql
SELECT type, name, peek
FROM read_ast('example.py', peek := 100)
WHERE type = 'function_definition';
```

- Size controlled by `peek` parameter
- NULL if `peek := 'none'`
- Use `peek := 'full'` for complete source text

---

## Column Availability by Parameter

### By `context` Parameter

| Column | `'none'` | `'node_types_only'` | `'normalized'` | `'native'` |
|--------|----------|---------------------|----------------|------------|
| `node_id` | Yes | Yes | Yes | Yes |
| `type` | Yes | Yes | Yes | Yes |
| `semantic_type` | No | Yes | Yes | Yes |
| `flags` | No | Yes | Yes | Yes |
| `name` | No | No | Yes | Yes |
| `signature_type` | No | No | No | Yes |
| `parameters` | No | No | No | Yes |
| `modifiers` | No | No | No | Yes |
| `annotations` | No | No | No | Yes |
| `qualified_name` | No | No | No | Yes |
| `file_path` | Yes | Yes | Yes | Yes |
| `language` | Yes | Yes | Yes | Yes |
| `start_line` | Yes | Yes | Yes | Yes |
| `end_line` | Yes | Yes | Yes | Yes |
| `parent_id` | Yes* | Yes* | Yes* | Yes |
| `depth` | Yes* | Yes* | Yes* | Yes |
| `sibling_index` | Yes* | Yes* | Yes* | Yes |
| `children_count` | Yes* | Yes* | Yes* | Yes |
| `descendant_count` | Yes* | Yes* | Yes* | Yes |
| `peek` | Yes** | Yes** | Yes** | Yes** |

\* Depends on `structure` parameter
\** Depends on `peek` parameter

### By `source` Parameter

| Column | `'none'` | `'path'` | `'lines_only'` | `'lines'` | `'full'` |
|--------|----------|----------|----------------|-----------|----------|
| `file_path` | No | Yes | Yes | Yes | Yes |
| `start_line` | No | No | Yes | Yes | Yes |
| `end_line` | No | No | Yes | Yes | Yes |
| `start_column` | No | No | No | No | **Yes** |
| `end_column` | No | No | No | No | **Yes** |

```sql
-- Default: line positions only
SELECT start_line, end_line FROM read_ast('example.py');

-- With source := 'full': includes column positions
SELECT start_line, start_column, end_line, end_column
FROM read_ast('example.py', source := 'full');
```

---

## Working with Output

### Create Table from Results

```sql
CREATE TABLE ast_cache AS
SELECT * FROM read_ast('src/**/*.py', ignore_errors := true);

-- Query cached data
SELECT type, COUNT(*) FROM ast_cache GROUP BY type;
```

### Export to Parquet

```sql
COPY (
    SELECT file_path, type, name, start_line, semantic_type
    FROM read_ast('src/**/*.py')
) TO 'ast_export.parquet';
```

### Join with Other Data

```sql
-- Join with file metadata
SELECT
    ast.file_path,
    ast.name,
    files.last_modified
FROM read_ast('src/**/*.py') ast
JOIN file_metadata files ON ast.file_path = files.path
WHERE ast.type = 'function_definition';
```

---

## Next Steps

- [Core Functions](core-functions.md) - Function reference
- [Parameters](parameters.md) - Parameter reference
- [Semantic Types](semantic-types.md) - Type system
- [Native Extraction Semantics](../native_extraction_semantics.md) - Cross-language field behavior
