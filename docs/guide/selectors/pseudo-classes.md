# Pseudo-Classes Reference

All pseudo-classes supported by `ast_select`, organized by category. Pseudo-classes compose freely — chain as many as you need on a single selector.

## Containment

### `:has()` — Contains Descendant

```sql
-- Functions containing a return statement
SELECT name FROM ast_select('src/*.py', '.func:has(return_statement)');

-- Functions that call execute()
SELECT name FROM ast_select('src/*.py', '.func:has(.call#execute)');
```

### `:not(:has())` — Does Not Contain

```sql
-- Functions without a return statement
SELECT name FROM ast_select('src/*.py', '.func:not(:has(return_statement))');

-- Functions that never call execute()
SELECT name FROM ast_select('src/*.py', '.func:not(:has(.call#execute))');
```

## Positional

### `:first-child` / `:last-child`

```sql
-- First function definition among its siblings
SELECT name FROM ast_select('src/*.py', 'function_definition:first-child');

-- Last method in a class
SELECT name FROM ast_select('src/*.js', 'method_definition:last-child');
```

### `:nth-child(n)`

1-based position among siblings:

```sql
-- Second function definition
SELECT name FROM ast_select('src/*.py', 'function_definition:nth-child(2)');
```

### `:empty`

Nodes with no children (leaf nodes):

```sql
-- Empty blocks (pass-only functions, empty classes)
SELECT name FROM ast_select('src/*.py', 'block:empty');
```

### `:root`

The top-level node (module/program):

```sql
-- The module node
SELECT type FROM ast_select('src/*.py', ':root');
```

## Structural

### `:named`

Nodes with a non-empty `name` field. Filters out the many anonymous structural nodes:

```sql
-- Only named function definitions (excludes unnamed wrappers)
SELECT name, start_line FROM ast_select('src/*.py', '.func:named');
```

### `:syntax`

Syntax-only tokens (keywords, punctuation). Useful for distinguishing keywords from their parent constructs:

```sql
-- Just the `if` keyword tokens (not if_statement)
SELECT * FROM ast_select('src/*.py', 'if:syntax');

-- if_statement constructs (not keywords) — use :not(:syntax) or exact type
SELECT * FROM ast_select('src/*.py', 'if:not(:syntax)');
```

### `:definition` / `:reference` / `:declaration`

Query by NAME_ROLE flag:

```sql
-- All name-binding sites (functions, classes, variables, parameters)
SELECT name, type FROM ast_select('src/*.py', ':definition');

-- All name references (identifiers that use a name)
SELECT name, type FROM ast_select('src/*.py', 'identifier:reference');

-- Forward declarations only (C++ prototypes, TS signatures)
SELECT name FROM ast_select('src/*.cpp', ':declaration');
```

## Scope

### `:scope` (bare) — Is a Scope Boundary

```sql
-- All scope-creating nodes (functions, classes, loops, module)
SELECT type, name FROM ast_select('src/*.py', ':scope');
```

### `:scope(type)` — Within Nearest Ancestor Scope

The most powerful pseudo-class. Matches nodes within the nearest ancestor of the given type, **excluding subtrees of nested ancestors of the same type**.

This solves the nested function problem:

```sql
-- Return statements within their DIRECT enclosing function
-- (not returns in nested inner functions)
SELECT peek, start_line
FROM ast_select('src/*.py', 'return_statement:scope(function)');

-- Calls within the nearest class (not from nested classes)
SELECT name FROM ast_select('src/*.py', '.call:scope(class)');
```

Without `:scope()`, `ast_has` reports `outer_function` as containing `execute()` even when the call is inside a nested `inner_function`. With `:scope(function)`, only the direct enclosing function matches.

### Scope Columns

Every node in `read_ast` output carries a `scope` STRUCT:

```
scope: STRUCT<
    current   BIGINT,   -- nearest enclosing scope's node_id (-1/NULL at root)
    function  BIGINT,   -- nearest function ancestor's node_id
    class     BIGINT,   -- nearest class/struct/trait ancestor
    module    BIGINT,   -- nearest module/namespace ancestor
    stack     LIST<STRUCT<id BIGINT, kind SEMANTIC_TYPE>>
                        -- scope nodes only: full ancestor chain with kinds
>
```

`scope.current` replaces the pre-v1.7.4 `scope_id` column. The
`function`/`class`/`module` shortcuts are the big win: they let you ask
"what function is this in?" as a single struct-field read instead of a
range join against the AST. `scope.stack` replaces the old
`scope_stack` (now typed — each entry carries its semantic kind).

```sql
-- Scope chain for a class method:
SELECT name, type, scope.current, list_transform(scope.stack, s -> s.id)
FROM read_ast('src/*.py')
WHERE type IN ('module', 'class_definition', 'function_definition')
ORDER BY node_id LIMIT 5;
-- module:     scope.current=NULL, stack=[0]
-- Config:     scope.current=0,    stack=[0, 32]
-- __init__:   scope.current=32,   stack=[0, 32, 42]

-- "who calls ExecuteRecursivePipelines?" — O(1) per candidate
SELECT DISTINCT caller.name
FROM read_ast('src/**/*.cpp') call_site
JOIN read_ast('src/**/*.cpp') caller
  ON caller.node_id = call_site.scope.function
 AND caller.file_path = call_site.file_path
WHERE call_site.semantic_type = 'COMPUTATION_CALL'
  AND call_site.name = 'ExecuteRecursivePipelines';
```

To get any node's full scope chain when it isn't a scope node itself:
look up its `scope.current` → read that node's `scope.stack`.

### Scope Resolution Macros

Three macros build on `scope.current`, `scope.function`, and `scope.stack`
for name resolution:

**`ast_exports(source)`** — module-level public definitions:
```sql
SELECT name, type FROM ast_exports('src/**/*.py');
```

**`ast_imports(source)`** — imported names with source module hints:
```sql
SELECT source_module, imported_name FROM ast_imports('src/*.py');
```

**`ast_resolve(source)`** — reference → definition binding via scope chain walk:
```sql
-- For each reference, find which definition it binds to
SELECT ref_name, ref_line, def_line, def_type, scope_hops
FROM ast_resolve('src/main.py');
```

Cross-file resolution: JOIN `ast_imports` with `ast_exports` to resolve imports:
```sql
SELECT im.imported_name, ex.file_path as resolved_file, ex.type
FROM ast_imports('src/app.py') im
JOIN ast_exports('src/**/*.py') ex
  ON ex.name = im.imported_name
  AND ex.file_path LIKE '%' || im.source_module || '%';
```

## Call Graph

### `:calls(name)` — Scope Contains a Call

Matches nodes whose scope contains a call to `name`. Unlike `:has(.call#name)`, this uses scope resolution to avoid matching calls in nested functions.

```sql
-- Functions that call execute (direct scope only)
SELECT name FROM ast_select('src/**/*.py', '.func:calls(execute)');

-- Classes that call validate
SELECT name FROM ast_select('src/**/*.py', '.class:calls(validate)');
```

### `:called-by(name)` — Call Inside Function

Matches call nodes that are inside the function `name`:

```sql
-- All calls made by main()
SELECT name, start_line FROM ast_select('src/**/*.py', '.call:called-by(main)');

-- Database calls inside process_request
SELECT name FROM ast_select('src/**/*.py', '.call:called-by(process_request)');
```

### `:is-called` — Function Is Called

Matches function definitions that are called somewhere in the file:

```sql
-- Functions that are actually called
SELECT name FROM ast_select('src/**/*.py', '.func:is-called');

-- Unused functions (defined but never called)
SELECT name FROM ast_select('src/**/*.py', '.func:not(:is-called)');
```

### `:is-referenced` — Definition Is Referenced

Matches definitions that are referenced somewhere:

```sql
-- Variables that are actually used
SELECT name FROM ast_select('src/**/*.py', '.var:is-referenced');

-- Dead code: defined but never referenced
SELECT name FROM ast_select('src/**/*.py', '.func:not(:is-referenced)');
```

### `:exported` — Module-Level Public Definition

Matches definitions at module scope that are part of the public API:

```sql
-- Public API surface
SELECT name, type FROM ast_select('src/**/*.py', ':exported');

-- Exported but never referenced internally
SELECT name FROM ast_select('src/**/*.py', ':exported:not(:is-referenced)');
```

### Pseudo-Elements (`::` — Navigation)

Pseudo-elements return *different* nodes rather than filtering. They navigate FROM matched nodes to related nodes.

#### Tree Navigation

```sql
-- Parent node
SELECT * FROM ast_select('src/*.py', 'return_statement::parent');

-- Enclosing scope
SELECT * FROM ast_select('src/*.py', '.func#inner::scope');

-- Nearest enclosing definition (function, class, variable)
SELECT * FROM ast_select('src/*.py', 'return_statement::parent-definition');

-- Adjacent siblings
SELECT * FROM ast_select('src/*.py', '.func#validate::next-sibling');
SELECT * FROM ast_select('src/*.py', '.func#validate::prev-sibling');
-- ::previous-sibling is accepted as an alias for ::prev-sibling
```

#### Call Graph Navigation

```sql
-- Functions that call this function
SELECT name FROM ast_select('src/*.py', '.func#get_user::callers');

-- What this function calls
SELECT name FROM ast_select('src/*.py', '.func#main::callees');
```

### Pseudo-Element Quick Reference

| Pseudo-element | Returns | Cardinality |
|---|---|---|
| `::parent` | Parent node | 1 |
| `::scope` | Enclosing scope node | 1 |
| `::parent-definition` | Nearest enclosing definition | 1 |
| `::next-sibling` | Next sibling | 1 |
| `::prev-sibling` | Previous sibling | 1 |
| `::callers` | Functions that call this | N |
| `::callees` | Functions this calls | N |

## Ordering

### `:precedes(type)` — Before a Sibling

```sql
-- Comments that appear before function definitions
SELECT peek FROM ast_select('src/*.py', 'comment:precedes(function_definition)');

-- Import statements before class definitions
SELECT name FROM ast_select('src/*.py', 'import:precedes(class)');
```

### `:follows(type)` — After a Sibling

```sql
-- Functions defined after the last class
SELECT name FROM ast_select('src/*.py', 'function_definition:follows(class)');

-- Statements after imports (module-level constants, etc.)
SELECT peek FROM ast_select('src/*.py', 'expression_statement:follows(import)');
```

These provide the reverse direction that CSS combinators (`~`, `+`) can't express. `A ~ B` returns B; `:precedes(B)` returns the A nodes.

## Modifiers

```sql
-- Async functions
SELECT name FROM ast_select('src/*.py', '.func:async');

-- Static methods
SELECT name FROM ast_select('src/*.java', '.func:static');

-- Abstract classes
SELECT name FROM ast_select('src/*.java', '.class:abstract');

-- Const/final variables
SELECT name FROM ast_select('src/*.js', '.var:const');

-- Access modifiers
SELECT name FROM ast_select('src/*.java', '.func:public');
SELECT name FROM ast_select('src/*.java', '.func:private');
SELECT name FROM ast_select('src/*.java', '.func:protected');
```

## Annotations

```sql
-- Decorated functions (have any annotation/decorator)
SELECT name FROM ast_select('src/*.py', '.func:decorated');

-- Functions with type annotations
SELECT name FROM ast_select('src/*.py', '.func:typed');

-- Functions without a return type (void/None)
SELECT name FROM ast_select('src/*.py', '.func:void');

-- Functions with variadic parameters (*args, **kwargs, ...rest)
SELECT name FROM ast_select('src/*.py', '.func:variadic');
```

## Pattern Matching

Two pseudo-classes parse their argument as real code and compare it structurally to the AST. They differ in *what* they compare against:

- **`:match("code")`** — the **current node** is the root of the parsed pattern. Strict: the target's type must equal the pattern root's type.
- **`:contains("code")`** — **some descendant** of the current node is the root of the parsed pattern. Equivalent to `:has(:match("code"))`.

### `:match("code")` — Current-Node Structural Match

Use `:match` when you know the type of node you're looking for and want to check it directly:

```sql
-- Find call expressions that are exactly db.execute() with no arguments
SELECT name FROM ast_select('src/*.py', 'call:match("db.execute()")');

-- Find return statements that return None
SELECT peek FROM ast_select('src/*.py', 'return_statement:match("return None")');
```

`.func:match("db.execute()")` returns **zero rows** — a function_definition is not a call node, so the types don't match. Use `:contains` for "function contains X".

### `:contains("code")` — Subtree Structural Match

Use `:contains` when you want to find a pattern *anywhere inside* a larger node:

```sql
-- Functions that contain a db.execute() call somewhere in their body
SELECT name FROM ast_select('src/*.py', '.func:contains("db.execute()")');

-- Functions containing a specific return pattern
SELECT name FROM ast_select('src/*.py', '.func:contains("return None")');

-- Classes that contain a self.db assignment anywhere inside
SELECT name FROM ast_select('src/*.py', '.class:contains("self.db = ___")');
```

### Wildcards and Semantics

Both pseudo-classes share the same pattern parser. Use `___` (triple underscore) as a wildcard for "any name":

```sql
-- Match any assignment to self.<something>
SELECT name FROM ast_select('src/*.py', '.func:contains("self.___ = ___")');
```

Both use DFS pre-order contiguity — a subtree is a contiguous slice of the node array, so structural matching becomes array substring matching. No recursive tree traversal. `:match` is a direct lookup on the current node; `:contains` scans descendants.

### One Pattern per Selector

Only one `:match` or `:contains` is supported per selector. To combine multiple patterns, chain `ast_select` calls via a CTE:

```sql
WITH execute_callers AS (
    SELECT * FROM ast_select('src/*.py', '.func:contains("db.execute()")')
)
SELECT f.name FROM execute_callers f
WHERE EXISTS (
    SELECT 1 FROM ast_select('src/*.py', '.func:contains("return ___")') r
    WHERE r.file_path = f.file_path AND r.node_id = f.node_id
);
```

## Quick Reference

| Pseudo-class | Meaning |
|---|---|
| **Containment** | |
| `:has(sel)` | Contains descendant matching sel |
| `:not(:has(sel))` | Does NOT contain descendant |
| `:match("code")` | Current node IS the parsed pattern root (direct match) |
| `:contains("code")` | Some descendant IS the parsed pattern root (subtree match) |
| **Positional** | |
| `:first-child` | First among siblings |
| `:last-child` | Last among siblings |
| `:nth-child(n)` | Nth sibling (1-based) |
| `:empty` | No children |
| `:root` | Top-level node (depth 0) |
| **Structural** | |
| `:named` | Has a non-empty name |
| `:syntax` | Syntax-only token (keyword, punctuation) |
| `:definition` | Introduces a name with implementation |
| `:reference` | Uses a name |
| `:declaration` | Introduces a name without implementation |
| **Scope** | |
| `:scope` | Is a scope boundary |
| `:scope(type)` | Within nearest ancestor of type (scope-aware) |
| **Call Graph** | |
| `:calls(name)` | Scope contains a call to name |
| `:called-by(name)` | This call is inside function name |
| `:is-called` | Function is called somewhere |
| `:is-referenced` | Definition is referenced somewhere |
| `:exported` | Module-level public definition |
| **Ordering** | |
| `:precedes(type)` | Before a sibling of type |
| `:follows(type)` | After a sibling of type |
| **Modifiers** | |
| `:async` | Has async modifier |
| `:static` | Has static modifier |
| `:abstract` | Has abstract modifier |
| `:const` | Has const/final modifier |
| `:public` / `:private` / `:protected` | Access modifiers |
| **Annotations** | |
| `:decorated` | Has decorators/annotations |
| `:typed` | Has type annotation/signature |
| `:void` | No return type |
| `:variadic` | Has variadic parameters (*args, ...rest) |

---

## See Also

- [CSS Selectors Overview](index.md) — Combinators, compound selectors, API reference
- [Node Type Selectors](node-types.md) — Three tiers of type specificity
- [Attribute Selectors](attributes.md) — Query by name, modifier, annotation, and more
- [Semantic Type Aliases](kinds-types-and-classes.md) — Full alias table for `.semantic` selectors
