# CSS Selectors for AST Querying

Query AST nodes using CSS selector syntax. If you know CSS, you already know how to query code structure.

```sql
-- Functions that call execute() but have no try block
SELECT name, start_line
FROM ast_select('src/**/*.py',
    '.function:has(.call#execute):not(:has(try_statement))');
```

## How It Works

`ast_select` parses your CSS selector using Sitting Duck's own tree-sitter CSS grammar, then translates the parsed selector AST into SQL conditions against your source code's AST. No string parsing — the CSS grammar does the heavy lifting.

## Syntax Reference

### Type Selectors

Match nodes by their tree-sitter type name:

```sql
-- All function definitions
SELECT name, start_line
FROM ast_select('src/*.py', 'function_definition');

-- All class bodies
SELECT name, start_line
FROM ast_select('src/*.js', 'class_body');
```

### `#name` — Name Filter

Match nodes by their identifier name, like CSS ID selectors:

```sql
-- The function named "main"
SELECT name, start_line
FROM ast_select('src/*.py', 'function_definition#main');

-- Calls to "execute"
SELECT name, start_line
FROM ast_select('src/*.py', 'call#execute');
```

### `.semantic` — Semantic Type Filter

Match nodes by semantic type, like CSS class selectors. Works across all 27 languages:

```sql
-- All function definitions (any language)
SELECT name, language, start_line
FROM ast_select('src/**/*.*', '.function');

-- All class definitions
SELECT name, start_line
FROM ast_select('src/**/*.*', '.class');

-- All function calls
SELECT name, start_line
FROM ast_select('src/*.py', '.call');
```

Both lowercase and uppercase work: `.function` and `.FUNCTION` are equivalent.

Available semantic types: `function`, `class`, `variable`, `module`, `type`, `call`, `literal`, `identifier`, `definition`, `computation`.

### Combinators

#### `A B` — Descendant

B is anywhere inside A (any depth):

```sql
-- Method definitions inside a class body
SELECT name, start_line
FROM ast_select('src/*.js', 'class_body method_definition');
```

#### `A > B` — Direct Child

B is an immediate child of A:

```sql
-- Methods directly inside a class body (not nested deeper)
SELECT name, start_line
FROM ast_select('src/*.js', 'class_body > method_definition');
```

#### `A ~ B` — General Sibling

B appears after A as a sibling (same parent):

```sql
-- Class definitions that follow an import statement
SELECT name, start_line
FROM ast_select('src/*.py', 'import_statement ~ class_definition');
```

#### `A + B` — Adjacent Sibling

B immediately follows A (next sibling):

```sql
-- Statements immediately after an import
SELECT name, start_line
FROM ast_select('src/*.py', 'import_statement + expression_statement');
```

### Pseudo-Classes

#### `:has()` — Contains Descendant

Match nodes that contain a descendant matching the inner selector:

```sql
-- Functions containing a return statement
SELECT name, start_line
FROM ast_select('src/*.py', 'function_definition:has(return_statement)');

-- Functions that call execute()
SELECT name, start_line
FROM ast_select('src/*.py', '.function:has(.call#execute)');
```

#### `:not(:has())` — Does Not Contain

Match nodes that do NOT contain a descendant:

```sql
-- Functions without a return statement
SELECT name, start_line
FROM ast_select('src/*.py', 'function_definition:not(:has(return_statement))');

-- Functions that never call execute()
SELECT name, start_line
FROM ast_select('src/*.py', '.function:not(:has(.call#execute))');
```

### Compound Selectors

Combine type, name, semantic, and pseudo-classes on the same element:

```sql
-- Functions named "main" that contain a try statement
SELECT name, start_line
FROM ast_select('src/*.py', 'function_definition#main:has(try_statement)');

-- Functions that call execute() but have no error handling
SELECT name, start_line
FROM ast_select('src/*.py',
    '.function:has(.call#execute):not(:has(try_statement))');
```

### Attribute Selectors

Filter by node attributes using `[attr=value]`:

```sql
-- Filter by name (same as #name)
SELECT name, start_line
FROM ast_select('src/*.py', 'function_definition[name=main]');

-- Filter by language
SELECT name, start_line
FROM ast_select('src/**/*.*', '.function[language=python]');

-- Filter by semantic type (same as .semantic)
SELECT name, start_line
FROM ast_select('src/*.py', '[semantic=FUNCTION]');
```

Supported attributes: `name`, `type`, `language`, `semantic`.

---

## Comparison with Other Tools

### vs `ast_has` / `ast_not_has`

`ast_select` composes the same operations as relational operators, but in a single string:

```sql
-- These are equivalent:
SELECT name FROM ast_select('src/*.py',
    '.function:has(.call#execute):not(:has(try_statement))');

SELECT h.name
FROM ast_has('src/*.py', 'function_definition', 'call', 'execute') h
WHERE NOT EXISTS (
    SELECT 1 FROM ast_has('src/*.py', 'function_definition', 'try_statement') t
    WHERE t.node_id = h.node_id
);
```

### vs `ast_match`

Use `ast_match` when you need to capture specific sub-expressions from code patterns. Use `ast_select` when you need to filter nodes by structural relationships:

| Need | Tool |
|------|------|
| "Find functions containing X" | `ast_select` |
| "Extract the return value from functions" | `ast_match` |
| "Find functions with X but not Y" | `ast_select` |
| "Capture both function name and its arguments" | `ast_match` |
| Cross-language structural queries | `ast_select` with `.semantic` |
| Pattern-by-example code search | `ast_match` with wildcards |

### vs grep

`ast_select` answers questions grep fundamentally cannot:

```sql
-- "Functions that DON'T contain X" — grep can't express absence within a scope
SELECT name FROM ast_select('src/*.py', '.function:not(:has(return_statement))');

-- "Functions with X but not Y" — grep can't intersect over scope boundaries
SELECT name FROM ast_select('src/*.py',
    '.function:has(for_statement):not(:has(try_statement))');
```

---

## API Reference

```sql
ast_select(source, selector, language := NULL)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `source` | VARCHAR | File path, glob pattern, or array of paths |
| `selector` | VARCHAR | CSS selector string |
| `language` | VARCHAR | Optional language override (auto-detected from extension) |

Returns the same columns as `read_ast()` — filtered to nodes matching the rightmost element in the selector chain.

---

## See Also

- [Structural Search](structural-search.md) — Pattern matching with `ast_match` and wildcards
- [Tutorial: Finding Code Patterns](tutorial-pattern-matching.md) — Step-by-step introduction
- [Cookbook](cookbook.md) — Practical analysis recipes
