# Output Schema

Complete reference for `read_ast()` output columns.

## Core Columns

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
- Depends on `context` parameter level

---

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
SELECT language, COUNT(*)
FROM read_ast(['**/*.py', '**/*.js'], ignore_errors := true)
GROUP BY language;
```

## Location Columns

### `start_line` / `end_line`

**Type:** `UINTEGER`

Line numbers (1-based).

```sql
SELECT name, start_line, end_line, end_line - start_line + 1 as line_count
FROM read_ast('example.py')
WHERE type = 'function_definition';
```

### `start_column` / `end_column`

**Type:** `UINTEGER`

Column positions (1-based).

```sql
SELECT name, start_line, start_column
FROM read_ast('example.py')
WHERE type = 'identifier';
```

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

Total number of descendant nodes (complexity metric).

```sql
-- Find complex functions
SELECT name, descendant_count as complexity
FROM read_ast('example.py')
WHERE type = 'function_definition'
ORDER BY complexity DESC;
```

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

## Semantic Columns

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

## Column Availability by Context

| Column | `'none'` | `'node_types_only'` | `'normalized'` | `'native'` |
|--------|----------|---------------------|----------------|------------|
| `node_id` | Yes | Yes | Yes | Yes |
| `type` | Yes | Yes | Yes | Yes |
| `name` | No | No | Yes | Yes |
| `file_path` | Yes | Yes | Yes | Yes |
| `language` | Yes | Yes | Yes | Yes |
| `start_line` | Yes | Yes | Yes | Yes |
| `end_line` | Yes | Yes | Yes | Yes |
| `parent_id` | Yes* | Yes* | Yes* | Yes |
| `depth` | Yes* | Yes* | Yes* | Yes |
| `semantic_type` | No | Yes | Yes | Yes |
| `peek` | Yes** | Yes** | Yes** | Yes** |

\* Depends on `structure` parameter
\** Depends on `peek` parameter

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

## Next Steps

- [Core Functions](core-functions.md) - Function reference
- [Parameters](parameters.md) - Parameter reference
- [Semantic Types](semantic-types.md) - Type system
