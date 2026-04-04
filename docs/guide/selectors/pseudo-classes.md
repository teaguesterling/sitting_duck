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

### `:matches("code")` — Structural Substring Match

The most expressive pseudo-class. Parses the argument as real code and checks if that structure appears as a contiguous subtree within the matched node.

```sql
-- Functions containing a db.execute() call (structural, not text search)
SELECT name FROM ast_select('src/*.py', '.func:matches("db.execute()")');

-- Functions containing a specific return pattern
SELECT name FROM ast_select('src/*.py', '.func:matches("return None")');

-- Classes containing self.db assignment
SELECT name FROM ast_select('src/*.py', '.class:matches("self.db = ___")');
```

`:matches()` uses DFS pre-order contiguity — a subtree is a contiguous slice of the node array, so structural matching becomes array substring matching. No tree traversal needed.

Use `___` (triple underscore) as a wildcard for "any name":

```sql
-- Match any assignment to self.anything
SELECT name FROM ast_select('src/*.py', '.func:matches("self.___ = ___")');
```

## Quick Reference

| Pseudo-class | Meaning |
|---|---|
| **Containment** | |
| `:has(sel)` | Contains descendant matching sel |
| `:not(:has(sel))` | Does NOT contain descendant |
| `:matches("code")` | Contains structural code pattern (substring match) |
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
