# Call Graph Analysis

Build call graphs — who calls what — using Sitting Duck's `scope.function` field and CSS pseudo-elements.

## Setup

Parse a small Python program with functions that call each other:

```sql
CREATE TABLE my_ast AS
SELECT * FROM parse_ast('
def validate(data):
    if not data:
        raise ValueError("empty")
    return True

def process(items):
    for item in items:
        if validate(item):
            transform(item)

def transform(item):
    result = item.upper()
    log(result)
    return result
', 'python');
```

Verify you can see the functions:

```sql
SELECT name, start_line
FROM my_ast
WHERE semantic_type = 'DEFINITION_FUNCTION';
```

```
┌──────────┬────────────┐
│   name   │ start_line │
├──────────┼────────────┤
│ validate │          2 │
│ process  │          7 │
│ transform│         12 │
└──────────┴────────────┘
```

## Finding who calls what

Every node has a `scope.function` field — the `node_id` of its nearest enclosing function. Join call nodes back to their enclosing function to build the call graph:

```sql
SELECT
    caller.name AS caller,
    c.name AS callee
FROM my_ast c
JOIN my_ast caller ON c.scope.function = caller.node_id
WHERE c.semantic_type = 'COMPUTATION_CALL'
  AND c.name IS NOT NULL
  AND caller.name IS NOT NULL
ORDER BY caller, callee;
```

```
┌──────────┬───────────┐
│  caller  │  callee   │
├──────────┼───────────┤
│ process  │ transform │
│ process  │ validate  │
│ transform│ log       │
└──────────┴───────────┘
```

This is a hash join — `scope.function` is a precomputed integer, so DuckDB matches callers in O(1) per row instead of scanning every function boundary.

## Using CSS pseudo-elements

The `::callers` and `::callees` pseudo-elements wrap this pattern in CSS selector syntax:

```sql
-- Who calls validate?
SELECT name
FROM ast_select_from('my_ast', '.func#validate::callers');
```

```
┌─────────┐
│  name   │
├─────────┤
│ process │
└─────────┘
```

```sql
-- What does process call?
SELECT name
FROM ast_select_from('my_ast', '.func#process::callees');
```

```
┌───────────┐
│   name    │
├───────────┤
│ validate  │
│ transform │
└───────────┘
```

Try it the other direction — what does `transform` call?

```sql
SELECT name FROM ast_select_from('my_ast', '.func#transform::callees');
-- → log
```

## Finding unused functions

Functions with no callers might be dead code. The `:is-called` pseudo-class tests whether any call site references a function's name:

```sql
SELECT name
FROM ast_select_from('my_ast', '.func:not(:is-called)');
```

```
┌─────────┐
│  name   │
├─────────┤
│ process │
└─────────┘
```

`process` has no callers in this snippet — it's the entry point (or dead code, depending on context).

## Adding call counts

For a weighted call graph, count how many times each pair appears:

```sql
SELECT
    caller.name AS caller,
    c.name AS callee,
    COUNT(*) AS times_called
FROM my_ast c
JOIN my_ast caller ON c.scope.function = caller.node_id
WHERE c.semantic_type = 'COMPUTATION_CALL'
  AND c.name IS NOT NULL
  AND caller.name IS NOT NULL
GROUP BY caller.name, c.name
ORDER BY times_called DESC;
```

## Scaling to real codebases

The same patterns work with `read_ast` on files:

```sql
CREATE TABLE codebase AS
SELECT * FROM read_ast('src/**/*.py', ignore_errors := true);

-- Top 20 most-called functions
SELECT
    c.name AS function_name,
    COUNT(*) AS call_count
FROM codebase c
WHERE c.semantic_type = 'COMPUTATION_CALL'
  AND c.name IS NOT NULL
GROUP BY c.name
ORDER BY call_count DESC
LIMIT 20;

-- Full call graph
SELECT
    caller.name AS caller,
    c.name AS callee,
    COUNT(*) AS calls
FROM codebase c
JOIN codebase caller ON c.scope.function = caller.node_id
WHERE c.semantic_type = 'COMPUTATION_CALL'
  AND c.name IS NOT NULL
  AND caller.name IS NOT NULL
GROUP BY caller.name, c.name
ORDER BY calls DESC;
```

Parse once into a table, then run as many call-graph queries as you want without re-parsing.

## Next steps

- [Scope System](../explanation/scope-system.md) — how `scope.function` is computed and why it's fast
- [CSS Pseudo-Classes](../reference/css-pseudo-classes.md) — `:is-called`, `::callers`, `::callees` reference
- [Common Queries](../how-to/common-queries.md) — more analysis patterns
