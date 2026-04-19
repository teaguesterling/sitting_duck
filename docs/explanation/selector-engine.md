# The CSS Selector Engine

## The self-hosting trick

`ast_select` parses CSS selectors using tree-sitter-css — the same parsing infrastructure Sitting Duck uses for source code. Your selector string becomes an AST, and that AST drives the SQL generation.

```sql
-- This selector...
'.func:has(.call#execute)'

-- ...parses into this AST (simplified):
-- class_selector
--   class_name: "func"
--   pseudo_class_selector
--     class_name: "has"
--     arguments
--       class_selector
--         class_name: "call"
--         id_selector
--           id_name: "execute"
```

No string parsing, no regex, no hand-written parser. The CSS grammar does the heavy lifting.

## Two entry points

```
ast_select(source, selector)
    │
    ├── read_ast(source)  ← parse files
    │
    └── ast_select_from(table, selector)  ← query the AST
```

- **`ast_select(source, selector)`** — parses source files, then queries. Convenient for one-off queries.
- **`ast_select_from(table, selector)`** — queries a pre-parsed AST table. Parse once, query many times.

Internally, `ast_select` calls `read_ast` then delegates to `ast_select_from`. If you're running multiple selectors against the same codebase, use `ast_select_from` to avoid re-parsing:

```sql
CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');
SELECT * FROM ast_select_from('my_ast', '.func:has(return_statement)');
SELECT * FROM ast_select_from('my_ast', '.class:has(.func#__init__)');
```

## How a selector becomes SQL

Walk through `.func:has(.call#execute)`:

**Step 1: Parse the selector.** The CSS string is parsed via `parse_ast_list_table(selector, 'css')`, producing an AST of the selector itself. This happens in the `sel` CTE.

**Step 2: Extract typed views.** The selector AST nodes are split into typed CTEs — one per node type the engine cares about:

```
sel_tag_names       ← type = 'tag_name'
sel_class_names     ← type = 'class_name'
sel_id_names        ← type = 'id_name'
sel_pseudo_classes  ← type = 'pseudo_class_selector'
sel_arg_blocks      ← type = 'arguments'
sel_attribute_*     ← attribute selector components
```

These typed views let downstream CTEs join different node types as separate relations, avoiding a DuckDB planner bug with self-correlating aliases.

**Step 3: Determine selector type.** The `sel_root` CTE finds the top-level selector node and classifies it: simple selector, combinator (descendant/child/sibling), or pseudo-element wrapper.

**Step 4: Dispatch to the right strategy.**

| Selector type | SQL pattern |
|--------------|-------------|
| Simple (type, class, id) | `WHERE` predicates on `semantic_type`, `type`, `name` |
| Combinators (`A B`, `A > B`, `A ~ B`, `A + B`) | Self-join using DFS range check |
| Pseudo-classes (`:has`, `:not`) | `EXISTS` subqueries |
| Pseudo-elements (`::callers`, `::callees`) | Post-filter navigation via `scope.function` |

**Step 5: Apply compound filters.** Multiple conditions on the same element (type + name + class + pseudo-class) compose as AND. The engine extracts each filter component from the selector AST and applies them uniformly.

## The DFS range check

Combinators rely on the DFS pre-order numbering to test subtree membership in O(1):

```sql
-- "B is a descendant of A" becomes:
WHERE b.node_id > a.node_id
  AND b.node_id <= a.node_id + a.descendant_count
```

This works because DFS pre-order numbering assigns consecutive IDs to subtrees. A node's descendants always have IDs in the range `(node_id, node_id + descendant_count]`.

Child selectors (`A > B`) add `b.parent_id = a.node_id`. Sibling selectors use `a.parent_id = b.parent_id` with ordering on `sibling_index`.

## The UNION ALL dispatch pattern

Early versions used a single `CASE` expression to dispatch by selector type:

```sql
-- OLD: single CASE (broken for performance)
SELECT * FROM ast WHERE
    CASE sel_root.type
        WHEN 'class_selector' THEN <simple match>
        WHEN 'descendant_selector' THEN <join match>
        WHEN 'child_selector' THEN <join match>
    END
```

This caused a critical performance bug. DuckDB's optimizer decorrelated the `EXISTS` subqueries in unused `CASE` branches into always-on hash joins. On DuckDB's own source tree (1.7M nodes), this produced a 28.8M-row hash join even for simple selectors that didn't use combinators.

The fix: restructure into `UNION ALL` of per-selector-type sub-queries, each guarded by `WHERE sel_root.type = 'X'`:

```sql
-- NEW: UNION ALL dispatch (correct)
SELECT * FROM simple_match
WHERE sel_root.type IN ('class_selector', 'id_selector', ...)

UNION ALL

SELECT * FROM descendant_match
WHERE sel_root.type = 'descendant_selector'

UNION ALL

SELECT * FROM child_match
WHERE sel_root.type = 'child_selector'
```

DuckDB evaluates only the branch whose guard matches, eliminating the phantom joins.

## Type resolution tiers

Selectors resolve types at three levels of specificity:

### 1. Semantic aliases (`.class` selectors)

```sql
-- .func matches DEFINITION_FUNCTION across all 27 languages
SELECT * FROM ast_select('src/**/*.*', '.func');
```

These use `SEMANTIC_TYPE` equality: `semantic_type = 'DEFINITION_FUNCTION'`. Cross-language, fast (single-byte comparison). ~80 aliases available — see [Semantic Aliases](../reference/semantic-aliases.md).

### 2. Bare keywords (prefix match)

```sql
-- "function" matches function_definition, function_declaration, function_item, etc.
SELECT * FROM ast_select('src/**/*.py', 'function');
```

Bare keywords match any tree-sitter `type` that starts with the keyword. Language-specific but more precise than semantic aliases.

### 3. Exact types

```sql
-- Exact tree-sitter node type
SELECT * FROM ast_select('src/**/*.py', 'function_definition');
```

Matches the tree-sitter `type` column exactly. Most specific, but tied to a single language's grammar.

## Performance characteristics

- **Parsing**: O(n) via tree-sitter, streamed in 2048-row chunks
- **Subtree membership**: O(1) via DFS range check
- **Scope lookups**: O(1) via `scope.function` hash join
- **Selector parsing**: negligible (CSS selectors are tiny)
- **Simple selectors**: single scan of the AST table
- **Combinators**: self-join (hash or merge, depending on DuckDB's planner)
- **Pseudo-classes**: correlated subquery per matched node

The bottleneck is almost always parsing, not querying. For repeated queries, parse once with `ast_select_from`.

## Why parse_ast_list_table exists

DuckDB table functions require literal arguments at bind time. But `ast_select_from` needs the selector to be a column reference — for features like `ast_select_rules` that dispatch one selector per rule via lateral join.

`parse_ast_list_table` is the escape hatch: it wraps the scalar `parse_ast_list()` inside a table macro. This lets the selector be a column expression at macro-expansion time while still producing a row-shaped result.

```
parse_ast(selector, 'css')         ← table function, literal only
parse_ast_list(selector, 'css')    ← scalar function, any expression
parse_ast_list_table(selector, 'css')  ← table macro wrapping the scalar
```

The `sel` CTE in `ast_select_from` uses `parse_ast_list_table` for exactly this reason.

## Extensibility via custom predicates

The pseudo-class dispatch has a fallback path: if a pseudo-class name isn't in the built-in list, the engine checks `duckdb_functions()` for a macro named `ast_selector_predicate_<name>`. If found, it calls `ast_dispatch_predicate(fn, node, arg)` — a shim macro that delegates to `apply()` from the [`func_apply`](https://community-extensions.duckdb.org/extensions/func_apply.html) extension.

This design keeps the selector engine itself unchanged while allowing arbitrary user-defined filters. The `func_apply` dependency is soft: if not installed, custom predicates are disabled with a clear error, but all built-in pseudo-classes continue to work.

The shim macro pattern solves a bind-time constraint: `apply()` fails at bind time if `func_apply` isn't loaded. By registering `ast_dispatch_predicate` conditionally at extension load time — as `apply(fn, node, arg)::BOOLEAN` when `func_apply` is available, `(false)` otherwise — the SQL macro body always compiles. The fallback `(false)` is never reached because the validation CTE raises an error first.

See [Custom Predicates](../reference/css-pseudo-classes.md#custom-predicates) for usage and examples.

## See also

- [CSS Selector Syntax](../reference/css-selectors.md) — full selector reference
- [Pseudo-Classes](../reference/css-pseudo-classes.md) — `:has`, `:not`, `:scope`, `:is-called`, custom predicates
- [Semantic Aliases](../reference/semantic-aliases.md) — the ~80 cross-language type aliases
- [Architecture](architecture.md) — how parsing and traversal fit together
- [Scope System](scope-system.md) — how `::callers`/`::callees` use scope.function
