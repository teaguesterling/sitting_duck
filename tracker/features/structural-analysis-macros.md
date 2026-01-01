# Feature: Structural Analysis SQL Macros

**Priority:** Medium
**Complexity:** Medium
**Status:** Proposed

## Summary

Add SQL macros and helper functions that simplify structural analysis patterns:
1. **Tree navigation helpers** - Low-level primitives for traversing AST relationships
2. **Analysis macros** - High-level functions built on the primitives

## Motivation

Current structural analysis requires verbose CTEs with range-based self-joins:

```sql
WITH function_bounds AS (
    SELECT file_path, name, start_line, end_line
    FROM read_ast('src/**/*.py')
    WHERE is_function_definition(semantic_type)
),
SELECT ...
FROM function_bounds f
JOIN read_ast('src/**/*.py') cf
  ON cf.file_path = f.file_path
  AND cf.start_line >= f.start_line
  AND cf.end_line <= f.end_line
  AND is_control_flow(cf.semantic_type)
```

The key insight is that we need two layers:
1. **Primitives** that make tree navigation easy
2. **Composed functions** that use primitives for common analyses

---

## Part 1: Tree Navigation Helpers

These are low-level building blocks that make AST relationship queries simple.

### `ast_children(ast, node_id)` → TABLE

Get immediate children of a node.

```sql
-- Get all children of a specific function
SELECT * FROM ast_children(
    (SELECT * FROM read_ast('example.py')),
    12345  -- node_id of a function
);
```

**Implementation approach:** Filter where `parent_id = node_id`

### `ast_descendants(ast, node_id)` → TABLE

Get all descendants of a node (the entire subtree).

```sql
-- Get everything inside a class definition
SELECT * FROM ast_descendants(
    (SELECT * FROM read_ast('example.py')),
    class_node_id
);
```

**Implementation approach:** Use `descendant_count` for efficient range queries, or recursive CTE on `parent_id`.

### `ast_ancestors(ast, node_id)` → TABLE

Get the ancestor chain from a node to the root.

```sql
-- Find what function/class contains this line
SELECT * FROM ast_ancestors(
    (SELECT * FROM read_ast('example.py')),
    some_node_id
) WHERE is_function_definition(semantic_type);
```

**Implementation approach:** Recursive CTE following `parent_id` upward.

### `ast_siblings(ast, node_id)` → TABLE

Get sibling nodes (same parent).

```sql
-- Find other statements at the same level
SELECT * FROM ast_siblings(ast, node_id);
```

### `ast_containing(ast, line_number)` → TABLE

Find all nodes that contain a specific line.

```sql
-- What function is line 42 in?
SELECT * FROM ast_containing(
    (SELECT * FROM read_ast('example.py')),
    42
) WHERE is_function_definition(semantic_type)
LIMIT 1;
```

### `ast_nodes_between(ast, start_line, end_line)` → TABLE

Get all nodes within a line range.

```sql
-- All nodes in lines 10-50
SELECT * FROM ast_nodes_between(ast, 10, 50);
```

---

## Part 2: Scope-Aware Helpers

These helpers understand code structure (functions, classes, blocks).

### `ast_function_scope(ast, function_node_id)` → TABLE

Get all nodes that are "inside" a function (respecting nested functions).

```sql
-- Get the body of a function, excluding nested function definitions
SELECT * FROM ast_function_scope(ast, func_id);
```

**Key difference from `ast_descendants`:** Excludes nested function bodies to avoid double-counting.

### `ast_class_members(ast, class_node_id)` → TABLE

Get direct members of a class (methods, fields), not their bodies.

```sql
SELECT name, type FROM ast_class_members(ast, class_id);
```

### `ast_block_contents(ast, node_id)` → TABLE

Get contents of a block/body, useful for analyzing control flow bodies.

---

## Part 3: High-Level Analysis Macros

### 1. `ast_function_metrics(file_path)`

Returns per-function metrics in one call.

**Output columns:**
- `file_path`, `name`, `start_line`, `end_line`
- `lines` (line count)
- `return_count` (number of return statements)
- `conditionals` (if/switch/match count)
- `loops` (for/while count)
- `cyclomatic` (complexity approximation)
- `max_depth` (deepest nesting level)

**Example:**
```sql
SELECT * FROM ast_function_metrics('src/**/*.py')
WHERE cyclomatic > 10
ORDER BY cyclomatic DESC;
```

### 2. `ast_functions_containing(file_path, node_type)`

Find functions that contain a specific node type.

**Example:**
```sql
-- Functions with error handling
SELECT * FROM ast_functions_containing('src/**/*.py', 'try_statement');

-- Functions using eval
SELECT * FROM ast_functions_containing('src/**/*.py', 'call')
WHERE child_name = 'eval';
```

### 3. `ast_nesting_analysis(file_path)`

Analyze nesting depth per function.

**Output columns:**
- `file_path`, `name`, `start_line`
- `max_depth` (deepest node)
- `avg_depth` (average node depth)
- `deep_nodes` (count of nodes with depth > 10)

### 4. `ast_security_audit(file_path)`

Automated security concern detection.

**Output columns:**
- `file_path`, `start_line`, `function_name`
- `risk_category` (Code Injection, Command Injection, etc.)
- `risk_level` (high, medium, low)
- `finding` (specific issue found)
- `context` (code snippet)

**Example:**
```sql
SELECT * FROM ast_security_audit('src/**/*.*')
WHERE risk_level = 'high';
```

### 5. `ast_dead_code(file_path)`

Find potentially unused code.

**Output columns:**
- `file_path`, `name`, `start_line`, `type`
- `reason` (never called, unreachable, etc.)

---

## Implementation Strategy

### Phase 1: Tree Navigation Primitives (SQL Macros)

These can be implemented as SQL macros using existing columns:

```sql
-- ast_children: Simple filter
CREATE MACRO ast_children(ast, parent_node_id) AS TABLE
    SELECT * FROM ast WHERE parent_id = parent_node_id;

-- ast_descendants: Use node_id range (nodes are in DFS order)
CREATE MACRO ast_descendants(ast, ancestor_id) AS TABLE
    WITH ancestor AS (
        SELECT node_id, descendant_count
        FROM ast WHERE node_id = ancestor_id
    )
    SELECT a.* FROM ast a, ancestor anc
    WHERE a.node_id > anc.node_id
      AND a.node_id <= anc.node_id + anc.descendant_count;

-- ast_ancestors: Recursive CTE
CREATE MACRO ast_ancestors(ast, child_id) AS TABLE
    WITH RECURSIVE ancestors AS (
        SELECT * FROM ast WHERE node_id = child_id
        UNION ALL
        SELECT a.* FROM ast a
        JOIN ancestors anc ON a.node_id = anc.parent_id
        WHERE anc.parent_id IS NOT NULL
    )
    SELECT * FROM ancestors;

-- ast_containing: Line range filter
CREATE MACRO ast_containing(ast, line_num) AS TABLE
    SELECT * FROM ast
    WHERE start_line <= line_num AND end_line >= line_num
    ORDER BY end_line - start_line;  -- Smallest containing node first
```

### Phase 2: Scope-Aware Helpers (SQL Macros)

Build on primitives:

```sql
-- ast_function_scope: Descendants minus nested function bodies
CREATE MACRO ast_function_scope(ast, func_id) AS TABLE
    WITH func AS (SELECT * FROM ast WHERE node_id = func_id),
         descendants AS (SELECT * FROM ast_descendants(ast, func_id)),
         nested_funcs AS (
             SELECT node_id, descendant_count FROM descendants
             WHERE is_function_definition(semantic_type) AND node_id != func_id
         )
    SELECT d.* FROM descendants d
    WHERE NOT EXISTS (
        SELECT 1 FROM nested_funcs nf
        WHERE d.node_id > nf.node_id
          AND d.node_id <= nf.node_id + nf.descendant_count
    );
```

### Phase 3: Analysis Macros (Table-Returning Functions)

These compose the primitives for common analyses. May require C++ for performance.

### Key Implementation Notes

1. **Node ordering:** Our `node_id` is assigned in DFS pre-order, which enables O(1) subtree queries using `descendant_count`.

2. **Performance optimization:** The primitives can be SQL macros, but high-level analysis functions may need C++ table functions for efficiency when scanning large codebases.

3. **Existing infrastructure:** We already have:
   - `parent_id` column for upward traversal
   - `descendant_count` for subtree size
   - `depth` for nesting analysis
   - `start_line`/`end_line` for range queries

4. **Language awareness:** Some analyses need language-specific knowledge:
   - Python: `try_statement`, decorators before functions
   - JavaScript: `try_statement`, arrow functions
   - Go: explicit error returns (no try/catch)
   - Rust: `Result` types, `?` operator

---

## Alternatives Considered

1. **Pure C++ table functions:** More performant but harder to maintain and extend
2. **View-based approach:** Pre-compute relationships on parse - adds overhead even when not used
3. **External tooling:** Delegate to existing static analysis tools - loses SQL composability
4. **Graph database:** Model AST as graph - overkill, loses DuckDB benefits

## Related Work

- **Cookbook:** `docs/guide/cookbook.md` - Contains verbose versions of these patterns
- **Semantic predicates:** `is_function_definition()`, `is_control_flow()`, etc.
- **Existing helpers:** `read_lines()` for source extraction

## Open Questions

1. Should primitives be SQL macros or C++ functions?
2. How to handle cross-file analysis (call graphs)?
3. Should we support user-defined metrics?
4. How to efficiently cache repeated traversals?
5. Should `ast_descendants` exclude the root node or include it?

## Success Criteria

A user should be able to write:

```sql
-- Before: 20+ lines of CTEs and joins
-- After: Simple, readable query
SELECT name, cyclomatic, max_depth
FROM ast_function_metrics('src/**/*.py')
WHERE cyclomatic > 10 OR max_depth > 8
ORDER BY cyclomatic DESC;
```

And for custom analysis:

```sql
-- Find functions that use eval but have no try/catch
SELECT f.name, f.file_path
FROM read_ast('src/**/*.py') f
WHERE is_function_definition(f.semantic_type)
  AND EXISTS (
      SELECT 1 FROM ast_function_scope(read_ast(f.file_path), f.node_id) s
      WHERE is_call(s.semantic_type) AND s.name = 'eval'
  )
  AND NOT EXISTS (
      SELECT 1 FROM ast_function_scope(read_ast(f.file_path), f.node_id) s
      WHERE s.type = 'try_statement'
  );
```
