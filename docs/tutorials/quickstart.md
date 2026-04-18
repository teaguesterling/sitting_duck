# Your First Query

This tutorial walks you through querying source code with Sitting Duck.
You will parse a Python file, find functions and classes with CSS selectors,
and explore scope relationships -- all in SQL.

The examples use `test/data/python/sample_app.py`, a small app with classes,
functions, and a few deliberate code smells.

## Parse a file

Start by loading the extension and reading the AST:

```sql
LOAD sitting_duck;

SELECT type, name, start_line, depth
FROM read_ast('test/data/python/sample_app.py')
WHERE name IS NOT NULL
LIMIT 10;
```

`read_ast` parses a file into a flat table -- one row per AST node.
Every row carries the node's `type`, `name`, `start_line`, `depth`,
`semantic_type`, and more. You *could* filter with raw `WHERE type = ...`
clauses, but there is a better way.

## Find functions with ast_select

`ast_select` lets you query the AST with CSS-style selectors instead
of hand-written WHERE clauses. The `.func` shorthand matches any function
definition, regardless of language-specific node types:

```sql
SELECT name, start_line, end_line
FROM ast_select('test/data/python/sample_app.py', '.func');
```

Similarly, `.class` matches class definitions:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py', '.class');
```

These semantic aliases (`.func`, `.class`, `.call`, `.var`, `.literal`)
work across all 27 supported languages. No need to memorize that Python
uses `function_definition` while JavaScript uses `function_declaration`.

## Filter by name

Append `#name` to match a specific identifier. Find the `main` function:

```sql
SELECT name, start_line, end_line, descendant_count
FROM ast_select('test/data/python/sample_app.py', '.func#main');
```

## Structural queries with :has

Selectors can express structural relationships. Find every function that
contains a `return_statement`:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py', '.func:has(return_statement)');
```

Or find classes that contain a call to `execute`:

```sql
SELECT name, start_line
FROM ast_select('test/data/python/sample_app.py', '.class:has(.call#execute)');
```

## Parse once, query many with ast_select_from

Each `ast_select` call re-parses the source file. When you are exploring
interactively, parse once into a table and query it repeatedly:

```sql
CREATE TABLE app_ast AS
  SELECT * FROM read_ast('test/data/python/sample_app.py');

-- All functions
SELECT name, start_line FROM ast_select_from('app_ast', '.func');

-- All call sites
SELECT name, start_line FROM ast_select_from('app_ast', '.call');

-- Functions that never return a value
SELECT name FROM ast_select_from('app_ast', '.func:not(:has(return_statement))');
```

This is the recommended workflow for interactive analysis. Parsing is
expensive; selector matching is cheap.

## Scope: "what function is this inside?"

Every node carries a `scope` struct. The `scope.function` field gives
the `node_id` of the enclosing function -- you can join on it to answer
"which function contains this call?":

```sql
SELECT
    call.name AS call_name,
    caller.name AS in_function,
    call.start_line
FROM ast_select_from('app_ast', '.call') AS call
JOIN app_ast AS caller
  ON caller.node_id = call.scope.function
  AND caller.file_path = call.file_path
ORDER BY call.start_line;
```

Or use the `::parent-definition` pseudo-element to get the nearest
enclosing definition directly:

```sql
SELECT name, start_line
FROM ast_select_from('app_ast', '.call#execute::parent-definition');
```

## Use semantic_type strings, not numbers

The `semantic_type` column uses human-readable strings like
`'DEFINITION_FUNCTION'` and `'COMPUTATION_CALL'`. You never need raw
numeric codes:

```sql
SELECT name, semantic_type, start_line
FROM read_ast('test/data/python/sample_app.py')
WHERE semantic_type = 'DEFINITION_FUNCTION';
```

But prefer `.func` in `ast_select` -- it is shorter and cross-language.

## Clean up

```sql
DROP TABLE IF EXISTS app_ast;
```

## Next steps

- [CSS Selectors Tutorial](css-selectors.md) -- combinators, pseudo-classes,
  attribute filters, and pseudo-elements in depth.
- [CSS Selectors Reference](../reference/css-selectors.md) -- complete syntax
  and examples for every selector feature.
