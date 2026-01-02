# Cookbook: Practical Recipes

Real-world SQL recipes for common code analysis tasks.

## Quick Reference

| Task | Key Query Pattern |
|------|-------------------|
| Find all functions | `WHERE is_function_definition(semantic_type)` |
| Find all classes | `WHERE is_class_definition(semantic_type)` |
| Find function calls | `WHERE is_call(semantic_type)` |
| Find imports | `WHERE is_import(semantic_type)` |
| Find control flow | `WHERE is_control_flow(semantic_type)` |
| Find by name | `WHERE name LIKE '%pattern%'` |

---

## Codebase Overview

### Language Distribution

```sql
SELECT
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as nodes
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY files DESC;
```

### Definition Inventory

```sql
SELECT
    language,
    SUM(CASE WHEN is_function_definition(semantic_type) THEN 1 ELSE 0 END) as functions,
    SUM(CASE WHEN is_class_definition(semantic_type) THEN 1 ELSE 0 END) as classes,
    SUM(CASE WHEN is_import(semantic_type) THEN 1 ELSE 0 END) as imports
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY functions DESC;
```

### File Complexity Ranking

```sql
SELECT
    file_path,
    language,
    COUNT(*) as total_nodes,
    MAX(depth) as max_depth,
    COUNT(CASE WHEN is_function_definition(semantic_type) THEN 1 END) as functions
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY file_path, language
ORDER BY total_nodes DESC
LIMIT 20;
```

---

## Finding Functions

### All Function Definitions

```sql
SELECT name, file_path, start_line, end_line
FROM read_ast('src/**/*.py')
WHERE is_function_definition(semantic_type)
  AND name IS NOT NULL
ORDER BY file_path, start_line;
```

### Functions by Name Pattern

```sql
-- Find all validation functions
SELECT name, file_path, start_line
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND name LIKE '%valid%'
ORDER BY name;
```

### Long Functions (Potential Refactoring Candidates)

```sql
SELECT
    name,
    file_path,
    start_line,
    end_line - start_line + 1 as lines,
    descendant_count as complexity
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND end_line - start_line > 50
ORDER BY lines DESC
LIMIT 20;
```

### Most Complex Functions

```sql
SELECT
    name,
    file_path,
    start_line,
    descendant_count as complexity,
    end_line - start_line + 1 as lines
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND descendant_count > 100
ORDER BY complexity DESC
LIMIT 20;
```

### Functions with Many Parameters

```sql
SELECT
    name,
    file_path,
    start_line,
    parameters
FROM read_ast('src/**/*.py', context := 'native')
WHERE is_function_definition(semantic_type)
  AND len(parameters) > 5
ORDER BY len(parameters) DESC;
```

---

## Finding Classes and Types

### All Class Definitions

```sql
SELECT name, file_path, start_line
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_class_definition(semantic_type)
  AND name IS NOT NULL
ORDER BY name;
```

### Classes by Pattern (e.g., Services, Controllers)

```sql
SELECT name, file_path, language
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_class_definition(semantic_type)
  AND (name LIKE '%Service%'
       OR name LIKE '%Controller%'
       OR name LIKE '%Handler%')
ORDER BY name;
```

### Large Classes (God Objects)

```sql
SELECT
    name,
    file_path,
    descendant_count as complexity,
    end_line - start_line + 1 as lines
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_class_definition(semantic_type)
  AND descendant_count > 500
ORDER BY complexity DESC;
```

---

## Import Analysis

### All Imports

```sql
SELECT type, name, file_path, start_line
FROM read_ast('src/**/*.py')
WHERE is_import(semantic_type)
ORDER BY file_path, start_line;
```

### Most Common Imports

```sql
SELECT name, COUNT(*) as usage_count
FROM read_ast('src/**/*.py')
WHERE is_import(semantic_type)
  AND name IS NOT NULL
GROUP BY name
ORDER BY usage_count DESC
LIMIT 20;
```

### Files with Many Imports

```sql
SELECT
    file_path,
    COUNT(*) as import_count
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_import(semantic_type)
GROUP BY file_path
HAVING COUNT(*) > 20
ORDER BY import_count DESC;
```

### External vs Internal Imports (Python)

```sql
SELECT
    file_path,
    SUM(CASE WHEN peek LIKE 'from .%' OR peek LIKE 'import .%' THEN 1 ELSE 0 END) as relative_imports,
    SUM(CASE WHEN peek NOT LIKE 'from .%' AND peek NOT LIKE 'import .%' THEN 1 ELSE 0 END) as external_imports
FROM read_ast('src/**/*.py')
WHERE is_import(semantic_type)
GROUP BY file_path
ORDER BY external_imports DESC;
```

---

## Function Calls

### Find All Calls to a Function

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.py')
WHERE is_call(semantic_type)
  AND name = 'print'
ORDER BY file_path, start_line;
```

### Most Called Functions

```sql
SELECT name, COUNT(*) as call_count
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_call(semantic_type)
  AND name IS NOT NULL
GROUP BY name
ORDER BY call_count DESC
LIMIT 30;
```

### Find Deprecated Function Usage

```sql
SELECT file_path, start_line, name, peek
FROM read_ast('src/**/*.py')
WHERE is_call(semantic_type)
  AND name IN ('eval', 'exec', 'compile', '__import__')
ORDER BY name, file_path;
```

---

## Control Flow Analysis

### Control Flow Density by File

```sql
SELECT
    file_path,
    SUM(CASE WHEN is_conditional(semantic_type) THEN 1 ELSE 0 END) as conditionals,
    SUM(CASE WHEN is_loop(semantic_type) THEN 1 ELSE 0 END) as loops,
    SUM(CASE WHEN is_jump(semantic_type) THEN 1 ELSE 0 END) as jumps
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_control_flow(semantic_type)
GROUP BY file_path
ORDER BY conditionals + loops DESC
LIMIT 20;
```

### Deeply Nested Code

```sql
SELECT
    file_path,
    start_line,
    depth,
    type,
    peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE depth > 10
  AND is_control_flow(semantic_type)
ORDER BY depth DESC
LIMIT 20;
```

### Functions with High Cyclomatic Complexity Proxy

```sql
WITH function_complexity AS (
    SELECT
        f.name,
        f.file_path,
        f.start_line,
        f.end_line,
        f.node_id
    FROM read_ast('src/**/*.py') f
    WHERE is_function_definition(f.semantic_type)
)
SELECT
    fc.name,
    fc.file_path,
    fc.start_line,
    COUNT(*) as control_flow_count
FROM function_complexity fc
JOIN read_ast('src/**/*.py') cf
  ON cf.file_path = fc.file_path
  AND cf.start_line >= fc.start_line
  AND cf.end_line <= fc.end_line
  AND is_control_flow(cf.semantic_type)
GROUP BY fc.name, fc.file_path, fc.start_line
ORDER BY control_flow_count DESC
LIMIT 20;
```

---

## String and Literal Analysis

### Find Hardcoded Strings

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_string_literal(semantic_type)
  AND (peek LIKE '%password%'
       OR peek LIKE '%secret%'
       OR peek LIKE '%api_key%'
       OR peek LIKE '%token%')
ORDER BY file_path;
```

### Find URL Patterns

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_string_literal(semantic_type)
  AND (peek LIKE '%http://%'
       OR peek LIKE '%https://%'
       OR peek LIKE '%localhost%')
ORDER BY file_path;
```

### Find SQL Strings (Potential Injection Risk)

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_string_literal(semantic_type)
  AND (peek LIKE '%SELECT %'
       OR peek LIKE '%INSERT %'
       OR peek LIKE '%UPDATE %'
       OR peek LIKE '%DELETE %')
ORDER BY file_path;
```

---

## API Surface Extraction

### Public Functions (Python)

```sql
SELECT name, file_path, start_line
FROM read_ast('src/**/*.py')
WHERE is_function_definition(semantic_type)
  AND name IS NOT NULL
  AND name NOT LIKE '\_%'  -- Exclude private (underscore prefix)
ORDER BY file_path, name;
```

### Exported Functions (JavaScript/TypeScript)

```sql
SELECT name, file_path, start_line, peek
FROM read_ast('src/**/*.{js,ts}', ignore_errors := true)
WHERE is_export(semantic_type)
  OR (is_function_definition(semantic_type) AND peek LIKE 'export %')
ORDER BY file_path;
```

### Interface Definitions (TypeScript)

```sql
SELECT name, file_path, start_line
FROM read_ast('src/**/*.ts')
WHERE type = 'interface_declaration'
ORDER BY name;
```

### REST Endpoint Patterns

```sql
-- Find decorator-based routes (Python Flask/FastAPI)
SELECT
    a.peek as decorator,
    f.name as function_name,
    f.file_path,
    f.start_line
FROM read_ast('src/**/*.py') a
JOIN read_ast('src/**/*.py') f
  ON f.file_path = a.file_path
  AND f.start_line = a.start_line + 1
WHERE is_annotation(a.semantic_type)
  AND (a.peek LIKE '%@app.route%'
       OR a.peek LIKE '%@router.%'
       OR a.peek LIKE '%@get%'
       OR a.peek LIKE '%@post%')
  AND is_function_definition(f.semantic_type);
```

---

## Code Quality Checks

### Empty Functions

```sql
SELECT name, file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND descendant_count < 5
  AND name IS NOT NULL
ORDER BY file_path;
```

### TODO/FIXME Comments

```sql
SELECT file_path, start_line, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_comment(semantic_type)
  AND (peek LIKE '%TODO%'
       OR peek LIKE '%FIXME%'
       OR peek LIKE '%HACK%'
       OR peek LIKE '%XXX%')
ORDER BY file_path, start_line;
```

### Functions Without Docstrings (Python)

```sql
WITH functions AS (
    SELECT name, file_path, start_line, node_id
    FROM read_ast('src/**/*.py')
    WHERE is_function_definition(semantic_type)
      AND name IS NOT NULL
),
docstrings AS (
    SELECT file_path, start_line
    FROM read_ast('src/**/*.py')
    WHERE type = 'expression_statement'
      AND peek LIKE '"""%'
)
SELECT f.name, f.file_path, f.start_line
FROM functions f
LEFT JOIN docstrings d
  ON d.file_path = f.file_path
  AND d.start_line = f.start_line + 1
WHERE d.file_path IS NULL
ORDER BY f.file_path;
```

---

## Cross-Language Patterns

### Compare Function Counts

```sql
SELECT
    language,
    COUNT(*) as function_count,
    ROUND(AVG(descendant_count), 1) as avg_complexity
FROM read_ast([
    'src/**/*.py',
    'src/**/*.js',
    'src/**/*.java',
    'src/**/*.go'
], ignore_errors := true)
WHERE is_function_definition(semantic_type)
GROUP BY language
ORDER BY function_count DESC;
```

### Find Similar Named Functions Across Languages

```sql
SELECT name, language, file_path, start_line
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND name IN (
      SELECT name
      FROM read_ast('src/**/*.*', ignore_errors := true)
      WHERE is_function_definition(semantic_type)
        AND name IS NOT NULL
      GROUP BY name
      HAVING COUNT(DISTINCT language) > 1
  )
ORDER BY name, language;
```

---

## Caching and Performance

### Create a Cached AST Table

```sql
CREATE TABLE codebase_ast AS
SELECT * FROM read_ast('src/**/*.*', ignore_errors := true);

-- Now query the cached table
SELECT language, COUNT(*) FROM codebase_ast GROUP BY language;
```

### Export to Parquet for Later Analysis

```sql
COPY (
    SELECT
        file_path,
        language,
        type,
        name,
        semantic_type,
        semantic_type_to_string(semantic_type) as semantic_name,
        start_line,
        end_line,
        depth,
        descendant_count
    FROM read_ast('src/**/*.*', ignore_errors := true)
    WHERE is_definition(semantic_type)
       OR is_call(semantic_type)
       OR is_import(semantic_type)
) TO 'codebase_analysis.parquet';
```

### Incremental Analysis Pattern

```sql
-- Store last analysis timestamp
CREATE TABLE IF NOT EXISTS analysis_meta (
    last_run TIMESTAMP
);

-- Query only modified files (requires file metadata)
-- This is a pattern - actual implementation needs file mtime access
```

---

## Security Analysis

### Dangerous Function Calls

```sql
-- Find eval/exec usage (code injection risk)
SELECT language, name, file_path, start_line, LEFT(peek, 80) as context
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_call(semantic_type)
  AND name IN ('eval', 'exec', 'compile', '__import__', 'execfile', 'system', 'popen')
ORDER BY language, name;
```

### Shell Command Execution

```sql
-- Find subprocess/os.system usage
SELECT file_path, start_line, LEFT(peek, 100) as context
FROM read_ast('src/**/*.py')
WHERE peek LIKE '%subprocess%' OR peek LIKE '%os.system%' OR peek LIKE '%os.popen%'
ORDER BY file_path;
```

### Input Without Validation

```sql
-- Find functions that take user input but don't validate
WITH input_functions AS (
    SELECT file_path, name, start_line, end_line
    FROM read_ast('src/**/*.py')
    WHERE is_function_definition(semantic_type) AND name IS NOT NULL
),
uses_input AS (
    SELECT DISTINCT f.file_path, f.name
    FROM input_functions f
    JOIN read_ast('src/**/*.py') i
      ON i.file_path = f.file_path
      AND i.start_line >= f.start_line AND i.end_line <= f.end_line
      AND is_call(i.semantic_type) AND i.name IN ('input', 'raw_input')
),
has_validation AS (
    SELECT DISTINCT f.file_path, f.name
    FROM uses_input u
    JOIN input_functions f ON u.file_path = f.file_path AND u.name = f.name
    JOIN read_ast('src/**/*.py') v
      ON v.file_path = f.file_path
      AND v.start_line >= f.start_line AND v.end_line <= f.end_line
      AND (v.type = 'try_statement'
           OR (is_call(v.semantic_type) AND v.name IN ('int', 'float', 'isinstance')))
)
SELECT u.file_path, u.name,
    CASE WHEN h.name IS NOT NULL THEN 'validated' ELSE 'UNVALIDATED' END as status
FROM uses_input u
LEFT JOIN has_validation h ON u.file_path = h.file_path AND u.name = h.name;
```

### Security Concern Summary

```sql
-- Categorize potential security issues
SELECT
    CASE
        WHEN name IN ('eval', 'exec', 'compile') THEN 'Code Injection'
        WHEN name IN ('system', 'popen', 'execvp') THEN 'Command Injection'
        WHEN name IN ('open', 'fopen') THEN 'File Operations'
        WHEN name IN ('socket', 'connect', 'bind') THEN 'Network Operations'
        ELSE 'Other'
    END as risk_category,
    language,
    COUNT(*) as occurrences
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_call(semantic_type)
  AND name IN ('eval', 'exec', 'compile', 'system', 'popen', 'open', 'socket', 'connect')
GROUP BY risk_category, language
ORDER BY risk_category, occurrences DESC;
```

---

## Structural Analysis

> **Tip**: Many of these patterns are now available as built-in macros. See [Structural Analysis Macros](../api/structural-analysis.md) for `ast_function_metrics`, `ast_nesting_analysis`, `ast_security_audit`, `ast_dead_code`, and more.

### Using Built-in Macros (Recommended)

```sql
-- Create cached AST table
CREATE TABLE codebase AS SELECT * FROM read_ast('src/**/*.py');

-- Function complexity metrics (returns, conditionals, loops, cyclomatic)
SELECT name, lines, cyclomatic, return_count
FROM ast_function_metrics('codebase')
WHERE cyclomatic > 5
ORDER BY cyclomatic DESC;

-- Nesting depth analysis
SELECT name, max_depth, deep_nodes
FROM ast_nesting_analysis('codebase')
WHERE deep_nodes > 0;

-- Security audit
SELECT function_name, risk_category, risk_level, matched_pattern
FROM ast_security_audit('codebase')
WHERE risk_level = 'high';

-- Dead code detection
SELECT name, definition_type, file_path
FROM ast_dead_code('codebase');

-- Functions containing specific patterns
SELECT func_name FROM ast_functions_containing('codebase', 'try_statement');
```

### Tree Navigation

```sql
-- Get children of a specific node
SELECT type, name FROM ast_children('codebase', 42);

-- Get all descendants (subtree)
SELECT * FROM ast_descendants('codebase', 42);

-- Get ancestors up to root
SELECT type, name FROM ast_ancestors('codebase', 100) ORDER BY depth;

-- Find what contains line 50
SELECT type, name FROM ast_containing_line('codebase', 50)
ORDER BY (end_line - start_line);  -- smallest first

-- Get nodes in a line range
SELECT * FROM ast_in_range('codebase', 10, 20);
```

### Scope-Aware Analysis

```sql
-- Get function scope (excludes nested function bodies)
SELECT * FROM ast_function_scope('codebase', 42);

-- Get class members (methods, fields)
SELECT name, type FROM ast_class_members('codebase', 1);

-- Find functions with error handling
SELECT func_name FROM ast_functions_containing('codebase', 'try_statement');

-- Find functions calling specific APIs
SELECT func_name, match_name
FROM ast_functions_containing('codebase', 'call')
WHERE match_name IN ('eval', 'exec');
```

---

## Code Analytics

### Language Fingerprint (Semantic Composition)

```sql
-- Semantic composition per 1000 nodes - unique fingerprint per language
WITH lang_totals AS (
    SELECT language, COUNT(*) as total
    FROM read_ast('src/**/*.*', ignore_errors := true)
    GROUP BY language
)
SELECT
    a.language,
    ROUND(1000.0 * SUM(CASE WHEN is_function_definition(semantic_type) THEN 1 ELSE 0 END) / t.total, 1) as functions_per_1k,
    ROUND(1000.0 * SUM(CASE WHEN is_call(semantic_type) THEN 1 ELSE 0 END) / t.total, 1) as calls_per_1k,
    ROUND(1000.0 * SUM(CASE WHEN is_control_flow(semantic_type) THEN 1 ELSE 0 END) / t.total, 1) as control_flow_per_1k,
    ROUND(1000.0 * SUM(CASE WHEN is_literal(semantic_type) THEN 1 ELSE 0 END) / t.total, 1) as literals_per_1k,
    ROUND(1000.0 * SUM(CASE WHEN is_comment(semantic_type) THEN 1 ELSE 0 END) / t.total, 1) as comments_per_1k
FROM read_ast('src/**/*.*', ignore_errors := true) a
JOIN lang_totals t ON a.language = t.language
GROUP BY a.language, t.total;
```

### Function Size Distribution

```sql
-- Categorize functions by size (complexity profile)
SELECT
    language,
    COUNT(*) as total_functions,
    ROUND(100.0 * COUNT(CASE WHEN end_line - start_line <= 5 THEN 1 END) / COUNT(*), 1) as tiny_pct,
    ROUND(100.0 * COUNT(CASE WHEN end_line - start_line BETWEEN 6 AND 20 THEN 1 END) / COUNT(*), 1) as small_pct,
    ROUND(100.0 * COUNT(CASE WHEN end_line - start_line BETWEEN 21 AND 50 THEN 1 END) / COUNT(*), 1) as medium_pct,
    ROUND(100.0 * COUNT(CASE WHEN end_line - start_line > 50 THEN 1 END) / COUNT(*), 1) as large_pct
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND name IS NOT NULL
GROUP BY language
ORDER BY large_pct DESC;
```

### Nested Function Analysis (Closure Usage)

```sql
-- Detect closure/callback-heavy code
SELECT
    language,
    COUNT(*) as all_functions,
    COUNT(CASE WHEN depth > 3 THEN 1 END) as nested_functions,
    ROUND(100.0 * COUNT(CASE WHEN depth > 3 THEN 1 END) / COUNT(*), 1) as nested_pct
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
GROUP BY language
ORDER BY nested_pct DESC;
```

### Definition to Call Ratio

```sql
-- Code style indicator: OOP (high ratio) vs functional (low ratio)
SELECT
    language,
    SUM(CASE WHEN is_function_definition(semantic_type) THEN 1 ELSE 0 END) as definitions,
    SUM(CASE WHEN is_call(semantic_type) THEN 1 ELSE 0 END) as calls,
    ROUND(
        SUM(CASE WHEN is_call(semantic_type) THEN 1.0 ELSE 0 END) /
        NULLIF(SUM(CASE WHEN is_function_definition(semantic_type) THEN 1 ELSE 0 END), 0),
    1) as calls_per_function
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY calls_per_function DESC;
```

### Control Flow Style

```sql
-- Loop vs conditional ratio (algorithmic vs branching)
SELECT
    language,
    SUM(CASE WHEN is_loop(semantic_type) THEN 1 ELSE 0 END) as loops,
    SUM(CASE WHEN is_conditional(semantic_type) THEN 1 ELSE 0 END) as conditionals,
    ROUND(
        SUM(CASE WHEN is_loop(semantic_type) THEN 1.0 ELSE 0 END) /
        NULLIF(SUM(CASE WHEN is_conditional(semantic_type) THEN 1 ELSE 0 END), 0),
    2) as loop_to_conditional_ratio
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_control_flow(semantic_type)
GROUP BY language
ORDER BY loop_to_conditional_ratio DESC;
```

### Comment Density (Documentation Quality)

```sql
SELECT
    language,
    COUNT(CASE WHEN is_comment(semantic_type) THEN 1 END) as comments,
    COUNT(CASE WHEN is_function_definition(semantic_type) THEN 1 END) as functions,
    ROUND(
        1.0 * COUNT(CASE WHEN is_comment(semantic_type) THEN 1 END) /
        NULLIF(COUNT(CASE WHEN is_function_definition(semantic_type) THEN 1 END), 0),
    2) as comments_per_function
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY comments_per_function DESC;
```

### Common Function Names Across Languages

```sql
-- Find universal naming patterns
SELECT
    name,
    COUNT(DISTINCT language) as languages,
    COUNT(*) as occurrences,
    STRING_AGG(DISTINCT language, ', ' ORDER BY language) as in_languages
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE is_function_definition(semantic_type)
  AND name IS NOT NULL
  AND length(name) > 2
GROUP BY name
HAVING COUNT(DISTINCT language) >= 2
ORDER BY languages DESC, occurrences DESC
LIMIT 20;
```

### Semantic Super Kind Distribution

```sql
-- High-level code composition
SELECT
    language,
    ROUND(100.0 * SUM(CASE WHEN get_super_kind(semantic_type) = 'DATA_STRUCTURE' THEN 1 ELSE 0 END) / COUNT(*), 1) as data_pct,
    ROUND(100.0 * SUM(CASE WHEN get_super_kind(semantic_type) = 'COMPUTATION' THEN 1 ELSE 0 END) / COUNT(*), 1) as compute_pct,
    ROUND(100.0 * SUM(CASE WHEN get_super_kind(semantic_type) = 'CONTROL_EFFECTS' THEN 1 ELSE 0 END) / COUNT(*), 1) as control_pct,
    ROUND(100.0 * SUM(CASE WHEN get_super_kind(semantic_type) = 'META_EXTERNAL' THEN 1 ELSE 0 END) / COUNT(*), 1) as meta_pct
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type != 0
GROUP BY language;
```

---

## Troubleshooting Queries

### Check What Types Exist

```sql
SELECT DISTINCT type, semantic_type_to_string(semantic_type) as semantic
FROM read_ast('example.py')
ORDER BY semantic, type;
```

### Debug Name Extraction

```sql
SELECT type, name, peek
FROM read_ast('example.py')
WHERE is_function_definition(semantic_type)
   OR is_class_definition(semantic_type);
```

### Check Parsing Errors

```sql
SELECT file_path, type, peek
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE type = 'ERROR'
LIMIT 10;
```

---

## See Also

- [Structural Analysis Macros](../api/structural-analysis.md) - Full macro reference
- [Cross-Language Analysis](cross-language.md) - More cross-language patterns
- [Semantic Types](../api/semantic-types.md) - Full type reference
- [API Reference](../api/core-functions.md) - Function documentation
