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

## Three Tiers of Type Selectors

Sitting Duck offers three levels of type specificity:

### Dotted selectors (`.semantic`) — Cross-language, broadest

Match by semantic category. Works identically across all 27 languages:

```sql
.func              -- all function definitions (any language)
.if                -- all conditionals: if, elif, else, switch, case, match
.loop              -- all loops: for, while, do-while
.call              -- all function/method calls
.import            -- all import statements
```

`.if` includes elif, else, switch, case — the entire conditional family. Both `.func` and `.FUNC` work (case-insensitive).

### Bare keywords — Language-specific, medium

Match by tree-sitter node type prefix. `if` matches `if` (the keyword token) AND `if_statement`, `if_clause`, etc.:

```sql
if                 -- if keyword + if_statement + if_clause
return             -- return keyword + return_statement
for                -- for keyword + for_statement + for_in_clause
class              -- class keyword + class_definition
```

Bare keywords are narrower than dotted selectors but broader than exact types:

```sql
-- These are different:
.if                -- ALL conditionals (if + elif + else + switch + case + match)
if                 -- just if and if_* variants
if_statement       -- exactly if_statement (most specific)
```

### Exact types — Tree-sitter specific, narrowest

Match the exact tree-sitter node type name:

```sql
function_definition       -- exactly this type, nothing else
method_definition         -- exactly this type
class_declaration         -- exactly this type (JS/TS)
```

## Semantic Type Aliases

The `.` selector accepts ~80 aliases organized by category:

### Definitions
| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.func` | `.fn`, `.function`, `.method` | Function/method definitions |
| `.class` | `.cls`, `.struct`, `.trait`, `.interface` | Class definitions |
| `.var` | `.variable`, `.let`, `.const` | Variable definitions |
| `.mod` | `.module`, `.package` | Module definitions |
| `.def` | `.definition` | All definitions (kind level) |

### Control Flow
| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.if` | `.cond`, `.conditional` | All conditionals |
| `.loop` | `.for`, `.while` | All loops |
| `.jump` | `.return`, `.break`, `.continue`, `.yield` | All jumps |
| `.flow` | `.control` | All control flow (kind level) |

### Error Handling
| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.try` | | Try blocks |
| `.catch` | `.except`, `.rescue` | Catch/except handlers |
| `.throw` | `.raise` | Throw/raise statements |
| `.finally` | `.ensure`, `.defer` | Finally blocks |
| `.error` | `.err` | All error handling (kind level) |

### Names and References
| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.id` | `.ident`, `.identifier` | Plain identifiers |
| `.qualified` | `.dotted` | Qualified names (module.attr) |
| `.self` | `.this` | Self/this references |
| `.call` | `.invoke` | Function/method calls |
| `.member` | `.attr`, `.field`, `.prop` | Member access |

### Literals
| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.str` | `.string` | String literals |
| `.num` | `.number` | Numeric literals |
| `.bool` | `.boolean` | Boolean literals |
| `.coll` | `.list`, `.dict`, `.array`, `.map`, `.set`, `.tuple` | Collection literals |
| `.lit` | `.literal`, `.value` | All literals (kind level) |

### Other
| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.import` | `.require`, `.use` | Import statements |
| `.export` | `.pub` | Export declarations |
| `.op` | `.operator` | All operators |
| `.arith` | `.math` | Arithmetic operators |
| `.cmp` | `.comparison` | Comparison operators |
| `.logic` | `.logical` | Logical operators |

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

## Pseudo-Classes

### `:has()` — Contains Descendant

Match nodes that contain a descendant matching the inner selector:

```sql
-- Functions containing a return statement
SELECT name FROM ast_select('src/*.py', '.func:has(return_statement)');

-- Functions that call execute()
SELECT name FROM ast_select('src/*.py', '.func:has(.call#execute)');
```

### `:not(:has())` — Does Not Contain

Match nodes that do NOT contain a descendant:

```sql
-- Functions without a return statement
SELECT name FROM ast_select('src/*.py', '.func:not(:has(return_statement))');

-- Functions that never call execute()
SELECT name FROM ast_select('src/*.py', '.func:not(:has(.call#execute))');
```

## Compound Selectors

Combine type, name, semantic, and pseudo-classes:

```sql
-- Functions that call execute() but have no error handling
SELECT name, start_line
FROM ast_select('src/*.py',
    '.func:has(.call#execute):not(:has(try_statement))');

-- Functions named "main" that contain a try statement
SELECT name FROM ast_select('src/*.py', 'function_definition#main:has(try_statement)');
```

---

## Discovering Node Types

Use `ast_type_map()` to explore what node types are available and how they map to semantic categories:

```sql
-- All Python function-related types
SELECT node_type, semantic_type, name_role, is_scope
FROM ast_type_map('python')
WHERE kind = 'definition';
```

```
assignment                DEFINITION_VARIABLE   definition   false
async_function_definition DEFINITION_FUNCTION   definition   true
class_definition          DEFINITION_CLASS      definition   true
function_definition       DEFINITION_FUNCTION   definition   true
lambda                    DEFINITION_FUNCTION   definition   true
...
```

```sql
-- What does `for_statement` map to across languages?
SELECT language, semantic_type, is_scope
FROM ast_type_map()
WHERE node_type = 'for_statement'
ORDER BY language;
```

```sql
-- All node types that create scopes
SELECT language, node_type, semantic_type
FROM ast_type_map()
WHERE is_scope
ORDER BY language, node_type;
```

```sql
-- All reference-type nodes in JavaScript
SELECT node_type, semantic_type
FROM ast_type_map('javascript')
WHERE name_role = 'reference';
```

---

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

---

## Performance

On 834K AST nodes (2,468 Python files from Rosettacode):

| Query | Time |
|---|---|
| `function_definition:has(return_statement)` | 4.4s |
| Compound `:has` + `:not(:has)` | 4.2s |
| Equivalent `ast_has` | 3.3s |

The ~30% overhead is the CSS selector parsing and CTE dispatch. On smaller codebases (58K nodes), queries return in ~120ms.

Parallel parsing (8 threads) provides 4.9x speedup on medium datasets.

---

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

- [Structural Search](structural-search.md) — Pattern matching with `ast_match` and wildcards
- [Tutorial: Finding Code Patterns](tutorial-pattern-matching.md) — Step-by-step introduction
- [Cookbook](cookbook.md) — Practical analysis recipes
