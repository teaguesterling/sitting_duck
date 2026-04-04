# CSS Selectors for AST Querying

Query AST nodes using CSS selector syntax. If you know CSS, you already know how to query code structure.

```sql
-- Functions that call execute() but have no try block
SELECT name, start_line
FROM ast_select('src/**/*.py',
    '.func:has(.call#execute):not(:has(try_statement))');
```

## How It Works

`ast_select` parses your CSS selector using Sitting Duck's own tree-sitter CSS grammar, then translates the parsed selector AST into SQL conditions against your source code's AST. No string parsing — the CSS grammar does the heavy lifting.

Type selectors come in three tiers of specificity — `.semantic` (cross-language), bare keywords (language-specific prefix match), and exact types (tree-sitter specific). See [Node Type Selectors](node-types.md) for details.

Semantic types provide ~80 aliases like `.func`, `.if`, `.loop` that work identically across all 27 languages. See [Semantic Type Aliases](kinds-types-and-classes.md) for the full table.

## `#name` — Name Filter

Match nodes by their identifier name:

```sql
-- The function named "main"
SELECT * FROM ast_select('src/*.py', 'function_definition#main');

-- Calls to "execute"
SELECT * FROM ast_select('src/*.py', '.call#execute');

-- Combine with :has
SELECT * FROM ast_select('src/*.py', '.func:has(.call#execute)');
```

## Combinators

### `A B` — Descendant

B is anywhere inside A (any depth):

```sql
-- Methods inside a class body
SELECT name FROM ast_select('src/*.js', 'class_body method_definition');

-- Return statements inside functions (using bare keywords)
SELECT name FROM ast_select('src/*.py', 'function return');
```

### `A > B` — Direct Child

B is an immediate child of A:

```sql
-- Methods directly inside a class body (not nested deeper)
SELECT name FROM ast_select('src/*.js', 'class_body > method_definition');
```

### `A ~ B` — General Sibling

B appears after A as a sibling (same parent):

```sql
-- Class definitions that follow an import statement
SELECT name FROM ast_select('src/*.py', 'import_statement ~ class_definition');
```

### `A + B` — Adjacent Sibling

B immediately follows A (next sibling):

```sql
-- Statements immediately after an import
SELECT name FROM ast_select('src/*.py', 'import_statement + expression_statement');
```

## Compound Selectors

All pseudo-classes compose freely:

```sql
-- Functions that call execute() but have no error handling
SELECT name, start_line
FROM ast_select('src/*.py',
    '.func:has(.call#execute):not(:has(try_statement))');

-- Named definitions that are scope boundaries
SELECT name, type FROM ast_select('src/*.py', ':named:definition:scope');

-- First function in each scope that has a return
SELECT name FROM ast_select('src/*.py',
    'function_definition:first-child:has(return_statement)');

-- Return statements in scope of functions that follow a class
SELECT peek FROM ast_select('src/*.py',
    'return_statement:scope(function_definition):follows(class_definition)');
```

## Comparison with Other Tools

### vs `ast_has` / `ast_not_has`

`ast_select` composes the same operations in a single string:

```sql
-- These are equivalent:
SELECT name FROM ast_select('src/*.py',
    '.func:has(.call#execute):not(:has(try_statement))');

SELECT h.name
FROM ast_has('src/*.py', 'function_definition', 'call', 'execute') h
WHERE NOT EXISTS (
    SELECT 1 FROM ast_has('src/*.py', 'function_definition', 'try_statement') t
    WHERE t.node_id = h.node_id
);
```

### vs `ast_match`

| Need | Tool |
|------|------|
| "Find functions containing X" | `ast_select` |
| "Extract the return value from functions" | `ast_match` |
| "Find functions with X but not Y" | `ast_select` |
| "Capture both function name and its arguments" | `ast_match` |
| Cross-language structural queries | `ast_select` with `.semantic` |
| Pattern-by-example code search | `ast_match` with wildcards |

### vs grep

`ast_select` answers questions grep cannot:

```sql
-- "Functions that DON'T contain X" — grep can't express absence within scope
SELECT name FROM ast_select('src/*.py', '.func:not(:has(return_statement))');

-- "Functions with X but not Y" — grep can't intersect over scope boundaries
SELECT name FROM ast_select('src/*.py', '.func:has(for_statement):not(:has(try_statement))');
```

## Performance

On 834K AST nodes (2,468 Python files from Rosettacode):

| Query | Time |
|---|---|
| `function_definition:has(return_statement)` | 4.4s |
| Compound `:has` + `:not(:has)` | 4.2s |
| Equivalent `ast_has` | 3.3s |

The ~30% overhead is the CSS selector parsing and CTE dispatch. On smaller codebases (58K nodes), queries return in ~120ms.

Parallel parsing (8 threads) provides 4.9x speedup on medium datasets.

## API Reference

```sql
ast_select(source, selector, language := NULL)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `source` | VARCHAR | File path, glob pattern, or array of paths |
| `selector` | VARCHAR | CSS selector string |
| `language` | VARCHAR | Optional language override (auto-detected) |

Returns the same columns as `read_ast()`.

```sql
ast_type_map(language := NULL)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `language` | VARCHAR | Optional: filter to one language |

Returns: `language`, `node_type`, `semantic_type`, `kind`, `name_role`, `is_scope`, `is_syntax`, `name_strategy`, `flags`.

---

## See Also

- [Node Type Selectors](node-types.md) — Three tiers of type specificity
- [Semantic Type Aliases](kinds-types-and-classes.md) — Full alias table for `.semantic` selectors
- [Pseudo-Classes Reference](pseudo-classes.md) — All pseudo-classes by category
- [Attribute Selectors](attributes.md) — Query by name, modifier, annotation, and more
- [Tutorial](tutorial.md) — Step-by-step walkthrough building up selectors
- [Examples / Cookbook](examples.md) — Practical recipes by use case
- [Structural Search](../structural-search.md) — Pattern matching with `ast_match` and wildcards
