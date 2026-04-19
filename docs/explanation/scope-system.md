# The Scope System

## The problem

In a flat AST table, answering "what function is this call inside?" requires a range-join:

```sql
SELECT f.name
FROM ast f
JOIN ast c ON c.node_id > f.node_id
              AND c.node_id <= f.node_id + f.descendant_count
WHERE f.semantic_type = 'DEFINITION_FUNCTION'
  AND c.semantic_type = 'COMPUTATION_CALL'
  AND c.name = 'execute';
```

This works, but it scans every function-call pair. On a large codebase, that's millions of comparisons for a question with a single-row answer.

Sitting Duck precomputes the answer during the DFS traversal that builds the AST table. Every node carries a `scope` STRUCT with the answer already filled in.

## The scope STRUCT

```
scope: STRUCT<
    current   BIGINT,   -- nearest enclosing scope's node_id
    function  BIGINT,   -- nearest enclosing function's node_id
    class     BIGINT,   -- nearest enclosing class/struct/trait
    module    BIGINT,   -- nearest enclosing module/namespace
    stack     LIST<STRUCT<id BIGINT, kind SEMANTIC_TYPE>>
                        -- scope nodes only: full ancestor chain
>
```

Every node gets `current`, `function`, `class`, and `module`. The `stack` field is populated only on nodes that create a new scope (those with `IS_SCOPE` in their flags).

## How it's computed

During the DFS traversal that produces one row per node:

1. A stack of scope-creating nodes is maintained, each tagged with its `SEMANTIC_TYPE`.
2. When a node has `IS_SCOPE` in its flags, it pushes a `{node_id, semantic_type}` entry onto the stack.
3. Every node reads the stack to fill its scope fields:
   - `scope.current` ← top of stack (nearest enclosing scope)
   - `scope.function` ← nearest entry where kind is `DEFINITION_FUNCTION`
   - `scope.class` ← nearest entry where kind is `DEFINITION_CLASS`
   - `scope.module` ← nearest entry where kind is `DEFINITION_MODULE`
4. When traversal leaves a node's subtree (tracked via `descendant_count`), its entry pops off the stack.

The cost is O(depth) per node — scanning up the stack for each shortcut. Since tree depth is typically under 30, this is negligible compared to parsing.

## Common patterns

### What function contains this node?

```sql
-- Direct field read — no join needed
SELECT name FROM ast
WHERE node_id = (
    SELECT scope.function FROM ast WHERE node_id = 42
);
```

### All module-level definitions

```sql
SELECT name, type FROM ast
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND (scope.current IS NULL OR scope.current <= 0);
```

Nodes at module level have no enclosing scope, so `scope.current` is NULL or the sentinel value.

### Call graph: who calls what?

```sql
SELECT
    caller.name AS caller,
    c.name AS callee,
    COUNT(*) AS call_count
FROM ast c
JOIN ast caller ON c.scope.function = caller.node_id
WHERE c.semantic_type = 'COMPUTATION_CALL'
  AND c.name IS NOT NULL
  AND caller.name IS NOT NULL
GROUP BY caller.name, c.name
ORDER BY call_count DESC;
```

This is a hash join on `scope.function = caller.node_id`. No range scans, no subtree containment checks.

### Calls inside try blocks

```sql
SELECT c.name, c.start_line
FROM ast c
WHERE c.semantic_type = 'COMPUTATION_CALL'
  AND list_filter(c.scope.stack, s -> s.kind = 'ERROR_TRY') != [];
```

The `scope.stack` gives you the full ancestor chain with semantic kinds, so you can filter by any scope type — try blocks, loops, conditionals, etc.

### Nested scope inspection

```sql
-- Functions inside classes (methods)
SELECT name, start_line
FROM ast
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND scope.class IS NOT NULL;

-- Functions NOT inside classes (free functions)
SELECT name, start_line
FROM ast
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND scope.class IS NULL;
```

## Why the shortcuts exist

Before `scope.function` existed, the call-graph query above required a range-join against every function definition. On DuckDB's source tree (~1.7M nodes), that query took **42 seconds**.

With `scope.function`, the same query takes **40ms** — a 1000x speedup. The difference: a hash join on a precomputed integer vs. a range scan over every function-call pair.

The same logic applies to `scope.class` and `scope.module`. These are the three questions that come up most often in code analysis:

| Shortcut | Question it answers |
|----------|-------------------|
| `scope.function` | "What function is this inside?" |
| `scope.class` | "What class is this inside?" |
| `scope.module` | "What module/namespace is this inside?" |

Each eliminates a range-join pattern that would otherwise dominate query time on large codebases.

## scope.stack vs shortcuts

The shortcuts answer the common questions instantly. The stack answers everything else.

**Use shortcuts when:**
- You need the nearest enclosing function/class/module
- You're building call graphs or class member queries
- Performance matters (hash join vs. list scan)

**Use scope.stack when:**
- You need the full ancestor chain: "is this inside a try block inside an async function?"
- You're filtering by scope kinds that don't have shortcuts (loops, try blocks, conditionals)
- You need to count nesting depth of a specific scope kind

```sql
-- How many nested loops is this node inside?
SELECT
    name,
    length(list_filter(scope.stack, s -> s.kind = 'CONTROL_LOOP')) AS loop_depth
FROM ast
WHERE semantic_type = 'COMPUTATION_CALL';
```

The stack is a `LIST<STRUCT<id BIGINT, kind SEMANTIC_TYPE>>` — outermost scope first, innermost last. It's only populated on scope-creating nodes to keep row size small for the majority of nodes that don't need it.

## How CSS selectors use scope

The CSS selector engine uses scope fields internally:

- **`:scope()`** pseudo-class — filters nodes by their enclosing scope type
- **`::callers`** pseudo-element — finds all functions that call the matched function, using `scope.function` to group calls by their enclosing function
- **`::callees`** pseudo-element — finds all functions called within the matched function, using the DFS range check scoped by `scope.function`
- **`:called`** pseudo-class — tests whether any call site references the matched function's name

```sql
-- These two are equivalent:
SELECT name FROM ast_select_from('my_ast', '.func#validate::callers');

SELECT DISTINCT caller.name
FROM ast caller
JOIN ast c ON c.scope.function = caller.node_id
WHERE c.semantic_type = 'COMPUTATION_CALL'
  AND c.name = 'validate';
```

The pseudo-element syntax is syntactic sugar over the scope-based hash join pattern.

## See also

- [Output Schema](../reference/output-schema.md) — column details and scope STRUCT definition
- [Architecture](architecture.md) — how parsing, traversal, and enrichment fit together
- [CSS Pseudo-Classes](../reference/css-pseudo-classes.md) — `:scope`, `:called`, and other scope-aware selectors
- [Selector Engine](selector-engine.md) — how `::callers`/`::callees` are implemented
