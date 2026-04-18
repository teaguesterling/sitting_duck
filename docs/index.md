# Sitting Duck

**Query source code with SQL.** CSS selectors, pattern matching, scope-aware call graphs — across 27 languages.

```sql
-- Functions that call execute() but have no error handling
SELECT name, file_path
FROM ast_select('src/**/*.py',
    '.func:has(.call#execute):not(:has(try_statement))');
```

Sitting Duck is a [DuckDB](https://duckdb.org) extension that parses source code into ASTs using [tree-sitter](https://tree-sitter.github.io/) grammars, then exposes them as SQL tables you can query, join, and aggregate.

## What You Can Ask

### Structural search with CSS selectors

Query AST nodes the way you'd query a DOM — type selectors, combinators, pseudo-classes, `:has()`, `:not()`:

```sql
-- Async functions that don't await anything
SELECT name FROM ast_select('src/**/*.js',
    'function_declaration:async:not(:has(await_expression))');

-- Class methods that follow another method (sibling combinator)
SELECT name FROM ast_select('src/**/*.py',
    'class_definition function_definition ~ function_definition');

-- Who calls this function? (pseudo-element navigation)
SELECT name FROM ast_select('src/**/*.py', '.func#validate::callers');
```

~80 semantic aliases (`.func`, `.class`, `.call`, `.loop`, `.if`, `.namespace`, ...) work identically across all 27 languages. [Full selector reference →](guide/selectors/index.md)

### Pattern matching by example

Find code structures using real syntax with named wildcards:

```sql
-- Capture function names and their return values
SELECT captures['F'].name AS func, captures['X'].peek AS returns
FROM ast_match('src/*.py',
    'def __F__(__):
        return __X__', 'python');

-- Find dangerous function calls and capture what's being passed
SELECT captures['X'].peek AS dangerous_input, file_path
FROM ast_match('src/**/*.py', 'subprocess.call(__X__)', 'python');
```

[Pattern matching guide →](guide/pattern-matching.md)

### Scope-aware analysis

Every node carries a `scope` struct with precomputed shortcuts — no range joins needed:

```sql
-- What function contains each call?
SELECT name, scope.function AS enclosing_fn
FROM read_ast('src/**/*.py')
WHERE semantic_type = 'COMPUTATION_CALL';

-- All class methods (inside a class, not a nested function)
SELECT name, scope.class
FROM read_ast('src/**/*.py')
WHERE semantic_type = 'DEFINITION_FUNCTION'
  AND scope.class IS NOT NULL;
```

### Call graphs

```sql
-- Who calls validate?
SELECT caller, call_line FROM ast_callers('src/**/*.py')
WHERE callee = 'validate';

-- What does main call?
SELECT callee, callee_line FROM ast_callees('src/**/*.py')
WHERE caller = 'main';
```

### Cross-language semantic types

The same query works on Python, JavaScript, Rust, Go, C++, and 22 more languages:

```sql
SELECT language, COUNT(*) AS functions
FROM read_ast(['src/**/*.py', 'src/**/*.js', 'src/**/*.go'])
WHERE semantic_type = 'DEFINITION_FUNCTION'
GROUP BY language;
```

## Quick Start

```sql
INSTALL sitting_duck FROM community;
LOAD sitting_duck;

-- Parse and explore
SELECT name, type, start_line
FROM read_ast('src/**/*.py')
WHERE semantic_type = 'DEFINITION_FUNCTION'
ORDER BY start_line;

-- Parse once, query many (interactive workflow)
CREATE TABLE ast AS SELECT * FROM read_ast('src/**/*.py');
SELECT * FROM ast_select_from('ast', '.func:has(return_statement)');
SELECT * FROM ast_select_from('ast', '.class:named');
```

## 27 Languages

| Category | Languages |
|----------|-----------|
| **Web** | JavaScript, TypeScript, HTML, CSS |
| **Systems** | C, C++, Go, Rust, Zig |
| **Scripting** | Python, Ruby, PHP, Lua, R, Bash |
| **Enterprise** | Java, C#, Kotlin, Swift, Dart |
| **Data** | SQL, DuckDB, GraphQL, JSON |
| **Config** | HCL (Terraform), TOML |
| **Docs** | Markdown |

## Documentation

- **[Getting Started](getting-started/installation.md)** — installation and first query
- **[CSS Selectors](guide/selectors/index.md)** — the full selector language (`.func:has(X):not(:has(Y))`)
- **[Pattern Matching](guide/pattern-matching.md)** — find code by example with `ast_match`
- **[Output Schema](api/output-schema.md)** — all 19 columns including `scope`, `semantic_type`, `qualified_name`
- **[Semantic Types](api/semantic-types.md)** — the cross-language type system
- **[Cookbook](guide/cookbook.md)** — practical recipes
- **[API Reference](api/core-functions.md)** — `read_ast`, `parse_ast`, `ast_select`, and more
- **[AI Agent Guide](ai-agent-guide.md)** — using Sitting Duck with Claude Code, Cursor, and other AI tools

## Why "Sitting Duck"?

- **Sitting** — a nod to [Tree-sitter](https://tree-sitter.github.io/), our parsing engine
- **Duck** — everything quacks like data in [DuckDB](https://duckdb.org)
- Your codebase becomes a sitting duck for SQL-based analysis
