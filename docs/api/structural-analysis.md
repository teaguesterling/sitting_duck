# Structural Analysis Macros

SQL table macros for navigating and analyzing AST structure. These macros accept a table name as a string and return table results.

## Overview

| Macro | Purpose |
|-------|---------|
| **Tree Navigation** | |
| `ast_children(table, node_id)` | Get direct children of a node |
| `ast_descendants(table, node_id)` | Get all descendants of a node |
| `ast_ancestors(table, node_id)` | Get ancestors from node to root |
| `ast_siblings(table, node_id)` | Get siblings (same parent) |
| `ast_containing_line(table, line)` | Find nodes containing a line |
| `ast_in_range(table, start, end)` | Find nodes in a line range |
| **Scope Analysis** | |
| `ast_function_scope(table, node_id)` | Function descendants excluding nested functions |
| `ast_class_members(table, node_id)` | Direct members of a class |
| **Code Metrics** | |
| `ast_function_metrics(table)` | Complexity metrics per function |
| `ast_nesting_analysis(table)` | Nesting depth analysis per function |
| `ast_functions_containing(table, type)` | Find functions with specific node types |
| **Quality Analysis** | |
| `ast_security_audit(table)` | Detect security anti-patterns |
| `ast_dead_code(table)` | Find unused functions/classes |

---

## Tree Navigation

### `ast_children(table, node_id)`

Get direct children of a node.

```sql
CREATE TEMP TABLE my_ast AS SELECT * FROM parse_ast('def foo(): pass', 'python');

-- Get children of the function definition (node_id=1)
SELECT type, name FROM ast_children('my_ast', 1);
-- def, identifier (foo), parameters, :, block
```

### `ast_descendants(table, node_id)`

Get all descendants of a node (subtree).

```sql
-- Get everything inside a function
SELECT COUNT(*) FROM ast_descendants('my_ast', 1);
```

Uses `descendant_count` for O(1) subtree queries - no recursion needed.

### `ast_ancestors(table, node_id)`

Get all ancestors from a node up to the root.

```sql
-- Find containing scopes for a deeply nested node
SELECT type, name FROM ast_ancestors('my_ast', 22) ORDER BY depth;
-- module → function_definition → block → function_definition → block → ...
```

Returns a `depth` column (0 = root) for ordering.

### `ast_siblings(table, node_id)`

Get sibling nodes (same parent, excluding self).

```sql
-- Find other statements in the same block
SELECT type, name FROM ast_siblings('my_ast', 14) ORDER BY node_id;
```

### `ast_containing_line(table, line)`

Find all nodes whose range contains a specific line.

```sql
-- What's on line 5?
SELECT type, name FROM ast_containing_line('my_ast', 5)
ORDER BY (end_line - start_line);  -- smallest (most specific) first
```

### `ast_in_range(table, start_line, end_line)`

Find all nodes within a line range.

```sql
-- Get all nodes in lines 10-20
SELECT type, name FROM ast_in_range('my_ast', 10, 20);
```

---

## Scope Analysis

### `ast_function_scope(table, node_id)`

Get function descendants **excluding nested function bodies**.

This is critical for accurate per-function analysis. Without it, metrics for outer functions would incorrectly include inner function code.

```sql
-- Outer function has 38 total descendants, inner has 15
-- ast_function_scope returns 38 - 15 = 23 nodes for outer
SELECT COUNT(*) FROM ast_function_scope('my_ast', 1);  -- 23, not 38
```

Example with nested functions:
```python
def outer():
    x = 1           # Included in outer's scope
    def inner():
        y = 2       # NOT included in outer's scope
        return y
    return inner()  # Included in outer's scope
```

### `ast_class_members(table, node_id)`

Get direct members of a class (methods, fields, nested classes).

```sql
SELECT name, type FROM ast_class_members('my_ast', 1) ORDER BY node_id;
-- class_var    assignment
-- __init__     function_definition
-- method       function_definition
```

Excludes:
- Local variables inside methods
- Syntax nodes (keywords, punctuation)
- Nodes with empty names

---

## Code Metrics

### `ast_function_metrics(table)`

Compute complexity metrics for all functions.

```sql
SELECT name, lines, return_count, conditionals, loops, cyclomatic
FROM ast_function_metrics('my_ast')
ORDER BY cyclomatic DESC;
```

| Column | Description |
|--------|-------------|
| `node_id` | Function node ID |
| `file_path` | Source file path |
| `name` | Function name |
| `language` | Source language |
| `start_line` | First line of function |
| `end_line` | Last line of function |
| `lines` | Total lines (end - start + 1) |
| `return_count` | Number of return statements |
| `conditionals` | Count of if/switch/match/ternary |
| `loops` | Count of for/while/do loops |
| `cyclomatic` | Cyclomatic complexity (1 + conditionals + loops) |

**Scope awareness**: Uses `ast_function_scope` internally - nested function metrics are separate.

```sql
-- outer: 1 return, 0 conditionals (inner's if excluded)
-- inner: 2 returns, 1 conditional
SELECT name, return_count, conditionals, cyclomatic
FROM ast_function_metrics('nested_ast');
```

### `ast_nesting_analysis(table)`

Analyze nesting depth per function.

```sql
SELECT name, max_depth, avg_depth, deep_nodes
FROM ast_nesting_analysis('my_ast')
ORDER BY max_depth DESC;
```

| Column | Description |
|--------|-------------|
| `name` | Function name |
| `max_depth` | Maximum nesting depth within function |
| `avg_depth` | Average nesting depth |
| `deep_nodes` | Count of nodes with relative depth > 5 |
| `total_nodes` | Total nodes in function scope |

### `ast_functions_containing(table, node_type)`

Find functions that contain a specific node type.

```sql
-- Find functions with try statements
SELECT func_name, match_line, match_peek
FROM ast_functions_containing('my_ast', 'try_statement');

-- Find functions calling eval
SELECT func_name, match_name
FROM ast_functions_containing('my_ast', 'call')
WHERE match_name = 'eval';
```

| Column | Description |
|--------|-------------|
| `func_name` | Containing function name |
| `func_line` | Function start line |
| `match_name` | Name of matched node (if any) |
| `match_line` | Line of matched node |
| `match_peek` | Preview of matched node |

**Scope awareness**: Respects function boundaries - nested function contents attributed to the nested function, not the outer one.

---

## Quality Analysis

### `ast_security_audit(table)`

Detect common security anti-patterns.

```sql
SELECT function_name, risk_category, risk_level, finding, matched_pattern
FROM ast_security_audit('my_ast')
WHERE risk_level = 'high'
ORDER BY function_name;
```

| Risk Category | Patterns | Level |
|---------------|----------|-------|
| Code Injection | `eval`, `exec`, `compile`, `Function` | high |
| Command Injection | `system`, `popen`, `spawn`, `execSync` | high |
| Deserialization | `pickle.load`, `yaml.load`, `unserialize` | high |
| SQL Injection | `execute`, `raw`, `rawQuery` | medium |
| Path Traversal | `readFile`, `writeFile`, `unlink` | medium |
| Weak Crypto | `md5`, `sha1`, `DES`, `RC4` | medium |
| Debug Code | `console.log`, `print`, `debugger` | low |

Output columns:
- `file_path`, `language`, `start_line`
- `function_name` - containing function (or `<module>`)
- `risk_category`, `risk_level`, `finding`
- `matched_pattern` - the detected pattern
- `context` - code preview

### `ast_dead_code(table)`

Find potentially unused functions and classes.

```sql
SELECT name, definition_type, file_path, start_line
FROM ast_dead_code('my_ast');
```

| Column | Description |
|--------|-------------|
| `name` | Function/class name |
| `definition_type` | `'function'` or `'class'` |
| `file_path` | Source file |
| `start_line`, `end_line` | Location |
| `type` | AST node type |
| `reason` | Why it's considered dead |

**Exclusions** (not flagged as dead):
- Dunder methods (`__init__`, `__str__`, etc.)
- Common entry points (`main`, `setup`, `teardown`, `init`, `constructor`)
- Functions/classes referenced anywhere (calls or identifiers)

**Limitations**:
- Heuristic analysis - cannot detect dynamic usage
- Best used on entire codebase, not single files
- Cross-file references require all files in the table

---

## Usage Patterns

### Analyze a Codebase

```sql
-- Create cached AST table
CREATE TABLE codebase AS
SELECT * FROM read_ast('src/**/*.py', ignore_errors := true);

-- Get function metrics for all code
SELECT * FROM ast_function_metrics('codebase')
WHERE cyclomatic > 10
ORDER BY cyclomatic DESC;

-- Security scan
SELECT * FROM ast_security_audit('codebase')
WHERE risk_level = 'high';

-- Find dead code
SELECT * FROM ast_dead_code('codebase');
```

### Investigate a Specific Function

```sql
-- Find the function
SELECT node_id, name FROM codebase
WHERE is_function_definition(semantic_type) AND name = 'process_data';
-- node_id = 42

-- Get its children
SELECT type, name FROM ast_children('codebase', 42);

-- Get metrics
SELECT * FROM ast_function_metrics('codebase') WHERE name = 'process_data';

-- Check nesting
SELECT * FROM ast_nesting_analysis('codebase') WHERE name = 'process_data';
```

### Find Complex Nested Code

```sql
SELECT name, max_depth, deep_nodes
FROM ast_nesting_analysis('codebase')
WHERE deep_nodes > 10
ORDER BY max_depth DESC;
```

---

## See Also

- [Utility Functions](utility-functions.md) - Semantic predicates and file utilities
- [Cookbook](../guide/cookbook.md) - More SQL recipes
- [Core Functions](core-functions.md) - `read_ast` and `parse_ast`
