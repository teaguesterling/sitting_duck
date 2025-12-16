# Basic Usage

Learn the fundamentals of using Sitting Duck for code analysis.

## Core Concepts

### AST Nodes

Every piece of source code is represented as a tree of nodes. Each node has:

| Property | Description |
|----------|-------------|
| `node_id` | Unique identifier |
| `type` | Language-specific node type (e.g., `function_definition`) |
| `name` | Extracted name (if applicable) |
| `semantic_type` | Universal semantic category (cross-language) |
| `start_line`, `end_line` | Location in source file |
| `depth` | Nesting level in the tree |
| `descendant_count` | Number of child nodes (complexity metric) |
| `peek` | Source code snippet |

### The `read_ast()` Function

The main function for parsing source files:

```sql
-- Basic usage
SELECT * FROM read_ast('file.py');

-- With language override
SELECT * FROM read_ast('file.txt', 'python');

-- With options
SELECT * FROM read_ast('file.py', ignore_errors := true);
```

## Filtering Nodes

### By Node Type

```sql
-- Find specific node types
SELECT * FROM read_ast('example.py')
WHERE type = 'function_definition';

-- Multiple types
SELECT * FROM read_ast('example.py')
WHERE type IN ('function_definition', 'class_definition');

-- Pattern matching
SELECT * FROM read_ast('example.py')
WHERE type LIKE '%definition%';
```

### By Semantic Type

Semantic types provide language-agnostic filtering:

```sql
-- Find all function definitions (any language)
SELECT * FROM read_ast('**/*.*', ignore_errors := true)
WHERE semantic_type = 240;  -- DEFINITION_FUNCTION

-- Use helper functions
SELECT * FROM read_ast('example.py')
WHERE is_definition(semantic_type);
```

### By Location

```sql
-- Nodes in a specific line range
SELECT * FROM read_ast('example.py')
WHERE start_line BETWEEN 10 AND 50;

-- Top-level nodes only
SELECT * FROM read_ast('example.py')
WHERE depth = 1;
```

## Working with Names

### Extract Named Elements

```sql
-- Functions with names
SELECT name, type, start_line
FROM read_ast('example.py')
WHERE name IS NOT NULL AND type = 'function_definition';

-- Find a specific function
SELECT * FROM read_ast('example.py')
WHERE name = 'my_function';
```

### Name Patterns

```sql
-- Find test functions
SELECT name, start_line
FROM read_ast('test_*.py')
WHERE type = 'function_definition'
  AND name LIKE 'test_%';

-- Find private methods (Python convention)
SELECT name, start_line
FROM read_ast('example.py')
WHERE type = 'function_definition'
  AND name LIKE '\_%';
```

## Analyzing Structure

### Tree Depth

```sql
-- Find deeply nested code
SELECT type, name, depth, start_line
FROM read_ast('example.py')
WHERE depth > 5
ORDER BY depth DESC;
```

### Complexity Metrics

```sql
-- Function complexity by descendant count
SELECT
    name,
    descendant_count as complexity,
    end_line - start_line + 1 as line_count
FROM read_ast('example.py')
WHERE type = 'function_definition'
ORDER BY complexity DESC;
```

### Parent-Child Relationships

```sql
-- Find children of a specific node
WITH target AS (
    SELECT node_id
    FROM read_ast('example.py')
    WHERE type = 'class_definition' AND name = 'MyClass'
)
SELECT type, name, start_line
FROM read_ast('example.py')
WHERE parent_id = (SELECT node_id FROM target);
```

## Working with Source Code

### Peek Content

The `peek` column contains a snippet of the source code:

```sql
-- See function signatures
SELECT name, peek
FROM read_ast('example.py')
WHERE type = 'function_definition';
```

### Controlling Peek Size

```sql
-- Larger peek for more context
SELECT name, peek
FROM read_ast('example.py', peek := 200)
WHERE type = 'function_definition';

-- Different peek modes
SELECT name, peek
FROM read_ast('example.py', peek := 'smart')
WHERE type = 'class_definition';
```

## Error Handling

### Ignore Parse Errors

```sql
-- Continue processing even if some files have errors
SELECT file_path, COUNT(*)
FROM read_ast('**/*.py', ignore_errors := true)
GROUP BY file_path;
```

### Find Parse Errors

```sql
-- Find files with parse errors
SELECT file_path, type, peek
FROM read_ast('**/*.py', ignore_errors := true)
WHERE type = 'ERROR';
```

## Output Formatting

### Aggregations

```sql
-- Node type distribution
SELECT type, COUNT(*) as count
FROM read_ast('example.py')
GROUP BY type
ORDER BY count DESC;
```

### Hierarchical Display

```sql
-- Pretty-print the tree structure
SELECT
    repeat('│ ', depth - 1) || '├─ ' || type as tree,
    COALESCE(name, '') as name,
    start_line as line
FROM read_ast('example.py')
ORDER BY node_id;
```

## Next Steps

- [Parsing Files](../guide/parsing-files.md) - Advanced file processing
- [Multi-File Processing](../guide/multi-file.md) - Glob patterns and arrays
- [Semantic Types](../guide/semantic-types.md) - Cross-language analysis
