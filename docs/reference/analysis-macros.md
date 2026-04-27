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
| `ast_call_arguments(table, call_node_id)` | Get arguments of a function call |
| **Relational Operators** | |
| `ast_has(source, ancestor_type, desc_type, desc_name)` | Ancestors containing a descendant at any depth |
| `ast_not_has(source, ancestor_type, desc_type, desc_name)` | Ancestors NOT containing a descendant |
| `ast_inside(source, desc_type, anc_type, anc_name, desc_name)` | Descendants within an ancestor type |
| `ast_precedes(source, node_type, before_type, before_name)` | Nodes preceding a sibling of given type |
| `ast_follows(source, node_type, after_type, after_name)` | Nodes following a sibling of given type |
| **Definition Helpers** | |
| `ast_definitions(table)` | Get all definitions with metadata |
| `ast_definition_parent(table)` | Nearest definition ancestor per definition |
| **Scope Analysis** | |
| `ast_function_scope(table, node_id)` | Function descendants excluding nested functions |
| `ast_class_members(table, node_id)` | Direct members of a class |
| **Code Metrics** | |
| `ast_function_metrics(table)` | Complexity metrics per function |
| `ast_nesting_analysis(table)` | Nesting depth analysis per function |
| `ast_functions_containing(table, type)` | Find functions with specific node types |
| **Call Analysis** | |
| `ast_get_calls(source)` | Extract calls with caller attribution and type |
| `ast_call_graph(source)` | Aggregated caller→callee graph |
| `ast_find_references(source, name)` | Scope-aware symbol reference resolution |
| `ast_callees(source)` | What does each function call? |
| `ast_callers(source)` | Who calls each function? |
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

### `ast_call_arguments(table, call_node_id)`

Extract arguments from a function call node.

```sql
CREATE TEMP TABLE call_ast AS SELECT * FROM parse_ast('foo(a, b, "hello", 42)', 'python');

-- Find the call node
SELECT node_id, name FROM call_ast WHERE type = 'call';
-- node_id=2, name=foo

-- Get its arguments
SELECT arg_position, arg_name, arg_type FROM ast_call_arguments('call_ast', 2);
```

| Column | Description |
|--------|-------------|
| `arg_position` | 0-based position in argument list |
| `arg_node_id` | Node ID of the argument |
| `arg_name` | Name or text of the argument |
| `arg_type` | AST type of the argument |
| `arg_peek` | Preview of the argument |
| `semantic_type` | Semantic type of the argument |
| `start_line`, `end_line` | Location |

Works across languages (Python, C++, Java, etc.) by detecting the appropriate argument list type (`argument_list`, `arguments`, `actual_parameters`).

---

## Relational Operators

These macros use the `descendant_count` range-check for O(1) subtree membership queries. Unlike tree navigation macros that take a table name, these take a source file path or glob and call `read_ast()` internally.

### `ast_has(source, ancestor_type, descendant_type, descendant_name, language)`

Find nodes of `ancestor_type` that contain a descendant of `descendant_type` at any depth.

```sql
-- Functions containing a return statement
SELECT name, start_line
FROM ast_has('src/**/*.py', 'function_definition', 'return_statement');

-- Functions containing a call to 'execute'
SELECT name, start_line
FROM ast_has('src/**/*.py', 'function_definition', 'call', 'execute');

-- If statements containing a return (unnamed ancestor type works)
SELECT start_line FROM ast_has('src/**/*.py', 'if_statement', 'return_statement');
```

### `ast_not_has(source, ancestor_type, descendant_type, descendant_name, language)`

Inverse of `ast_has` — find nodes that do NOT contain a descendant.

```sql
-- Functions WITHOUT return statements
SELECT name, start_line
FROM ast_not_has('src/**/*.py', 'function_definition', 'return_statement');
```

### `ast_inside(source, descendant_type, ancestor_type, ancestor_name, descendant_name, language)`

Find descendant nodes that are inside an ancestor of a given type.

```sql
-- Return statements inside function 'process_data'
SELECT peek, start_line
FROM ast_inside('src/**/*.py', 'return_statement', 'function_definition', 'process_data');

-- All calls to 'execute' inside any function
SELECT name, start_line
FROM ast_inside('src/**/*.py', 'call', 'function_definition', descendant_name := 'execute');
```

### `ast_precedes(source, node_type, before_type, before_name, language)`

Find nodes that come before (lower `sibling_index`) a sibling of given type.

```sql
-- Comments that appear before function definitions
SELECT peek, start_line
FROM ast_precedes('src/**/*.py', 'comment', 'function_definition');
```

### `ast_follows(source, node_type, after_type, after_name, language)`

Find nodes that come after (higher `sibling_index`) a sibling of given type.

```sql
-- Return statements that follow an if statement (same parent)
SELECT peek, start_line
FROM ast_follows('src/**/*.py', 'return_statement', 'if_statement');
```

> **Note:** `ast_has`/`ast_not_has` use AST subtree ranges, not function-scope-aware regions. A function containing a nested function will include the nested function's descendants. Use `ast_function_scope()` for scope-aware queries.

---

## Definition Helpers

### `ast_definitions(table)`

Get all definitions (functions, classes, variables, etc.) with unified metadata.

```sql
SELECT name, definition_type, start_line
FROM ast_definitions('my_ast')
ORDER BY start_line;
```

| Column | Description |
|--------|-------------|
| `name` | Definition name |
| `definition_type` | `function`, `class`, `variable`, `module`, `type` |
| `language` | Source language |
| `file_path` | Source file |
| `start_line`, `end_line` | Location |
| `node_id` | For joining back to AST |
| `type` | Original AST node type |
| `semantic_type` | Semantic type code |

```sql
-- Count definitions by type
SELECT definition_type, COUNT(*)
FROM ast_definitions('codebase')
GROUP BY definition_type;

-- Find all functions
SELECT name, file_path, start_line
FROM ast_definitions('codebase')
WHERE definition_type = 'function';
```

### `ast_definition_parent(table)`

Resolve the nearest definition ancestor for each definition node, skipping organizational/structural nodes (blocks, bodies). Useful for building stable structural identity keys like `(name, kind, parent_name, parent_kind)` for cross-revision diffs.

```sql
SELECT def_name, kind, parent_def_name, parent_def_kind
FROM ast_definition_parent('my_ast');
```

| Column | Description |
|--------|-------------|
| `node_id` | Definition node ID |
| `def_name` | Definition name |
| `kind` | Semantic type string (e.g., `DEFINITION_FUNCTION`) |
| `parent_def_name` | Name of nearest definition ancestor |
| `parent_def_kind` | Semantic type of nearest definition ancestor |
| `parent_def_node_id` | Node ID of nearest definition ancestor |

Example resolution chain for Python:
```
inner_var  (DEFINITION_VARIABLE) → nested     (DEFINITION_FUNCTION)
nested     (DEFINITION_FUNCTION) → method     (DEFINITION_FUNCTION)
method     (DEFINITION_FUNCTION) → OuterClass (DEFINITION_CLASS)
OuterClass (DEFINITION_CLASS)    → <module>   (DEFINITION_MODULE)
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

## Call Analysis

### `ast_get_calls(source, language := NULL)`

Extract all function/method calls with caller attribution and call-type classification. Uses `scope.function` for O(1) caller lookup.

```sql
-- All calls in a codebase
SELECT caller_name, called_name, call_type, start_line
FROM ast_get_calls('src/**/*.py');

-- Method calls only
SELECT caller_name, called_name
FROM ast_get_calls('src/**/*.py')
WHERE call_type = 'method';
```

| Column | Description |
|--------|-------------|
| `file_path` | Source file |
| `caller_name` | Containing function (or `<module>`) |
| `called_name` | Called function/method name |
| `call_expression` | Code preview |
| `call_type` | `function`, `method`, `constructor`, or `macro` |
| `language` | Source language |
| `start_line` | Call site line |
| `node_id` | Call node ID |
| `caller_node_id` | Caller function node ID |

### `ast_call_graph(source, language := NULL)`

Build an aggregated caller-to-callee graph with call counts.

```sql
-- Full call graph
SELECT caller, callee, call_type, call_count
FROM ast_call_graph('src/**/*.py')
ORDER BY call_count DESC;

-- What does main() call?
SELECT callee, call_type, call_count
FROM ast_call_graph('src/**/*.py')
WHERE caller = 'main';
```

| Column | Description |
|--------|-------------|
| `file_path` | Source file |
| `caller` | Caller function name |
| `callee` | Called function name |
| `call_type` | Call classification |
| `call_count` | Number of calls |

### `ast_find_references(source, target_name, language := NULL)`

Find all uses of a symbol via scope-chain resolution. Handles shadowed names correctly — if two scopes define the same name, only references that resolve to the specific definition are returned.

```sql
-- All references to 'process'
SELECT ref_kind, start_line, scope_name, peek
FROM ast_find_references('src/**/*.py', 'process');

-- Call sites only
SELECT start_line, peek
FROM ast_find_references('src/**/*.py', 'execute')
WHERE ref_kind = 'call';
```

| Column | Description |
|--------|-------------|
| `file_path` | Source file |
| `name` | Target symbol name |
| `ref_kind` | `definition`, `call`, or `reference` |
| `node_type` | AST node type |
| `start_line` | Location |
| `peek` | Code preview |
| `scope_name` | Containing function (or `<module>`) |
| `def_node_id` | Resolved definition's node ID |

### `ast_callees(source, language := NULL)`

Find what each function calls. Simpler than `ast_get_calls` — no call-type classification.

```sql
SELECT caller, callee, callee_line
FROM ast_callees('src/**/*.py')
WHERE caller = 'main';
```

### `ast_callers(source, language := NULL)`

Find who calls each function. Module-level calls get `caller = '<module>'`.

```sql
SELECT caller, callee, call_line
FROM ast_callers('src/**/*.py')
WHERE callee = 'execute';
```

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
- [Cookbook](../how-to/cookbook.md) - More SQL recipes
- [Core Functions](functions.md) - `read_ast` and `parse_ast`
