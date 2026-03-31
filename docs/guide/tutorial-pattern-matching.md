# Tutorial: Finding Code Patterns with Wildcards

You've inherited a Python codebase and need to understand it quickly. How many functions are there? What do they return? Which ones touch the database? Are there error-handling gaps?

You could read every file line by line. Or you could ask structural questions and get precise answers in seconds.

This tutorial walks through Sitting Duck's pattern matching system using a single sample application. Each section builds on the previous one, introducing one new concept at a time.

## Setup

```sql
LOAD sitting_duck;
```

We'll work with a small Python application that has configuration, database access, a user service, and some utility functions. You can follow along with any Python file — adjust the file paths to your own code.

Let's see what we're working with:

```sql
SELECT COUNT(*) as total_nodes,
       COUNT(DISTINCT name) as named_nodes
FROM read_ast('app.py');
```

## 1. Simple Wildcards: What Functions Exist?

The simplest pattern question: "find all function definitions." The `__X__` syntax is a **named wildcard** — it matches any single node and captures it with the name `X`.

```sql
SELECT
    captures['F'][1].name as func_name,
    start_line
FROM ast_match('app.py',
    'def __F__(__):
        %__<BODY*>__%',
    'python')
ORDER BY start_line;
```

This returns every function definition in the file — top-level functions, methods, nested functions — all of them.

The pattern reads: "a `def` statement, with any name (captured as `F`), any parameters (the anonymous `__`), and any body." We'll explain `%__<BODY*>__%` in Section 3 — for now, it just means "some body content."

### Multiple Captures

Wildcards compose. Put multiple named wildcards in one pattern to extract several pieces at once:

```sql
SELECT
    captures['F'][1].name as method,
    captures['R'][1].peek as returns,
    start_line
FROM ast_match('app.py',
    'def __F__(self, __):
        %__<BODY*>__%
        return __R__',
    'python')
ORDER BY start_line;
```

| method | returns | start_line |
|--------|---------|------------|
| load | self | 16 |
| get | self.data.get(key, default) | 21 |
| connect | self | 32 |
| execute | self._conn.execute(query, params) | 36 |
| fetch_all | result.fetchall() | 41 |
| get_user | None | 56 |
| create_user | self.get_user(email) | 65 |
| delete_user | user | 72 |
| search_users | results | 81 |
| export_users | self.db.fetch_all("SELECT * FROM users") | 96 |

With one query you can see the return expression of every method. The `self` parameter makes this match only methods (not standalone functions), and `__R__` captures whatever follows `return`.

### How Captures Work

Every match returns a `captures` column — a MAP from names to lists of structs:

```sql
captures['F'][1].name       -- the identifier text
captures['F'][1].peek       -- the source code snippet
captures['F'][1].start_line -- location in the file
captures['F'][1].type       -- AST node type
```

The `[1]` index exists because captures are always lists — variadic wildcards (Section 3) can capture multiple nodes. For simple wildcards, the list always has exactly one element.

### Anonymous Wildcards

Use `__` (no name) when you need a placeholder but don't care about the value:

```sql
-- Find classes, ignore what's in them
SELECT captures['C'][1].name as class_name, start_line
FROM ast_match('app.py',
    'class __C__:
        %__<BODY*>__%',
    'python')
ORDER BY start_line;
```

| class_name | start_line |
|------------|------------|
| Config | 10 |
| DatabaseConnection | 25 |
| UserService | 51 |

---

## 2. Variadic Wildcards: Flexible Body Matching

### The Problem

Without variadic wildcards, patterns are rigid. This pattern:

```sql
SELECT * FROM ast_match('app.py', 'def __F__(__): return __X__', 'python');
```

only matches functions whose *entire body* is a single return statement. A function like `def f(x): print(x); return x` has two body statements, so it won't match.

### `<*>` — Zero or More

The `%__<BODY*>__%` wildcard matches **zero or more sibling nodes** at the same depth:

```sql
SELECT
    captures['F'][1].name as func,
    length(captures['BODY']) as body_stmts
FROM ast_match('app.py',
    'def __F__(__):
        %__<BODY*>__%
        return __R__',
    'python')
ORDER BY body_stmts DESC
LIMIT 5;
```

| func | body_stmts |
|------|------------|
| validate_email | 4 |
| process_file | 3 |
| transform | 3 |
| get_user | 2 |
| delete_user | 2 |

`validate_email` has 4 statements before its return — if/elif checks, splits, etc. The variadic captures all of them.

**Important:** Variadic wildcards need a **concrete anchor** at the same depth to establish scope. The `return __R__` after the variadic is the anchor. A pattern with *only* a variadic and no other concrete nodes will return empty captures.

### `<+>` — One or More

Use `<+>` when you want to *require* at least one statement. This filters out functions that are just a bare return:

```sql
SELECT captures['F'][1].name as func,
       length(captures['BODY']) as body_stmts
FROM ast_match('app.py',
    'def __F__(__):
        %__<BODY+>__%
        return __R__',
    'python')
ORDER BY body_stmts DESC
LIMIT 5;
```

Same results here because all the matched functions happen to have body content. But if you had `def simple(x): return x`, `<+>` would exclude it while `<*>` would include it.

---

## 3. Optional and Negation: Presence and Absence

### `<?>` — Optional (Zero or One)

Sometimes you want to know if something is present, without requiring it. The `<?>` wildcard matches zero or one node:

```sql
SELECT
    captures['F'][1].name as func,
    CASE WHEN length(captures['PRE']) > 0
         THEN captures['PRE'][1].peek
         ELSE '(nothing)' END as before_return
FROM ast_match('app.py',
    'def __F__(__):
        %__<PRE?>__%
        return __R__',
    'python')
ORDER BY start_line;
```

For a pair of simple functions like:

```python
def simple(x):
    return x

def with_setup(x):
    print(x)
    return x
```

You'd see:

| func | before_return |
|------|---------------|
| simple | (nothing) |
| with_setup | print(x) |

`<?>` captures the statement if one exists, or produces an empty list if not — letting you handle both cases.

### `<~>` — Negation (Must Be Empty)

The inverse of `<+>`: require that *nothing* exists at that position.

```sql
-- Find functions whose body is ONLY a return (nothing before it)
SELECT captures['F'][1].name as func
FROM ast_match('app.py',
    'def __F__(__):
        %__<EMPTY~>__%
        return __R__',
    'python')
ORDER BY start_line;
```

This finds the simplest functions — the ones with no setup, no conditionals, just a direct return.

**Note:** Cardinality constraints (`<?>`, `<~>`, `<+>`) only work with **named** wildcards. Anonymous forms like `%__<?>__%` behave like `<*>`.

---

## 4. Recursive Wildcards: Searching at Any Depth

### The Gap

Variadic wildcards match siblings — nodes at the **same depth**. But real code nests things inside `if`, `try`, `for`, and `with` blocks:

```python
def nested_execute(db):
    if db:
        try:
            db.execute("SELECT 1")  # <-- 3 levels deep
        except:
            pass
```

A variadic pattern like `def __F__(__): %__<*>__% db.execute(__)` would miss this because `db.execute()` isn't a direct body statement — it's buried inside `if > try`.

### `<**>` — Any Depth

The recursive wildcard `<**>` matches at **any depth** within the subtree:

```sql
SELECT
    captures['F'][1].name as func,
    start_line
FROM ast_match('app.py',
    'def __F__(__):
        %__<D**>__%
        return __',
    'python')
ORDER BY start_line;
```

This finds every function containing a `return` statement at any nesting level — whether it's a direct `return x` or a `return` buried inside `if > for > if`.

### Named Recursive Captures

When you name a recursive wildcard, it captures **all descendants** in the scope:

```sql
SELECT
    captures['F'][1].name as func,
    length(captures['D']) as descendant_nodes
FROM ast_match('app.py',
    'def __F__(__):
        %__<D**>__%
        return __',
    'python')
ORDER BY start_line
LIMIT 5;
```

| func | descendant_nodes |
|------|-----------------|
| load | 27 |
| get | 10 |
| connect | 20 |
| execute | 23 |
| fetch_all | 17 |

The count tells you roughly how complex each function's body is before the return.

---

## 5. Relational Operators: Structural Questions Without Patterns

Pattern matching is powerful but sometimes you just want to ask: "does function X contain node type Y?" Relational operators answer these questions directly.

### `ast_has` — Contains

Find functions that contain a return statement at any depth:

```sql
SELECT name, start_line, end_line
FROM ast_has('app.py', 'function_definition', 'return_statement')
ORDER BY start_line;
```

| name | start_line | end_line |
|------|------------|----------|
| load | 16 | 19 |
| get | 21 | 22 |
| connect | 32 | 34 |
| execute | 36 | 39 |
| fetch_all | 41 | 43 |
| get_user | 56 | 63 |
| create_user | 65 | 70 |
| delete_user | 72 | 79 |
| search_users | 81 | 86 |
| export_users | 96 | 97 |
| process_file | 100 | 106 |
| transform | 109 | 119 |
| validate_email | 122 | 129 |
| retry | 132 | 141 |

14 of 20 functions have returns. Which 6 don't?

### `ast_not_has` — Does Not Contain

```sql
SELECT name, start_line, end_line
FROM ast_not_has('app.py', 'function_definition', 'return_statement')
ORDER BY start_line;
```

| name | start_line | end_line |
|------|------------|----------|
| \_\_init\_\_ | 12 | 14 |
| \_\_init\_\_ | 27 | 30 |
| close | 45 | 48 |
| \_\_init\_\_ | 53 | 54 |
| bulk_import | 88 | 94 |
| main | 144 | 159 |

The three `__init__` methods (constructors), `close` (cleanup), `bulk_import` (side effects), and `main` (entry point). Makes sense — these are action-oriented functions, not value-returning ones.

### `ast_has` with Name Filters

Add a name filter to the descendant to narrow the search:

```sql
-- Which functions call execute()?
SELECT name, start_line
FROM ast_has('app.py', 'function_definition', 'call', 'execute')
ORDER BY start_line;
```

| name | start_line |
|------|------------|
| execute | 36 |
| fetch_all | 41 |
| create_user | 65 |
| delete_user | 72 |

Four functions touch the database directly. The others go through `fetch_all` or don't use the database at all.

### `ast_inside` — The Inverse Direction

`ast_has` returns the *ancestor*. `ast_inside` returns the *descendant*:

```sql
-- What does validate_email return?
SELECT peek, start_line
FROM ast_inside('app.py',
    'return_statement', 'function_definition', 'validate_email')
ORDER BY start_line;
```

| peek | start_line |
|------|------------|
| return False | 125 |
| return False | 128 |
| return len(parts[1]) > 0 | 129 |

Three return paths: two early `False` returns for invalid input, and a final validation check. You can read the function's control flow without opening the file.

### `ast_precedes` / `ast_follows` — Sibling Order

Find nodes based on their position relative to siblings:

```sql
-- What imports come before the first class?
SELECT type, name, start_line
FROM ast_precedes('app.py', 'import_statement', 'class_definition')
ORDER BY start_line;
```

| type | name | start_line |
|------|------|------------|
| import_statement | os | 2 |
| import_statement | json | 3 |

```sql
-- What standalone functions are defined after the classes?
SELECT name, start_line
FROM ast_follows('app.py', 'function_definition', 'class_definition')
ORDER BY start_line;
```

| name | start_line |
|------|------------|
| process_file | 100 |
| transform | 109 |
| validate_email | 122 |
| retry | 132 |
| main | 144 |

This gives you the file's layout at a glance: imports, then classes, then utility functions, then `main`.

---

## 6. Combining Tools

The real power comes from composing these operators with SQL. Each operator returns a standard table, so you can JOIN, filter, and aggregate freely.

### Finding Error-Handling Gaps

Which functions call `execute()` but don't wrap it in `try/except`?

```sql
WITH db_callers AS (
    SELECT name, start_line, node_id
    FROM ast_has('app.py', 'function_definition', 'call', 'execute')
),
has_try AS (
    SELECT node_id
    FROM ast_has('app.py', 'function_definition', 'try_statement')
)
SELECT d.name, d.start_line
FROM db_callers d
LEFT JOIN has_try t ON d.node_id = t.node_id
WHERE t.node_id IS NULL
ORDER BY d.start_line;
```

Any functions that appear here are database callers without error handling — potential reliability issues.

### Functions That Both Read and Write

```sql
WITH readers AS (
    SELECT name, node_id, start_line
    FROM ast_has('app.py', 'function_definition', 'call', 'fetch_all')
),
writers AS (
    SELECT node_id
    FROM ast_has('app.py', 'function_definition', 'call', 'execute')
)
SELECT r.name, r.start_line
FROM readers r
JOIN writers w ON r.node_id = w.node_id
ORDER BY r.start_line;
```

Functions that appear in both sets do reads and writes — worth reviewing for transaction safety.

### Finding Dangerous Patterns

Combine `ast_has` with `read_ast` to find specific code smells:

```sql
-- Find f-string SQL queries (potential injection)
SELECT type, peek, start_line
FROM read_ast('app.py')
WHERE type = 'string'
  AND peek LIKE 'f"SELECT%'
ORDER BY start_line;
```

| type | peek | start_line |
|------|------|------------|
| string | f"SELECT * FROM users WHERE name LIKE '%{query}%'" | 84 |

Line 84 has an f-string building a SQL query with direct string interpolation — a textbook SQL injection vulnerability.

### Module Structure at a Glance

```sql
-- Constants defined after imports
SELECT name, peek, start_line
FROM ast_follows('app.py', 'expression_statement', 'import_from_statement')
ORDER BY start_line;
```

| name | peek | start_line |
|------|------|------------|
| | MAX_RETRIES = 3 | 7 |
| | DEFAULT_TIMEOUT = 30 | 8 |

### Cross-Language: JavaScript Too

These tools work across all 27 supported languages:

```sql
-- Which JS methods use await?
SELECT name, start_line, peek
FROM ast_has('app.js', 'method_definition', 'await_expression')
ORDER BY start_line;
```

| name | start_line | peek |
|------|------------|------|
| login | 13 | async login(username, password) { |

---

## 7. Beyond grep: Questions Only Structure Can Answer

Every query so far could — with enough patience — be approximated by reading the code manually. But some questions are structurally impossible for text-based tools like `grep` or `ripgrep`. These tools operate on **lines**. The queries below operate on **scopes** — and that's a fundamental difference.

### Negation Over Scope: "Functions That Never Call X"

grep can find `open(` calls. It *cannot* find the functions that don't contain one.

```sql
-- Which functions never call open()?
SELECT name, start_line, file_path
FROM ast_not_has('scripts/*.py', 'function_definition', 'call', 'open')
ORDER BY file_path, start_line;
```

This returns 27 functions across 15 files. To do this with grep, you'd need to: (1) find every function boundary, (2) search each one individually, (3) report only those without a match. grep has no concept of "function boundary" — it doesn't know where `def generate_header` ends and the next function begins.

### Scoped Containment: "Returns Inside This Specific Function"

`grep return embed_sql_macros.py` gives you every return in the file. Which function does each one belong to?

```sql
-- Pair each return with its enclosing function
SELECT
    a.name as in_function,
    r.peek as returns,
    r.start_line
FROM ast_has('scripts/embed_sql_macros.py',
    'function_definition', 'return_statement') a
JOIN read_ast('scripts/embed_sql_macros.py') r
  ON r.type = 'return_statement'
  AND r.node_id > a.node_id
  AND r.node_id <= a.node_id + a.descendant_count
ORDER BY a.start_line, r.start_line;
```

| in_function | returns | start_line |
|-------------|---------|------------|
| escape_raw_string_delimiter | return content | 19 |
| split_content | return [content] | 24 |
| split_content | return chunks | 41 |

`split_content` has two return paths — an early return for small content and a final return after chunking. grep gives you three decontextualized lines; this query gives you three lines each paired with their enclosing function.

### Structural Conjunction: "Functions With Both X and Y"

grep can find lines with `for` and lines with `try`. It cannot determine they're in the same function.

```sql
-- Functions containing BOTH a for-loop AND a try-block
SELECT f.name, f.start_line, f.file_path
FROM ast_has('scripts/*.py', 'function_definition', 'for_statement') f
JOIN ast_has('scripts/*.py', 'function_definition', 'try_statement') t
  ON f.node_id = t.node_id
ORDER BY f.file_path, f.start_line;
```

| name | start_line | file_path |
|------|------------|-----------|
| update_file | 25 | scripts/update_test_names.py |

Only one function in the entire scripts directory iterates *and* handles errors. That's a finding — it means every other loop runs unprotected.

### Structural Difference: "Has X but Not Y"

This is conjunction + negation — the query grep absolutely cannot express: "functions that have for-loops but no try-block."

```sql
-- Functions with for-loops but NO error handling
WITH has_for AS (
    SELECT name, node_id, start_line, file_path
    FROM ast_has('scripts/*.py', 'function_definition', 'for_statement')
),
has_try AS (
    SELECT node_id
    FROM ast_has('scripts/*.py', 'function_definition', 'try_statement')
)
SELECT h.name, h.start_line, h.file_path
FROM has_for h
LEFT JOIN has_try t ON h.node_id = t.node_id
WHERE t.node_id IS NULL
ORDER BY h.file_path, h.start_line;
```

| name | start_line | file_path |
|------|------------|-----------|
| replace | 38 | scripts/bootstrap-template.py |
| replace_everywhere | 84 | scripts/bootstrap-template.py |
| remove_placeholder | 109 | scripts/bootstrap-template.py |
| split_content | 21 | scripts/embed_sql_macros.py |
| generate_header | 43 | scripts/embed_sql_macros.py |
| get_query_result | 9 | scripts/fix_clean_api_test.py |
| generate_def_file | 141 | scripts/generate_def_file.py |
| update_test_file | 39 | scripts/migrate_tests_to_semantic_types.py |
| update_sql_macro | 107 | scripts/migrate_tests_to_semantic_types.py |
| main | 220 | scripts/migrate_tests_to_semantic_types.py |
| \_\_init\_\_ | 21 | scripts/parquet_index_manager.py |
| main | 295 | scripts/parquet_index_manager.py |
| main | 65 | scripts/update_test_names.py |

13 functions iterate over data without error handling. For a code review, this is an actionable finding — each one is a candidate for adding `try/except` around the loop body.

### Depth-Sensitive Nesting: "open() Inside a for-loop"

grep cannot distinguish `open()` *inside* a for-loop from `open()` *next to* a for-loop in the same file. Both are just lines matching the pattern.

```sql
-- for-loops that contain open() calls (file I/O inside iteration)
SELECT name, start_line, end_line, file_path
FROM ast_has('scripts/*.py', 'for_statement', 'call', 'open')
ORDER BY file_path, start_line;
```

| name | start_line | end_line | file_path |
|------|------------|----------|-----------|
| | 24 | 70 | scripts/add_parsing_function.py |
| | 71 | 96 | scripts/embed_sql_macros.py |
| | 109 | 116 | scripts/embed_sql_macros.py |
| | 29 | 32 | scripts/parquet_index_manager.py |
| | 10 | 24 | scripts/remove_all_parsing_context.py |
| | 26 | 36 | scripts/remove_parsing_context.py |

6 for-loops open files during iteration. These are the hotspots for file-handle exhaustion if the loop body doesn't use `with` statements or close handles — a class of bug that text search cannot even describe, let alone find.

### Per-Function Counting: "Functions With Multiple Returns"

`grep -c return` counts per *file*. This query counts per *function*:

```sql
WITH func_returns AS (
    SELECT a.name as func_name, a.start_line, a.file_path,
           COUNT(*) as return_count
    FROM ast_has('scripts/*.py', 'function_definition', 'return_statement') a
    JOIN read_ast('scripts/*.py') r
      ON r.file_path = a.file_path
      AND r.type = 'return_statement'
      AND r.node_id > a.node_id
      AND r.node_id <= a.node_id + a.descendant_count
    GROUP BY a.name, a.start_line, a.file_path
)
SELECT func_name, return_count, start_line, file_path
FROM func_returns
WHERE return_count > 1
ORDER BY return_count DESC;
```

| func_name | return_count | start_line | file_path |
|-----------|-------------|------------|-----------|
| classify_node_type | 24 | 42 | scripts/generate_def_file.py |
| format_output | 6 | 177 | scripts/analyze_file_functions.py |
| get_query_result | 3 | 9 | scripts/fix_clean_api_test.py |
| update_file | 3 | 25 | scripts/update_test_names.py |
| split_content | 2 | 21 | scripts/embed_sql_macros.py |

`classify_node_type` has **24 return paths**. That's a function worth refactoring — and grep's per-file count would completely hide this signal inside the file-level total.

### Cross-Language Semantics: One Query, All Languages

grep needs `class\s+\w+:` for Python, `class\s+\w+\s*\{` for JavaScript, `type\s+\w+\s+struct` for Go. Semantic types handle all 27 languages with one predicate:

```sql
SELECT name, language, start_line, file_path
FROM read_ast(['app.py', 'app.js'])
WHERE is_class_definition(semantic_type)
ORDER BY file_path, start_line;
```

| name | language | start_line | file_path |
|------|----------|------------|-----------|
| AuthService | javascript | 7 | app.js |
| Config | python | 10 | app.py |
| DatabaseConnection | python | 25 | app.py |
| UserService | python | 51 | app.py |

One query, two languages, zero regex maintenance. Add Go, Rust, Java, TypeScript — same query, same results.

### Why This Matters

The common thread across all these examples: **grep operates on lines; structural queries operate on scopes**. Any question of the form "within scope X, does Y exist / not exist / relate to Z?" is fundamentally beyond what text search can express, no matter how clever the regex.

| Question shape | grep | Structural query |
|---------------|------|-----------------|
| "Find X" | yes | yes |
| "Find X in file F" | yes | yes |
| "Find X inside function F" | no | `ast_inside` |
| "Functions without X" | no | `ast_not_has` |
| "Functions with X and Y" | no | `ast_has` JOIN `ast_has` |
| "Functions with X but not Y" | no | `ast_has` LEFT JOIN `ast_not_has` |
| "X nested inside Y" | no | `ast_has(Y_type, X_type)` |
| "Count X per function" | no | `ast_has` + GROUP BY |
| "Find class in any language" | N regexes | `is_class_definition()` |

---

## Quick Reference

### Wildcards

| Syntax | Meaning | Scope |
|--------|---------|-------|
| `__X__` | Named capture, matches one node | Exact position |
| `__` | Anonymous, matches one node | Exact position |
| `%__<X*>__%` | Named, 0 or more siblings | Sibling level |
| `%__<X+>__%` | Named, 1 or more siblings | Sibling level |
| `%__<X?>__%` | Named, 0 or 1 sibling | Sibling level |
| `%__<X~>__%` | Named, must be 0 siblings | Sibling level |
| `%__<X**>__%` | Named, any depth | Full subtree |

### Relational Operators

| Operator | Question It Answers |
|----------|-------------------|
| `ast_has(src, parent_type, child_type)` | Does parent contain child? |
| `ast_not_has(src, parent_type, child_type)` | Does parent NOT contain child? |
| `ast_inside(src, child_type, parent_type)` | Find children within parent |
| `ast_precedes(src, type_a, type_b)` | Find A that comes before B |
| `ast_follows(src, type_a, type_b)` | Find A that comes after B |

### When to Use What

| Need | Tool |
|------|------|
| Extract specific captures from code | `ast_match` with `__X__` |
| Flexible function body matching | `ast_match` with `<*>` / `<+>` |
| Find patterns at any nesting depth | `ast_match` with `<**>` |
| "Does X contain Y?" | `ast_has` / `ast_not_has` |
| "What Y is inside X?" | `ast_inside` |
| "What comes before/after X?" | `ast_precedes` / `ast_follows` |
| Combine multiple structural questions | SQL JOINs on relational operators |

### Tips

- Variadic wildcards need a **concrete anchor** (a non-wildcard node at the same depth) to work correctly
- Cardinality constraints (`<?>`, `<~>`, `<+>`) require **named** wildcards
- Wildcard names must be UPPERCASE to avoid conflicting with Python's `__dunder__` methods
- Relational operators use O(1) subtree checks — they're fast even on large files
- All operators accept glob patterns: `'src/**/*.py'` searches an entire directory tree

---

## Next Steps

- [Structural Search Reference](structural-search.md) — Complete wildcard reference and more recipes
- [Pattern Matching API](pattern-matching.md) — Full `ast_match` parameter reference
- [Cookbook](cookbook.md) — More practical analysis recipes
- [Cross-Language Analysis](cross-language.md) — Using `semantic_type` for language-agnostic queries
