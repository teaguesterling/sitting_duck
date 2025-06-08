# Segfault Analysis

## Summary
The extension has intermittent segfaults when using multiple `read_ast()` calls in a single query with complex JOINs.

## Reproducer Pattern
**Safe**: Single `read_ast()` call or simple JOINs
**Unsafe**: Multiple `read_ast()` calls with LEFT JOINs

```sql
-- This works fine:
SELECT COUNT(*) FROM read_ast('file.cpp') WHERE type = 'function_definition';

-- This segfaults:
WITH func_defs AS (SELECT node_id FROM read_ast('file.cpp') WHERE type = 'function_definition')
SELECT COUNT(*) FROM func_defs fd
JOIN read_ast('file.cpp') d ON d.parent_id = fd.node_id
LEFT JOIN read_ast('file.cpp') gc ON gc.parent_id = d.node_id
LEFT JOIN read_ast('file.cpp') pc ON pc.parent_id = d.node_id;
```

## Root Cause Hypothesis
Multiple calls to `read_ast()` in the same query may have:
1. **State management issues**: Table function state not properly isolated
2. **Memory management**: Smart pointer lifecycle issues
3. **Parser reuse**: Tree-sitter parser state conflicts

## Current Status
- ✅ Basic functionality works
- ✅ Simple JOINs work  
- ✅ Sequential queries work
- ❌ Complex multi-JOIN queries with multiple `read_ast()` calls segfault

## Workaround
Use single `read_ast()` call and materialize to CTE:
```sql
-- Safe pattern:
WITH ast_data AS (SELECT * FROM read_ast('file.cpp'))
SELECT ... FROM ast_data a1 JOIN ast_data a2 ON ...
```

## Next Steps
1. Investigate table function state management
2. Check smart pointer usage in `UnifiedASTBackend`
3. Verify tree-sitter parser cleanup
4. Add table function caching to avoid multiple parses