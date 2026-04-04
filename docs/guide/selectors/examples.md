# Examples / Cookbook

Practical `ast_select` recipes organized by use case. All examples are copy-pasteable — replace the glob patterns with your own paths.

## Security Auditing

### Find SQL injection risks

String interpolation inside SQL query strings:

```sql
SELECT name, peek, start_line, file_path
FROM ast_select('src/**/*.py',
    '.func:has(.str[peek*=SELECT]):has(.str[peek*=%])');
```

### Find functions using dangerous calls without error handling

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py',
    '.func:has(.call#execute):not(:has(.try))');
```

### Find eval/exec usage

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py', '.call#eval');

SELECT name, start_line, file_path
FROM ast_select('src/**/*.js', '.call#eval');
```

### Find hardcoded credentials

```sql
SELECT peek, start_line, file_path
FROM ast_select('src/**/*.py',
    '.var[name*=password]:has(.str)');

SELECT peek, start_line, file_path
FROM ast_select('src/**/*.py',
    '.var[name*=secret]:has(.str)');
```

### Find functions that catch all exceptions (bare except)

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py',
    '.func:has(except_clause:not(:has(.id)))');
```

## Code Quality

### Functions without return statements

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py',
    '.func:named:not(:has(return_statement))');
```

### Functions with loops but no error handling

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py',
    '.func:has(.loop):not(:has(.try))');
```

### Deeply nested functions (functions inside functions)

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py',
    '.func .func');
```

### Empty except blocks

```sql
SELECT peek, start_line, file_path
FROM ast_select('src/**/*.py',
    'except_clause:has(block:empty)');
```

### Large classes (many methods)

```sql
SELECT name, file_path,
       (SELECT count(*) FROM ast_select(file_path, '.class#' || name || ' > .func')) as method_count
FROM ast_select('src/**/*.py', '.class:named')
ORDER BY method_count DESC;
```

## Refactoring

### Find all uses of a deprecated function

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py', '.call#old_function_name');
```

### Find classes that don't define `__init__`

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py',
    '.class:not(:has(.func#__init__))');
```

### Find methods that could be static (no self reference)

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py',
    '.class .func:not(:has(.self)):named');
```

### Find string literals that should be constants

```sql
SELECT peek, start_line, file_path
FROM ast_select('src/**/*.py',
    '.func .str[peek*=http]');
```

### Find duplicated import patterns

```sql
SELECT name, file_path
FROM ast_select('src/**/*.py', '.import')
GROUP BY name
HAVING count(DISTINCT file_path) > 5
ORDER BY count(DISTINCT file_path) DESC;
```

## API Discovery

### Find all route handlers (Flask/FastAPI)

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py', '.func[annotation*=route]');

SELECT name, start_line, file_path
FROM ast_select('src/**/*.py', '.func[annotation*=get]');

SELECT name, start_line, file_path
FROM ast_select('src/**/*.py', '.func[annotation*=post]');
```

### Find all test functions

```sql
SELECT name, start_line, file_path
FROM ast_select('test/**/*.py', '.func[name^=test_]');
```

### Find all class constructors across languages

```sql
-- Python
SELECT name, file_path FROM ast_select('src/**/*.py', '.func#__init__');

-- JavaScript/TypeScript
SELECT name, file_path FROM ast_select('src/**/*.{js,ts}', '.func#constructor');

-- Java
SELECT name, file_path FROM ast_select('src/**/*.java', 'constructor_declaration');
```

### Find exported functions (JavaScript/TypeScript)

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.ts', '.export .func');
```

### Find all async functions

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py', '.func:async');

SELECT name, start_line, file_path
FROM ast_select('src/**/*.js', '.func:async');
```

## Cross-Language Analysis

### Compare error handling patterns

```sql
-- Python: functions with try/except
SELECT 'python' as lang, name, file_path
FROM ast_select('src/**/*.py', '.func:has(.try)');

-- JavaScript: functions with try/catch
SELECT 'javascript' as lang, name, file_path
FROM ast_select('src/**/*.js', '.func:has(.try)');

-- Go: functions with error checks (idiomatic)
SELECT 'go' as lang, name, file_path
FROM ast_select('src/**/*.go', '.func:has(.if:has(.id#err))');
```

### Find all class definitions regardless of language

```sql
SELECT name, language, file_path
FROM ast_select('src/**/*', '.class:named');
```

### Find all function definitions with their scope context

```sql
SELECT name, type, start_line, file_path
FROM ast_select('src/**/*', '.func:named:scope');
```

---

## See Also

- [CSS Selectors Overview](index.md) — Full API reference and combinators
- [Tutorial](tutorial.md) — Step-by-step introduction
- [Pseudo-Classes Reference](pseudo-classes.md) — Complete pseudo-class documentation
- [Attribute Selectors](attributes.md) — All attribute operators and fields
- [Cookbook](../cookbook.md) — General analysis recipes beyond CSS selectors
