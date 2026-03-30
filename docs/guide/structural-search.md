# Structural Search Manual

A practical guide to finding code structures with `ast_match`, relational operators, and pattern wildcards.

Structural search lets you find code by describing **what it looks like**, not by writing queries against AST node types. Instead of asking "find nodes where type = 'call' and name = 'eval'", you write the code pattern you're looking for: `eval(__X__)`.

## Getting Started

### Your First Pattern

```sql
-- Find all calls to print() in a file
SELECT file_path, start_line, peek
FROM ast_match('src/main.py', 'print(__X__)');
```

`__X__` is a **wildcard** — it matches any single expression and captures it as `X`. The rest of the pattern (`print(...)`) is literal structure that must match exactly.

### Patterns Match Structure, Not Text

A pattern like `print(__X__)` doesn't grep for the string "print" — it parses the pattern as actual code, builds an AST, and matches that AST structure against the source AST. This means:

- `print(__X__)` matches `print(42)`, `print(foo.bar)`, `print(a + b)` — any single-argument call to `print`
- It does NOT match `print(a, b)` — that has two arguments, but the pattern has one
- It does NOT match `my_print(x)` — the function name must be `print`

### Globs Work Too

```sql
-- Search across all Python files
SELECT file_path, start_line, peek
FROM ast_match('src/**/*.py', 'print(__X__)');
```

---

## Wildcards

### Simple Wildcards

| Pattern | Meaning |
|---------|---------|
| `__X__` | Named wildcard — matches one node, captures it as `X` |
| `__` | Anonymous wildcard — matches one node, no capture |

Wildcards use UPPERCASE names to avoid conflicting with Python's `__dunder__` methods. `__init__` is lowercase, so it's treated as literal code, not a wildcard.

```sql
-- Capture both function name and argument
SELECT
    captures['F'][1].name as func_name,
    captures['X'][1].peek as argument
FROM ast_match('src/**/*.py', '__F__(__X__)');
```

```sql
-- Match any 3-argument call, only capture the last argument
SELECT captures['Z'][1].peek
FROM ast_match('src/**/*.py', '__F__(__, __, __Z__)');
```

### Working with Captures

Every match returns a `captures` column — a MAP from capture names to LISTs of structs:

```sql
SELECT
    captures['F'][1].name,       -- function name
    captures['F'][1].type,       -- AST node type
    captures['F'][1].peek,       -- source code text
    captures['F'][1].start_line  -- location
FROM ast_match('src/**/*.py', '__F__(__X__)');
```

The `[1]` index is needed because captures are always lists (variadic wildcards can capture multiple nodes). For simple wildcards, the list always has exactly one element.

The convenience macro `ast_capture` saves typing:

```sql
SELECT ast_capture(captures, 'F').name
FROM ast_match('src/**/*.py', '__F__(__X__)');
```

### Same-Name Constraints

When the same capture name appears multiple times, all positions must match the same source text:

```sql
-- Find self-equality: x == x, a == a (but not a == b)
SELECT captures['X'][1].peek
FROM ast_match('src/**/*.py', '__X__ == __X__');

-- Find self-addition: x + x
SELECT captures['X'][1].peek
FROM ast_match('src/**/*.py', '__X__ + __X__');
```

---

## Variadic Wildcards

### The Problem: Exact Structure

Without variadics, patterns must match the exact structure:

```sql
-- This only matches functions whose ENTIRE body is a single return
-- i.e., `def f(x): return x` — not `def f(x): print(x); return x`
SELECT * FROM ast_match('src/**/*.py', 'def __F__(__): return __X__');
```

Real functions have multiple statements, docstrings, setup code. Exact matching is too rigid.

### `<*>` — Zero or More Siblings

The `<*>` wildcard matches **zero or more sibling nodes** at the same depth level:

```sql
-- Find functions with a return, regardless of other body content
SELECT captures['F'][1].name, start_line
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<*>__%
    return __X__');
```

This matches:
- `def f(x): return x` — zero siblings before return
- `def f(x): print(x); return x` — one sibling before return
- `def f(x): a(); b(); c(); return x` — three siblings before return

### `<+>` — One or More Siblings

Use `<+>` when you require at least one statement:

```sql
-- Functions with at least one statement BEFORE the return
-- (excludes trivial one-liner returns)
SELECT captures['F'][1].name
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__BODY<+>__%
    return __X__');
```

### `<?>` — Optional (Zero or One)

Use `<?>` when at most one node should exist at that position:

```sql
-- Functions with at most one statement before return
-- Matches: `def f(): return x` and `def f(): setup(); return x`
-- Rejects: `def f(): a(); b(); return x` (2 statements)
SELECT captures['F'][1].name
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__SETUP<?>__%
    return __X__');
```

### `<~>` — Negation (Must Be Empty)

Use `<~>` to require that NO siblings exist at that position:

```sql
-- Functions whose body is ONLY a return statement (nothing else)
SELECT captures['F'][1].name
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<~>__%
    return __X__');
```

This is the inverse of `<+>`: where `<+>` requires at least one sibling, `<~>` requires zero.

### Named Variadic Captures

Named variadics capture all matched siblings as a list:

```sql
-- See what comes before the return
SELECT
    captures['F'][1].name as func_name,
    length(captures['BODY']) as body_statements,
    captures['BODY'][1].peek as first_statement
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__BODY<*>__%
    return __X__');
```

### Anonymous Variadics

Use `%__<*>__%` (no name) when you don't need the captures:

```sql
-- Just find functions with returns, don't capture the body
SELECT captures['F'][1].name
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<*>__%
    return __X__');
```

---

## Recursive Matching (`<**>`)

### The Gap Between `<*>` and `<**>`

`<*>` matches siblings — nodes at the **same depth level**. This fails when the target is nested inside control flow:

```sql
-- <*> only finds functions where db.execute() is a DIRECT body statement
-- Misses functions where execute() is inside an if, try, or loop
SELECT captures['F'][1].name
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<*>__%
    db.execute(__Y__)');
-- Returns: direct_execute, inner
-- Missing: nested_execute (execute is inside if/try)
```

### `<**>` — Any Depth

`<**>` matches the target pattern at **any depth** within the subtree:

```sql
-- Find functions containing db.execute() ANYWHERE — inside if, try, loops, etc.
SELECT captures['F'][1].name
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<**>__%
    db.execute(__Y__)');
-- Returns: direct_execute, nested_execute, outer_with_execute, inner
```

### When to Use Each

| Wildcard | Scope | Use When |
|----------|-------|----------|
| `<*>` | 0+ siblings | Flexible matching, any number of siblings OK |
| `<+>` | 1+ siblings | Need at least one sibling before/after |
| `<?>` | 0-1 siblings | At most one optional element (e.g., docstring) |
| `<~>` | 0 siblings | Require nothing at that position |
| `<**>` | Any depth | Target may be nested inside control flow |

### Real-World Examples

**Find functions that use database connections anywhere in their body:**

```sql
SELECT DISTINCT captures['F'][1].name as func_name, file_path, start_line
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<**>__%
    cursor.execute(__SQL__)');
```

**Find functions containing a specific API call at any nesting level:**

```sql
SELECT captures['F'][1].name, captures['M'][1].peek
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<**>__%
    requests.__M__(__URL__)');
```

**Find classes with methods that raise exceptions:**

```sql
SELECT captures['C'][1].name as class_name, start_line
FROM ast_match('src/**/*.py',
    'class __C__:
    %__<**>__%
    raise __E__');
```

---

## Relational Operators

For queries that don't need full pattern matching, relational operators provide a simpler, faster alternative.

### `ast_has` — Ancestors Containing a Descendant

Find nodes of one type that contain a descendant of another type at any depth:

```sql
-- Functions that contain a return statement (at any depth)
SELECT name, start_line
FROM ast_has('src/**/*.py', 'function_definition', 'return_statement');
```

With a name filter on the descendant:

```sql
-- Functions that call 'execute' somewhere inside
SELECT name, start_line
FROM ast_has('src/**/*.py', 'function_definition', 'call', 'execute');
```

### `ast_not_has` — Ancestors NOT Containing a Descendant

The inverse of `ast_has` — find nodes that do NOT contain a descendant type:

```sql
-- Functions that do NOT contain a return statement
SELECT name, start_line
FROM ast_not_has('src/**/*.py', 'function_definition', 'return_statement');

-- Functions that never call 'execute'
SELECT name, start_line
FROM ast_not_has('src/**/*.py', 'function_definition', 'call', 'execute');
```

### `ast_inside` — Descendants Within an Ancestor

The inverse of `ast_has` — returns the descendants, scoped to an ancestor:

```sql
-- All return statements inside a specific function
SELECT peek, start_line
FROM ast_inside('src/**/*.py', 'return_statement', 'function_definition', 'process_data');
```

```sql
-- All function calls inside any class definition
SELECT name, peek, start_line
FROM ast_inside('src/**/*.py', 'call', 'class_definition')
WHERE name IS NOT NULL;
```

### `ast_precedes` / `ast_follows` — Sibling Ordering

Find nodes based on their position relative to siblings:

```sql
-- Return statements that come after an if statement (same parent)
SELECT peek, start_line
FROM ast_follows('src/**/*.py', 'return_statement', 'if_statement');
```

```sql
-- Comments that appear before function definitions
SELECT peek, start_line
FROM ast_precedes('src/**/*.py', 'comment', 'function_definition');
```

### Combining Operators

Relational operators compose naturally with SQL:

```sql
-- Functions that contain BOTH a try statement AND a return
SELECT h1.name, h1.start_line
FROM ast_has('src/**/*.py', 'function_definition', 'try_statement') h1
JOIN ast_has('src/**/*.py', 'function_definition', 'return_statement') h2
  ON h1.node_id = h2.node_id;
```

```sql
-- Functions that call execute() but NOT inside a try block
SELECT h.name, h.start_line
FROM ast_has('src/**/*.py', 'function_definition', 'call', 'execute') h
WHERE NOT EXISTS (
    SELECT 1 FROM ast_has('src/**/*.py', 'function_definition', 'try_statement') t
    WHERE t.node_id = h.node_id
);
```

---

## Practical Recipes

### Security Audit: Find Dangerous Calls at Any Depth

```sql
-- Find every function that uses dangerous evaluation anywhere in its body
SELECT
    captures['F'][1].name as func_name,
    file_path,
    start_line
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<**>__%
    eval(__INPUT__)')
ORDER BY file_path, start_line;
```

### Code Review: Functions With Bare Except

```sql
-- Find functions containing try/except with bare except clauses
SELECT captures['F'][1].name as func_name, start_line
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<**>__%
    try:
        __
    except:
        __');
```

### Refactoring: Find Print Statements in Production Code

```sql
-- Find functions with print() calls (should probably use logging)
SELECT name, start_line, file_path
FROM ast_has('src/**/*.py', 'function_definition', 'call', 'print')
WHERE file_path NOT LIKE '%test%'
ORDER BY file_path, start_line;
```

### API Discovery: Find All HTTP Handlers

```sql
-- Find functions decorated with route decorators
SELECT captures['F'][1].name, start_line
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<*>__%
    return __X__')
WHERE file_path LIKE '%views%' OR file_path LIKE '%routes%';
```

### Complexity: Functions With Deep Nesting

```sql
-- Functions containing nested loops (loop inside a loop)
SELECT name, start_line, file_path
FROM ast_has('src/**/*.py', 'for_statement', 'for_statement')
ORDER BY file_path;
```

### Testing: Find Assert Patterns

```sql
-- Assertions comparing two values
SELECT
    captures['A'][1].peek as actual,
    captures['B'][1].peek as expected,
    file_path, start_line
FROM ast_match('test/**/*.py', 'assert __A__ == __B__')
ORDER BY file_path, start_line;
```

### Migration: Find Old API Usage

```sql
-- Find all calls to a deprecated function at any depth
SELECT
    captures['F'][1].name as containing_func,
    file_path, start_line
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<**>__%
    old_api_call(__ARGS__)')
ORDER BY file_path, start_line;
```

### Architecture: Module Dependency Detection

```sql
-- Find which functions call functions from a specific module
SELECT
    captures['F'][1].name as caller,
    captures['M'][1].peek as method_call,
    file_path
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<**>__%
    database.__M__(__)')
ORDER BY file_path;
```

---

## Choosing the Right Tool

| Need | Tool | Example |
|------|------|---------|
| Find structural code patterns | `ast_match` | `'def __F__(__): return __X__'` |
| Check if X contains Y (any depth) | `ast_has` | Functions containing `try_statement` |
| Check if X does NOT contain Y | `ast_not_has` | Functions without `return_statement` |
| Find Y inside X (any depth) | `ast_inside` | Return statements inside a function |
| Any-depth pattern matching | `<**>` in `ast_match` | Functions with dangerous calls nested in if/try |
| Sibling-level flexible matching | `<*>`/`<+>` in `ast_match` | Functions with body before return |
| Sibling ordering | `ast_precedes`/`ast_follows` | Comments before functions |
| Simple node queries | `read_ast` + WHERE | `WHERE name = 'execute'` |

### Performance Guidelines

- `ast_has` / `ast_inside` are fastest — they use O(1) subtree range checks
- `ast_match` with `<*>` is fast for simple patterns
- `ast_match` with `<**>` is slower because it relaxes depth constraints, potentially producing more candidates
- For large codebases, materialize the AST first: `CREATE TABLE code AS SELECT * FROM read_ast(...)`
- `ast_precedes` / `ast_follows` are simple sibling index comparisons

### Nested Functions and Scope

`ast_has`, `ast_not_has`, and `<**>` use AST subtree range checks, not function-scope-aware analysis. This means:

- If function `outer` contains a nested function `inner`, and `inner` calls `execute()`, then `outer` is reported as "having" an `execute` call — because `inner`'s subtree is within `outer`'s descendant range.
- To get scope-aware results (excluding nested function internals), use `ast_function_scope()` instead, or filter results with `depth` checks.

```sql
-- Scope-aware: functions that directly call execute (not via nested functions)
SELECT f.name, f.start_line
FROM read_ast('src/**/*.py') f
WHERE is_function_definition(f.semantic_type)
  AND EXISTS (
      SELECT 1 FROM ast_function_scope('my_ast', f.node_id) s
      WHERE s.type = 'call' AND s.name = 'execute'
  );
```

### Anonymous Wildcard Cardinality

Anonymous wildcards (`%__<?>__%`, `%__<~>__%`) do not enforce cardinality constraints — they behave like `<*>`. Use named wildcards (`%__X<?>__%`, `%__X<~>__%`) when you need the 0-1 or 0-only restriction. The captured list can be ignored if you don't need the values.

---

## Inspecting and Debugging Patterns

### See How Your Pattern Is Parsed

```sql
SELECT rel_depth, pattern_type, pattern_name, is_wildcard, capture_name
FROM ast_pattern('def __F__(__): return __X__', 'python');
```

This shows every node in the parsed pattern, including:
- Which nodes are wildcards
- The relative depth of each node
- Sibling indices

### Check What Types Exist

If your pattern isn't matching, the node types might not be what you expect:

```sql
-- See what types exist in a file
SELECT DISTINCT type, name, peek
FROM read_ast('src/main.py')
WHERE depth <= 3
ORDER BY type;
```

### Debug With `ast_pattern`

```sql
-- Parse a complex pattern to understand its structure
SELECT *
FROM ast_pattern(
    clean_pattern('def __F__(__): %__BODY<*>__% return __X__'),
    'python');
```

---

## Reference

### `ast_match` Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `source` | required | File path, glob, or table name |
| `pattern_str` | required | Code pattern with wildcards |
| `language` | `'python'` | Language for parsing source and pattern |
| `match_syntax` | `false` | Include punctuation in matching |
| `match_by` | `'type'` | Match by `'type'` or `'semantic_type'` |
| `depth_fuzz` | `0` | Allow +/- N levels of depth flexibility |

### Relational Operator Signatures

```sql
ast_has(source, ancestor_type, descendant_type, descendant_name := NULL, language := NULL)
ast_not_has(source, ancestor_type, descendant_type, descendant_name := NULL, language := NULL)
ast_inside(source, descendant_type, ancestor_type, ancestor_name := NULL, language := NULL)
ast_precedes(source, node_type, before_type, before_name := NULL, language := NULL)
ast_follows(source, node_type, after_type, after_name := NULL, language := NULL)
```

### Wildcard Quick Reference

| Syntax | Meaning | Scope |
|--------|---------|-------|
| `__X__` | Named capture | Exact position |
| `__` | Anonymous match | Exact position |
| `%__X<*>__%` | Named 0+ match | Siblings |
| `%__X<+>__%` | Named 1+ match | Siblings |
| `%__X<?>__%` | Named optional (0-1) | Siblings |
| `%__X<~>__%` | Named negation (0 only) | Siblings |
| `%__<*>__%` | Anonymous 0+ match | Siblings |
| `%__<+>__%` | Anonymous 1+ match | Siblings |
| `%__X<**>__%` | Named any-depth match | Descendants |
| `%__<**>__%` | Anonymous any-depth match | Descendants |

### See Also

- [Pattern Matching Reference](pattern-matching.md) — Detailed API reference
- [Cookbook](cookbook.md) — More code analysis recipes
- [Cross-Language Analysis](cross-language.md) — Using semantic types
- [Parsing Files](parsing-files.md) — How to parse source code
