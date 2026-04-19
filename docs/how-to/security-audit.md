# Security Audit

Find security anti-patterns in your codebase: dangerous function calls, hardcoded secrets, shell injection risks, and unvalidated input.

## Quick approach: ast_security_audit macro

```sql
CREATE TABLE codebase AS SELECT * FROM read_ast('src/**/*.py');

SELECT function_name, risk_category, risk_level, matched_pattern
FROM ast_security_audit('codebase')
WHERE risk_level = 'high'
ORDER BY risk_category;
```

The macro detects common patterns across categories: code injection, command injection, file operations, and network operations.

## Hardcoded secrets

Find strings that look like credentials or API keys:

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 'LITERAL_STRING'
  AND string_contains_any_i(peek, ['password', 'secret', 'api_key', 'token', 'private_key'])
ORDER BY file_path;
```

## URL and endpoint patterns

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 'LITERAL_STRING'
  AND string_contains_any(peek, ['http://', 'https://', 'localhost', '127.0.0.1'])
ORDER BY file_path;
```

## SQL in string literals

Potential SQL injection if these strings include user input:

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 'LITERAL_STRING'
  AND string_contains_any(peek, ['SELECT ', 'INSERT ', 'UPDATE ', 'DELETE ', 'DROP '])
ORDER BY file_path;
```

## Categorized risk summary

```sql
SELECT
    CASE
        WHEN name IN ('compile') THEN 'Code Injection'
        WHEN name IN ('system', 'popen') THEN 'Command Injection'
        WHEN name IN ('open', 'fopen') THEN 'File Operations'
        WHEN name IN ('socket', 'connect', 'bind') THEN 'Network Operations'
        ELSE 'Other'
    END AS risk_category,
    language,
    COUNT(*) AS occurrences
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 'COMPUTATION_CALL'
  AND name IN ('compile', 'system', 'popen', 'open', 'socket', 'connect')
GROUP BY risk_category, language
ORDER BY risk_category, occurrences DESC;
```

## Using CSS selectors

```sql
CREATE TABLE codebase AS SELECT * FROM read_ast('src/**/*.py');

-- Functions containing dangerous calls
SELECT name, file_path
FROM ast_select_from('codebase',
    '.func:has(.call#compile)');
```

## Cross-language audit

```sql
CREATE TABLE all_code AS
SELECT * FROM read_ast([
    'src/**/*.py', 'src/**/*.js', 'src/**/*.java', 'src/**/*.go'
], ignore_errors := true);

SELECT function_name, risk_category, risk_level, language
FROM ast_security_audit('all_code')
ORDER BY risk_level DESC, risk_category;
```

## Caveats

- **False positives**: `open()` for file I/O is flagged even when used safely. Review results in context.
- **Framework-level safety**: ORMs, prepared statements, and sandboxing aren't visible at the AST level.
- **Dynamic patterns**: `getattr`-based dispatch and string concatenation that builds dangerous calls won't be caught by name-based matching.

## See also

- [Analysis Macros](../reference/analysis-macros.md) — `ast_security_audit` reference
- [Semantic Types](../reference/semantic-types.md) — `LITERAL_STRING`, `COMPUTATION_CALL` types
- [CSS Pseudo-Classes](../reference/css-pseudo-classes.md) — `:has` for containment queries
