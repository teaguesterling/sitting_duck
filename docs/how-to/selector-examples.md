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

## Call Graph Analysis

### Find leaf functions (no calls inside)

```sql
SELECT name, file_path
FROM ast_select('src/**/*.py', '.func:not(:has(.call))');
```

### Find hub functions (many callers AND many callees)

```sql
WITH callees AS (
    SELECT caller, COUNT(DISTINCT callee) as out_degree
    FROM ast_callees('src/**/*.py') GROUP BY caller
),
callers AS (
    SELECT callee, COUNT(DISTINCT caller) as in_degree
    FROM ast_callees('src/**/*.py') GROUP BY callee
)
SELECT callees.caller as name, in_degree, out_degree
FROM callees
JOIN callers ON callers.callee = callees.caller
WHERE in_degree >= 3 AND out_degree >= 3
ORDER BY in_degree + out_degree DESC;
```

### Find unused functions (defined but never called)

```sql
SELECT name, start_line, file_path
FROM ast_select('src/**/*.py', '.func:not(:is-called)');
```

### Find definitions that are never referenced anywhere

```sql
SELECT name, type, start_line, file_path
FROM ast_select('src/**/*.py', '.def:not(:is-referenced)');
```

### Cross-file call chain: what does main() call, and what do those call?

```sql
-- Level 1: direct callees of main
SELECT callee as level_1
FROM ast_callees('src/**/*.py')
WHERE caller = 'main';

-- Level 2: what do those functions call?
SELECT c1.callee as level_1, c2.callee as level_2
FROM ast_callees('src/**/*.py') c1
JOIN ast_callees('src/**/*.py') c2 ON c2.caller = c1.callee
WHERE c1.caller = 'main';
```

### Find functions that call a dangerous function

```sql
-- Functions calling eval
SELECT name, file_path
FROM ast_select('src/**/*.py', '.func:calls(eval)');

-- Functions calling exec
SELECT name, file_path
FROM ast_select('src/**/*.py', '.func:calls(exec)');

-- Functions calling both eval and exec
SELECT name, file_path
FROM ast_select('src/**/*.py', '.func:calls(eval):calls(exec)');
```

---

## See Also

- [CSS Selectors Overview](../reference/css-selectors.md) — Full API reference and combinators
- [Tutorial](../tutorials/css-selectors.md) — Step-by-step introduction
- [Pseudo-Classes Reference](../reference/css-pseudo-classes.md) — Complete pseudo-class documentation
- [Attribute Selectors](../reference/css-attributes.md) — All attribute operators and fields
- [Cookbook](cookbook.md) — General analysis recipes beyond CSS selectors
