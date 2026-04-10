# Tutorial: Building CSS Selectors Step by Step

This tutorial walks through `ast_select` progressively, using a sample Python application with classes and functions. Each step adds one concept.

The example file is `test/data/python/sample_app.py` — a small app with `Config`, `DatabaseConnection`, and `UserService` classes, plus standalone functions like `process_file`, `validate_email`, `retry`, and `main`.

## Step 1: Simple Type Selector

Start by finding all function definitions:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py', 'function_definition');
```

This returns every function and method: `__init__`, `load`, `get`, `connect`, `execute`, `fetch_all`, `close`, `get_user`, `create_user`, `delete_user`, `search_users`, `bulk_import`, `export_users`, `process_file`, `transform`, `validate_email`, `retry`, `main`.

That's a lot. Let's narrow it down.

## Step 2: Add `#name` Filter

Find a specific function by name:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py', 'function_definition#main');
```

Returns just the `main` function at line 144. The `#name` filter is an exact match on the node's `name` field.

Find all calls to `execute`:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py', '.call#execute');
```

## Step 3: Use `.semantic` for Cross-Language Queries

Instead of the tree-sitter-specific `function_definition`, use `.func`:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py', '.func');
```

Same results, but `.func` works across all 27 languages. If you later scan Java or TypeScript files, the same selector works without changes.

Find all class definitions:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py', '.class');
```

Returns `Config`, `DatabaseConnection`, `UserService`.

## Step 4: Add `:has()` and `:not(:has())`

Find functions that contain a call to `execute`:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py',
    '.func:has(.call#execute)');
```

This returns `fetch_all`, `get_user`, `create_user`, `delete_user`, `search_users`, `export_users` — every method that calls `self.db.execute()` or `self.db.fetch_all()` (which itself calls `execute`).

Now find functions that call `execute` but have no error handling:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py',
    '.func:has(.call#execute):not(:has(.try))');
```

This excludes `bulk_import` (which has a try block), surfacing methods where database errors would propagate unhandled.

## Step 5: Combine with Attribute Selectors

Find functions whose names start with `get_`:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py',
    '.func[name^=get_]');
```

Returns `get_user` and `get` (from `Config.get`).

Find string literals containing SQL:

```sql
SELECT peek, start_line
FROM ast_select('test/data/python/sample_app.py',
    '.str[peek*=SELECT]');
```

This catches the SQL query strings in `get_user`, `search_users`, and `export_users`.

Combine type and attribute selectors to find the dangerous pattern — string literals with SQL inside functions that don't use parameterized queries:

```sql
SELECT name, peek, start_line
FROM ast_select('test/data/python/sample_app.py',
    '.func:has(.str[peek*=SELECT]):has(.str[peek*=%])');
```

## Step 6: Use `:scope()` for Precision

The sample app has methods inside classes and standalone functions at module level. `:scope()` lets you distinguish them.

Find return statements scoped to their direct enclosing function (not returns from nested functions):

```sql
SELECT peek, start_line
FROM ast_select('test/data/python/sample_app.py',
    'return_statement:scope(function)');
```

Find calls scoped to the `UserService` class (not calls in other classes):

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py',
    '.call:scope(.class#UserService)');
```

## Step 7: Use `:matches()` for Structural Patterns

`:matches()` parses its argument as real code and checks for structural subtree matches.

Find functions that contain a `self.db` assignment:

```sql
SELECT name
FROM ast_select('test/data/python/sample_app.py',
    '.func:matches("self.db")');
```

Find functions that return `None` explicitly:

```sql
SELECT name
FROM ast_select('test/data/python/sample_app.py',
    '.func:matches("return None")');
```

Use `___` as a wildcard — find any method that does `self.___ = ___` (attribute assignment):

```sql
SELECT name
FROM ast_select('test/data/python/sample_app.py',
    '.func:matches("self.___ = ___")');
```

Returns the `__init__` methods that set instance attributes.

## Putting It All Together

Combine everything to answer a complex question: "Which methods in UserService call the database without error handling, and what SQL do they use?"

```sql
SELECT name, start_line, peek
FROM ast_select('test/data/python/sample_app.py',
    '.class#UserService .func:has(.call#execute):not(:has(.try)):has(.str[peek*=SELECT])');
```

This single selector uses:
- `.class#UserService` — scope to the UserService class
- `.func` — match methods
- `:has(.call#execute)` — that call execute
- `:not(:has(.try))` — without error handling
- `:has(.str[peek*=SELECT])` — containing SQL SELECT strings

## Step 8: Call Graph Queries

The call graph pseudo-classes bring everything together by answering questions about how functions relate to each other.

Find functions that call `execute` using scope-aware resolution:

```sql
SELECT name
FROM ast_select('test/data/python/sample_app.py', '.func:calls(execute)');
```

Unlike `:has(.call#execute)`, `:calls()` uses scope resolution -- if `execute` is called inside a nested helper, it only attributes the call to the inner function, not the outer one.

Find which functions are actually used:

```sql
-- Popular functions (called 2+ times)
SELECT name
FROM ast_select('test/data/python/sample_app.py', '.func:callers(2)');

-- Dead code: functions nobody calls
SELECT name
FROM ast_select('test/data/python/sample_app.py', '.func:unreferenced');
```

Build a call graph for the whole file:

```sql
SELECT caller, callee
FROM ast_callees('test/data/python/sample_app.py');
```

Combine call graph with earlier selectors for precise questions: "Which functions call `execute` without error handling, and are themselves called by `main`?"

```sql
SELECT name
FROM ast_select('test/data/python/sample_app.py',
    '.func:calls(execute):not(:has(.try)):called-by(main)');
```

---

## See Also

- [CSS Selectors Overview](index.md) — Full API reference and combinators
- [Examples / Cookbook](examples.md) — More practical recipes
- [Node Type Selectors](node-types.md) — Understanding the three tiers
- [Pseudo-Classes Reference](pseudo-classes.md) — Complete pseudo-class list
