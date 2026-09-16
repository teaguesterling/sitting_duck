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

`:scope` and `:in-scope` are complementary, mirroring CSS where `:scope` is the
reference element **itself**, never its descendants:

- **`:scope`** — the node **is** a scope boundary.
- **`:in-scope(...)`** — the node is **contained within** a scope.

Both take the same argument: a `function` / `class` / `module` keyword, a semantic
class (`.fn`, `.cls`, `.mod`, and their aliases), or a bare tree-sitter node type;
the `.class` form also accepts a `#name` filter.

### `:scope` — Is a Scope Boundary

```sql
-- All scope-creating nodes (functions, classes, module, …)
SELECT type, name FROM ast_select('src/*.py', ':scope');

-- Only class scopes; only the scope named Config
SELECT name FROM ast_select('src/*.py', ':scope(class)');
SELECT name FROM ast_select('src/*.py', ':scope(.class#Config)');
```

### `:in-scope(type)` — Contained Within Nearest Scope

The most powerful pseudo-class. Matches nodes contained within the nearest scope of
the given kind. For `function` / `class` / `module` it reads the precomputed
`scope.*` struct; for a bare tree-sitter type it walks to the nearest ancestor of
that type, **excluding subtrees of nested ancestors of the same type**.

This solves the nested function problem:

```sql
-- Return statements within their DIRECT enclosing function
-- (not returns in nested inner functions)
SELECT peek, start_line
FROM ast_select('src/*.py', 'return_statement:in-scope(function_definition)');

-- Variables inside the function named `load`  (the workhorse form)
SELECT name FROM ast_select('src/*.py', '.var:in-scope(.fn#load)');
```

Without `:in-scope()`, `ast_has` reports `outer_function` as containing `execute()`
even when the call is inside a nested `inner_function`. With `:in-scope(function)`,
only the direct enclosing function matches.

> **Note:** a `#name` filter must use the semantic-class form — `:in-scope(.fn#load)`,
> not `:in-scope(function#load)`. The CSS grammar lumps `function#load` into a single
> token it cannot split, so that form is rejected with an error. Arbitrary semantic
> scopes (e.g. `:in-scope(.loop)`) and full nested selectors are deferred to #145.

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

### `:calls(name)` — Function Directly Calls a Name

Matches a **function** whose own body directly calls `name`. This is **direct** (immediate scope): a call inside a nested function or lambda belongs to *that* inner scope, not to the outer function, so it does not match here. Resolved through the precomputed `scope.function` edge (bounded — see note below), unlike `:has(.call#name)`, which is a subtree match that would include nested calls.

```sql
-- Functions that directly call execute (calls inside nested lambdas excluded)
SELECT name FROM ast_select('src/**/*.py', '.func:calls(execute)');
```

### `:called-by(name)` — Immediate Enclosing Function Is `name`

Matches nodes (typically calls) whose **immediate** enclosing function is named `name`. **Direct/lambda-aware**: a call inside a lambda inside `F` is called-by the *lambda*, not `F` — the call belongs to the nearest function scope, which a lambda is.

```sql
-- Calls whose immediate enclosing function is main() (not calls nested in a lambda inside main)
SELECT name, start_line FROM ast_select('src/**/*.py', '.call:called-by(main)');

-- Database calls immediately inside process_request
SELECT name FROM ast_select('src/**/*.py', '.call:called-by(process_request)');
```

> **Note (name-matched, not resolved).** `:calls`/`:called-by` match on the call's *name*, not a resolved call target — same-named functions across files are not disambiguated (that is the resolver's job). Both are evaluated through the bounded `scope.function` equi-join, which is what keeps them from exhausting memory on large tables.

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

## Custom Predicates

Define your own pseudo-classes by registering macros with the `ast_selector_predicate_<name>` naming convention. Once registered, use them in selectors as `:<name>` or `:<name>("arg")`.

### Setup

Dynamic custom predicates require the [`func_apply`](https://community-extensions.duckdb.org/extensions/func_apply.html) extension for runtime dispatch:

```sql
INSTALL func_apply FROM community;
PRAGMA sitting_duck_enable_dynamic_predicates;
```

The pragma loads `func_apply` and replaces the internal dispatch stub with a real `apply()`-based dispatcher. If `func_apply` isn't installed, the pragma raises a clear error with next steps.

### Defining a Predicate

A predicate macro takes two arguments: `node` (the AST row as a struct) and `arg` (the string argument from the selector, or `NULL` if none):

```sql
-- Predicate with an argument: :name_starts("prefix")
CREATE MACRO ast_selector_predicate_name_starts(node, prefix) AS (
    node.name IS NOT NULL AND starts_with(node.name, prefix)
);

-- Predicate with no argument: :is_deep
CREATE MACRO ast_selector_predicate_is_deep(node, arg) AS (
    node.depth >= 3
);

-- Predicate using semantic types: :is_test
CREATE MACRO ast_selector_predicate_is_test(node, arg) AS (
    node.name IS NOT NULL AND (
        starts_with(node.name, 'test_')
        OR starts_with(node.name, 'Test')
    )
);
```

The `node` struct contains all columns from `read_ast()` — `name`, `type`, `depth`, `semantic_type`, `peek`, `start_line`, `scope`, etc. Use any column to build your predicate logic.

### Using Custom Predicates

Custom predicates work exactly like built-in pseudo-classes:

```sql
-- With an argument
SELECT name FROM ast_select('src/*.py', '.func:name_starts("test_")');

-- Without an argument
SELECT name FROM ast_select('src/*.py', 'function_definition:is_deep');

-- Negation
SELECT name FROM ast_select('src/*.py', '.func:not(:is_test)');

-- Combined with built-in pseudo-classes
SELECT name FROM ast_select('src/*.py', '.func:named:is_deep');
SELECT name FROM ast_select('src/*.py', '.func:is_test:has(return_statement)');
```

### Discoverability

All registered predicates are discoverable via DuckDB's function catalog:

```sql
SELECT function_name
FROM duckdb_functions()
WHERE function_name LIKE 'ast_selector_predicate_%';
```

### Example: FTS Predicate

Combine with DuckDB's full-text search to find nodes by content:

```sql
-- Build a text index on the AST
CREATE TABLE code AS SELECT * FROM read_ast('src/**/*.py');
PRAGMA create_fts_index('code', 'node_id', 'peek');

-- Predicate that checks FTS relevance
CREATE MACRO ast_selector_predicate_mentions(node, term) AS (
    node.peek IS NOT NULL AND node.peek ILIKE '%' || term || '%'
);

-- Functions mentioning "database"
SELECT name FROM ast_select('src/**/*.py', '.func:mentions("database")');
```

### Error Messages

If you use a custom pseudo-class without the required setup, you'll get targeted guidance:

- **No `func_apply` installed**: suggests `INSTALL func_apply FROM community;` then `PRAGMA sitting_duck_enable_dynamic_predicates;`
- **`func_apply` loaded but no matching macro**: reports the specific macro name that's missing

### Advanced: Custom Dispatch Without `func_apply`

Under the hood, custom predicates are dispatched through the `ast_dispatch_predicate(fn, node, arg)` macro. When `func_apply` is loaded, Sitting Duck registers this as a call to `apply()`. Without `func_apply`, it's a no-op stub.

You can replace this macro with your own dispatcher — no `func_apply` required:

```sql
CREATE OR REPLACE MACRO ast_dispatch_predicate(fn, node, arg) AS (
    CASE fn
        WHEN 'ast_selector_predicate_is_test'
            THEN node.name IS NOT NULL AND starts_with(node.name, 'test_')
        WHEN 'ast_selector_predicate_mentions'
            THEN node.peek IS NOT NULL AND node.peek ILIKE '%' || arg || '%'
        ELSE false
    END
);
```

This is useful in environments where you can't install community extensions, or when you have a fixed set of predicates and want to avoid the `func_apply` dependency.

---

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
| `:scope` | Is a scope boundary (optionally of a kind/name) |
| `:in-scope(type)` | Contained within nearest scope of type (scope-aware) |
| **Call Graph** | |
| `:calls(name)` | Function directly calls name (immediate scope; nested lambdas excluded) |
| `:called-by(name)` | Immediate enclosing function is name (a lambda counts as the function) |
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
| **Custom** | |
| `:<name>` | User-defined predicate (requires `func_apply`) |
| `:<name>("arg")` | User-defined predicate with argument |

---

## See Also

- [CSS Selectors Overview](css-selectors.md) — Combinators, compound selectors, API reference
- [Node Type Selectors](node-type-selectors.md) — Three tiers of type specificity
- [Attribute Selectors](css-attributes.md) — Query by name, modifier, annotation, and more
- [Semantic Type Aliases](semantic-aliases.md) — Full alias table for `.semantic` selectors
