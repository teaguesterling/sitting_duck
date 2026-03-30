# Pattern Matching

Find code structures using pattern-by-example matching with wildcards.

## Overview

Pattern matching lets you write code patterns with wildcards to find matching AST structures. Instead of manually constructing tree queries, write the code pattern you're looking for and let the system find matches.

```sql
-- Find all eval() calls and capture their arguments
SELECT * FROM ast_match('my_table', 'eval(__X__)', 'python');
```

## Wildcard Syntax

### Simple Wildcards

Simple wildcards match nodes at exact structural positions:

| Pattern | Description |
|---------|-------------|
| `__X__` | Named wildcard - captures matched node as 'X' |
| `__` | Anonymous wildcard - matches any node, no capture |

Wildcards use UPPERCASE letters to distinguish from Python's `__dunder__` methods (which use lowercase).

```sql
-- Find print calls with any argument
SELECT * FROM ast_match('code', 'print(__X__)', 'python');

-- Find 3-arg calls: capture func and last arg, ignore middle
SELECT * FROM ast_match('code', '__F__(__, 2, __X__)', 'python');
```

### Extended Wildcards

Extended wildcards add rules for flexible matching:

| Pattern | Description |
|---------|-------------|
| `%__X<*>__%` | Named variadic: matches 0 or more siblings at this level |
| `%__X<+>__%` | Named variadic: matches 1 or more siblings at this level |
| `%__<*>__%` | Anonymous variadic: matches 0+ siblings (no capture) |
| `%__<+>__%` | Anonymous variadic: matches 1+ siblings (no capture) |
| `%__X<type=T>__%` | Type constraint: only match nodes of type T |

```sql
-- Find functions with ANY body content before a return
SELECT * FROM ast_match('code',
    'def __F__(__):
        %__BODY<*>__%
        return __Y__',
    'python');
```

## Basic Usage

### Setup

First, load the pattern matching macros and create an AST table:

```sql
.read src/sql_macros/pattern_matching.sql

CREATE TABLE code AS
SELECT * FROM read_ast('src/**/*.py', ignore_errors := true);
```

### Finding Patterns

```sql
-- Find function calls
SELECT match_id, file_path, start_line, peek
FROM ast_match('code', '__F__(__X__)', 'python');

-- Find nested calls like len(str(__X__))
SELECT * FROM ast_match('code', 'len(str(__X__))', 'python');

-- Find assert statements with messages
SELECT * FROM ast_match('code', 'assert __, __MSG__', 'python');
```

### Working with Captures

Captures are returned as a MAP of structs:

```sql
SELECT
    match_id,
    captures['F'].name as func_name,
    captures['F'].peek as func_code,
    captures['X'].peek as argument
FROM ast_match('code', '__F__(__X__)', 'python')
LIMIT 10;
```

Each capture contains:
- `capture` - The capture name
- `node_id` - AST node ID
- `type` - Node type (e.g., 'identifier', 'call')
- `name` - Extracted name (if applicable)
- `peek` - Source code preview
- `start_line`, `end_line` - Location

### Unnesting Captures

For flat output, unnest the captures map:

```sql
SELECT
    m.match_id,
    m.peek as match_code,
    c.key as capture_name,
    c.value.peek as captured_code
FROM ast_match('code', '__F__(__X__, __Y__)', 'python') m,
     LATERAL (SELECT unnest(map_entries(m.captures))) c;
```

## Variadic Patterns

Variadic wildcards enable matching structures with variable-length content.

### The Problem

Without variadics, patterns require exact structural match:

```sql
-- This matches 0 functions! Real functions have docstrings,
-- multiple statements, etc.
SELECT count(*)
FROM ast_match('code', 'def __F__(__): return __X__', 'python');
```

### The Solution

Use `%__NAME<*>__%` to match 0+ siblings at that level:

```sql
-- This finds functions with return statements (regardless of body content)
SELECT count(*)
FROM ast_match('code',
    'def __F__(__):
        %__BODY<*>__%
        return __X__',
    'python');
```

### Variadic Semantics

- `<*>` matches **0 or more** siblings at the same depth level
- `<+>` matches **1 or more** siblings at the same depth level
- `<**>` matches at **any depth** within the subtree (see below)
- Variadics match horizontally (siblings), not vertically (descendants)
- Variadic captures return LIST-valued results

### Recursive Matching (`<**>`)

The `<**>` wildcard enables matching descendants at **any depth**, unlike `<*>` which only matches siblings at the same level:

```sql
-- Find functions containing db.execute() at ANY depth (inside if, try, loops, etc.)
SELECT captures['F'][1].name, start_line
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<**>__%
    db.execute(__Y__)');
```

Compare with `<*>` which only finds functions where `db.execute()` is a direct body statement:

```sql
-- <*> only matches at the same depth level
SELECT * FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__<*>__%
    db.execute(__Y__)');
```

Named `<**>` captures collect all descendants in the scope region:

```sql
-- Capture the body content with a named recursive wildcard
SELECT captures['F'][1].name, length(captures['DEEP'])
FROM ast_match('src/**/*.py',
    'def __F__(__):
    %__DEEP<**>__%
    return __X__');
```

### Relational Operators

Standalone macros for structural queries without full pattern matching:

```sql
-- ast_has: find ancestor nodes containing a descendant type at any depth
SELECT name, start_line
FROM ast_has('src/**/*.py', 'function_definition', 'return_statement');

-- ast_has with name filter: functions containing calls to 'execute'
SELECT name, start_line
FROM ast_has('src/**/*.py', 'function_definition', 'call', 'execute');

-- ast_inside: find descendant nodes within an ancestor type
SELECT peek, start_line
FROM ast_inside('src/**/*.py', 'return_statement', 'function_definition', 'my_func');

-- ast_precedes: nodes that come before a sibling of given type
SELECT * FROM ast_precedes('src/**/*.py', 'comment', 'function_definition');

-- ast_follows: nodes that come after a sibling of given type
SELECT * FROM ast_follows('src/**/*.py', 'return_statement', 'if_statement');
```

## Parameters

### match_syntax

Include punctuation/delimiters in matching:

```sql
-- Default: punctuation is ignored
SELECT * FROM ast_match('code', '__F__(__X__)', 'python');

-- With match_syntax: parentheses must match exactly
SELECT * FROM ast_match('code', '__F__(__X__)', 'python',
    match_syntax := true);
```

### match_by

Choose matching strategy:

```sql
-- Default: match on tree-sitter type names
SELECT * FROM ast_match('code', '__F__(__X__)', 'python',
    match_by := 'type');

-- Cross-language: match on semantic types
SELECT * FROM ast_match('code', '__F__(__X__)', 'python',
    match_by := 'semantic_type');
```

### depth_fuzz

Allow flexibility in depth matching for cross-language patterns:

```sql
-- Allow +/- 1 level of depth difference
SELECT * FROM ast_match('code', '__F__(__X__)', 'python',
    depth_fuzz := 1);
```

## Cross-Language Matching

Use `match_by := 'semantic_type'` for patterns that work across languages:

```sql
-- Python pattern matches function calls in any language
SELECT * FROM ast_match('js_code', '__F__(__X__)', 'python',
    match_by := 'semantic_type');
```

Note: The pattern language determines structure, semantic types enable cross-language matching.

## Examples

### Security: Find eval/exec Calls

```sql
SELECT
    file_path,
    start_line,
    captures['X'].peek as dangerous_input
FROM ast_match('code', 'eval(__X__)', 'python')
UNION ALL
SELECT
    file_path,
    start_line,
    captures['X'].peek
FROM ast_match('code', 'exec(__X__)', 'python');
```

### Find Functions Returning Booleans

```sql
SELECT
    captures['F'].name as func_name,
    file_path,
    start_line
FROM ast_match('code',
    'def __F__(__):
        %__<*>__%
        return True',
    'python')
UNION ALL
SELECT
    captures['F'].name,
    file_path,
    start_line
FROM ast_match('code',
    'def __F__(__):
        %__<*>__%
        return False',
    'python');
```

### Find Try/Except with Bare Except

```sql
SELECT file_path, start_line, peek
FROM ast_match('code',
    'try:
        __
    except:
        __',
    'python');
```

### Find Assertions Without Messages

```sql
SELECT file_path, start_line, peek
FROM ast_match('code', 'assert __X__', 'python')
WHERE captures['X'].type != 'tuple';  -- tuple would indicate message
```

## Inspecting Patterns

Use `ast_pattern` to see how your pattern is parsed:

```sql
SELECT * FROM ast_pattern('def __F__(__): return __X__', 'python');
```

This shows each pattern node with:
- `rel_depth` - Depth relative to pattern root
- `sibling_index` - Position among siblings
- `pattern_type` - AST node type
- `is_wildcard` - Whether it's a wildcard
- `capture_name` - Name to capture as (or NULL)

## Limitations

1. **Structural matching**: Patterns match AST structure, not text
2. **No regex in patterns**: Wildcards match nodes, not text patterns
3. **Language-specific parsing**: Pattern is parsed as valid code in the specified language
4. **Recursive capture quality**: `<**>` captures at deeply nested positions may pick up nodes at unintended depths — match correctness is reliable, but captured values may need filtering

## Next Steps

- [Cross-Language Analysis](cross-language.md) - Using semantic types
- [Cookbook](cookbook.md) - Practical code analysis recipes
- [Semantic Types](semantic-types.md) - Understanding type classifications
