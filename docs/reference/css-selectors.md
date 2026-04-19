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

Type selectors come in three tiers of specificity — `.semantic` (cross-language), bare keywords (language-specific prefix match), and exact types (tree-sitter specific). See [Node Type Selectors](node-type-selectors.md) for details.

Semantic types provide ~80 aliases like `.func`, `.if`, `.loop` that work identically across all 27 languages. See [Semantic Type Aliases](semantic-aliases.md) for the full table.

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

-- Dead code: exported but never referenced
SELECT name FROM ast_select('src/*.py', ':exported:not(:is-referenced)');

-- Navigate: who calls this function?
SELECT name FROM ast_select('src/*.py', '.func#validate::callers');

-- Navigate: what class contains this method?
SELECT name FROM ast_select('src/*.py', '.func#validate::parent-definition');
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

## Custom Pseudo-Classes

Define your own pseudo-classes and use them in selectors:

```sql
-- Define a predicate
CREATE MACRO ast_selector_predicate_is_test(node, arg) AS (
    node.name IS NOT NULL AND starts_with(node.name, 'test_')
);

-- Use it like any built-in pseudo-class
SELECT name FROM ast_select('src/*.py', '.func:is_test');
SELECT name FROM ast_select('src/*.py', '.func:not(:is_test):has(return_statement)');
```

For dynamic dispatch, install `func_apply` and run `PRAGMA sitting_duck_enable_dynamic_predicates`. You can also override `ast_dispatch_predicate` directly for a static set of predicates without any dependencies. See [Custom Predicates](css-pseudo-classes.md#custom-predicates) for full details.

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

### `ast_select_from` — Parse Once, Query Many (v1.7.4+)

For repeated queries against the same source, parse once into a table and then run selectors against the cached AST:

```sql
-- Parse once (expensive)
CREATE TABLE my_ast AS SELECT * FROM read_ast('src/**/*.py');

-- Query many times (cheap — no re-parsing)
SELECT * FROM ast_select_from('my_ast', '.func:has(return_statement)');
SELECT * FROM ast_select_from('my_ast', '.class:named');
SELECT * FROM ast_select_from('my_ast', 'class_definition function_definition');
```

```sql
ast_select_from(source, selector)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `source` | VARCHAR | Name of a pre-parsed AST table (string) |
| `selector` | VARCHAR | CSS selector string |

Same output, same selector syntax. The only difference is the `source` parameter is a table name rather than a file path. On large codebases, this amortizes the parse cost across queries — each subsequent `ast_select_from` call runs in ~1s instead of re-parsing from disk.

### Multi-Rule Dispatch (v1.7.0+, currently WIP)

For queries that contain **multiple CSS rules** — each with its own selector and declaration block — sitting_duck provides two macros that dispatch `ast_select` once per rule and tag the matches with per-rule metadata.

```sql
ast_select_rules(source, query, language := NULL)    -- flat: row per (rule, match)
ast_select_list(source, query, language := NULL)     -- nested: row per rule, matches as LIST
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `source` | VARCHAR | File path, glob pattern, or array of paths |
| `query` | VARCHAR | Multi-rule CSS query — each rule must include a declaration block (e.g., `'.fn { show: signature; }'`). Bare selectors should use `ast_select` instead. |
| `language` | VARCHAR | Optional language override (auto-detected) |

**Intended query shape:**

```sql
-- Flat form: one row per (rule, match) pair
SELECT rule_index, selector, declarations['show'], name
FROM ast_select_rules('src/**/*.py',
    '.func { show: signature; }
     #main { show: body; }
     .class#Config { show: outline; }');
```

Declarations are stored as `MAP(VARCHAR, VARCHAR)` where the value is the raw text span between `:` and `;` (trimmed). Consumers cast as needed, e.g., `declarations['depth']::INT`, `string_split(declarations['trace'], ',')`.

**Status**: The macros register successfully but any call currently crashes at bind time due to a DuckDB v1.5.1 planner regression ([duckdb/duckdb#21890](https://github.com/duckdb/duckdb/issues/21890)) in the optimizer's dependent-join flattening. The sitting_duck side is complete and will "just work" once the upstream fix lands. See `tracker/bugs/007-duckdb-correlated-macro-bind-error.md` for the minimal repro, workarounds, and resolution plan.

---

```sql
ast_type_map(language := NULL)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `language` | VARCHAR | Optional: filter to one language |

Returns: `language`, `node_type`, `semantic_type`, `kind`, `name_role`, `is_scope`, `is_syntax`, `name_strategy`, `flags`.

### Scope Resolution Macros

```sql
ast_exports(source, language := NULL)    -- Module-level public definitions
ast_imports(source, language := NULL)    -- Imported names with source module
ast_resolve(source, language := NULL)    -- Reference → definition binding
```

`ast_resolve` walks the scope chain (`scope.current` → `scope.stack`) to find which definition each reference binds to:

```sql
SELECT ref_name, ref_line, def_line, def_type
FROM ast_resolve('src/main.py')
WHERE ref_name = 'config';
```

Cross-file resolution via import/export JOIN:

```sql
SELECT im.imported_name, ex.file_path, ex.type
FROM ast_imports('src/app.py') im
JOIN ast_exports('src/**/*.py') ex
  ON ex.name = im.imported_name
  AND ex.file_path LIKE '%' || im.source_module || '%';
```

### Call Graph Macros

```sql
ast_callees(source, language := NULL)   -- For each function, all calls within its scope
ast_callers(source, language := NULL)   -- For each call, the enclosing function
```

`ast_callees` returns one row per (function, call) pair. `ast_callers` returns one row per (call, enclosing function) pair. Both use scope resolution to handle nested functions correctly.

```sql
-- Full call graph
SELECT caller, callee, COUNT(*) as n
FROM ast_callees('src/**/*.py')
GROUP BY caller, callee ORDER BY n DESC;

-- Who calls validate?
SELECT caller, file_path FROM ast_callers('src/**/*.py') WHERE callee = 'validate';
```

Cross-file call graph with import resolution:

```sql
-- Combine call graph with imports to trace cross-module calls
SELECT c.caller, c.callee, c.file_path as caller_file, ex.file_path as callee_file
FROM ast_callees('src/**/*.py') c
JOIN ast_exports('src/**/*.py') ex ON ex.name = c.callee
WHERE c.file_path != ex.file_path;
```

---

## See Also

- [Node Type Selectors](node-type-selectors.md) — Three tiers of type specificity
- [Semantic Type Aliases](semantic-aliases.md) — Full alias table for `.semantic` selectors
- [Pseudo-Classes Reference](css-pseudo-classes.md) — All pseudo-classes by category
- [Attribute Selectors](css-attributes.md) — Query by name, modifier, annotation, and more
- [Tutorial](../tutorials/css-selectors.md) — Step-by-step walkthrough building up selectors
- [Examples / Cookbook](../how-to/selector-examples.md) — Practical recipes by use case
- [Structural Search](../how-to/structural-search.md) — Pattern matching with `ast_match` and wildcards
