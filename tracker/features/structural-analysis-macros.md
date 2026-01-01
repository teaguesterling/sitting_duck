# Feature: Structural Analysis SQL Macros

**Priority:** Medium
**Complexity:** Medium
**Status:** Proposed

## Summary

Add SQL macros that simplify common structural analysis patterns, reducing verbose self-join queries to simple function calls.

## Motivation

Current structural analysis (e.g., "find all functions with high cyclomatic complexity") requires verbose CTEs with range-based self-joins:

```sql
WITH function_bounds AS (
    SELECT file_path, name, start_line, end_line
    FROM read_ast('src/**/*.py')
    WHERE is_function_definition(semantic_type)
),
-- ... more CTEs ...
SELECT ...
FROM function_bounds f
JOIN read_ast('src/**/*.py') cf
  ON cf.file_path = f.file_path
  AND cf.start_line >= f.start_line
  AND cf.end_line <= f.end_line
  AND is_control_flow(cf.semantic_type)
-- ...
```

These patterns are powerful but require understanding the "nodes within function" join pattern. Macros would make structural analysis accessible to more users.

## Proposed Macros

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

## Implementation Notes

1. **Performance:** These macros will perform multiple passes over the AST. Consider:
   - Caching intermediate results
   - Using materialized CTEs
   - Parallel execution where possible

2. **Language awareness:** Some metrics (like error handling) are language-specific:
   - Python: `try_statement`
   - JavaScript: `try_statement`
   - Go: explicit error returns (different pattern)
   - Rust: `Result` types (different pattern)

3. **Incremental approach:** Start with `ast_function_metrics` as it's the most generally useful, then add others based on user feedback.

## Alternatives Considered

1. **C++ table functions:** More performant but harder to maintain
2. **View-based approach:** Pre-compute metrics on load
3. **External tooling:** Delegate to existing static analysis tools

## Related

- Cookbook: `docs/guide/cookbook.md` - Contains the verbose versions of these patterns
- Semantic predicates: `is_function_definition()`, `is_control_flow()`, etc.

## Open Questions

1. Should metrics be computed lazily or eagerly?
2. How to handle cross-file analysis (function called in different file)?
3. Should we support custom metric definitions?
