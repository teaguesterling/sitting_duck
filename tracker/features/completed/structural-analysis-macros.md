# Feature: Structural Analysis SQL Macros

**Priority:** Medium
**Complexity:** Medium
**Status:** ✅ Implemented

## Implementation Summary

All proposed macros have been implemented as SQL table macros in `src/sql_macros/tree_navigation.sql`.

### Implemented Macros (13 total)

#### Tree Navigation Primitives
| Macro | Description | Commit |
|-------|-------------|--------|
| `ast_children(table, node_id)` | Direct children of a node | 35feabc |
| `ast_descendants(table, node_id)` | All descendants (subtree) | 35feabc |
| `ast_ancestors(table, node_id)` | Path from node to root | 35feabc |
| `ast_siblings(table, node_id)` | Same-parent nodes | 35feabc |
| `ast_containing_line(table, line)` | Nodes spanning a line | 35feabc |
| `ast_in_range(table, start, end)` | Nodes in line range | 35feabc |

#### Scope-Aware Helpers
| Macro | Description | Commit |
|-------|-------------|--------|
| `ast_function_scope(table, node_id)` | Function body excluding nested functions | 5aa4a78 |
| `ast_class_members(table, node_id)` | Direct class members | 3d6675d |

#### Analysis Macros
| Macro | Description | Commit |
|-------|-------------|--------|
| `ast_function_metrics(table)` | Cyclomatic complexity, returns, loops | b350d18 |
| `ast_functions_containing(table, type)` | Find functions with specific patterns | 45c3de9 |
| `ast_nesting_analysis(table)` | Depth analysis per function | b15c5c5 |
| `ast_security_audit(table)` | Security anti-pattern detection | adab8ca |
| `ast_dead_code(table)` | Unused code detection | f6d2240 |

### Documentation

- **API Reference:** `docs/api/structural-analysis.md`
- **Cookbook:** Updated `docs/guide/cookbook.md` with macro examples
- **Tests:** `test/sql/tree_navigation_macros.test` (145+ assertions)

### Key Design Decisions

1. **Table name as string parameter:** All macros accept table name as string (not subquery) for composability with `query_table()`.

2. **Scope isolation:** `ast_function_scope` and analysis macros correctly exclude nested function bodies to avoid double-counting.

3. **O(1) subtree queries:** Leverages `descendant_count` and DFS pre-order `node_id` for efficient range-based subtree queries.

4. **SQL-only implementation:** All macros are pure SQL using CTEs and `query_table()`. No C++ required.

---

## Original Proposal

(Preserved below for reference)

### Motivation

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

### Success Criteria ✅

A user can now write:

```sql
-- Before: 20+ lines of CTEs and joins
-- After: Simple, readable query
CREATE TABLE codebase AS SELECT * FROM read_ast('src/**/*.py');

SELECT name, cyclomatic, max_depth
FROM ast_function_metrics('codebase')
WHERE cyclomatic > 10
ORDER BY cyclomatic DESC;
```

### Open Questions (Resolved)

1. ✅ **Should primitives be SQL macros or C++ functions?** → SQL macros work well
2. ⏳ **How to handle cross-file analysis (call graphs)?** → Future work
3. ⏳ **Should we support user-defined metrics?** → Future work
4. ⏳ **How to efficiently cache repeated traversals?** → Use temp tables
5. ✅ **Should `ast_descendants` exclude the root node?** → Yes, excludes root
