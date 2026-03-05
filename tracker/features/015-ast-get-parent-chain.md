# ast_get_parent_chain() - Navigate Scope Hierarchy

**Source**: Peer Review Feedback
**Priority**: P1 (High - Enables scope understanding)
**Status**: Implemented (as `ast_ancestors`)

## What's Implemented

### `ast_ancestors(ast_table, child_node_id)` (in `tree_navigation.sql`)
Recursive CTE macro that follows `parent_id` upward from any node to the root. Returns full AST rows for each ancestor.

```sql
-- Get all ancestors of a node
SELECT * FROM ast_ancestors('my_ast', 42);

-- Find the class containing a method
SELECT a.name, a.type
FROM ast_ancestors('my_ast', method_node_id) a
WHERE a.type = 'class_definition';
```

### Related macros already implemented
- `ast_children(ast_table, parent_node_id)` — immediate children
- `ast_descendants(ast_table, ancestor_node_id)` — entire subtree (O(1) via descendant_count)
- `ast_siblings(ast_table, target_node_id)` — same-parent nodes
- `ast_class_members(ast_table, class_node_id)` — direct members of a class
- `ast_function_scope(ast_table, func_node_id)` — function body excluding nested functions

## Remaining Gaps

- **Scope path convenience**: No `ast_scope_path(ast_table, node_id)` that returns just the chain of named ancestors as a list (e.g., `['module', 'ClassName', 'method']`)
- **Qualified name**: No `ast_qualified_name(ast_table, node_id)` that returns `module.ClassName.method`
- **Max depth parameter**: The original design included `max_depth` limiting — current `ast_ancestors` always goes to root
- **Method chaining**: `.get_parent_chain()` syntax not yet available (depends on #023)

## Implementation History

- `ast_ancestors` macro — implemented in tree_navigation.sql
- Full tree navigation suite (13 macros) — implemented
