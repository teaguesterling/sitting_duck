# Feature: Pattern-by-Example Matching

**Priority:** P2
**Complexity:** Medium
**Status:** Design Phase

## Summary

Match AST patterns by writing actual code with placeholders. The pattern is parsed into an AST, then matched against target code using our flat table structure with `depth`, `sibling_index`, and `descendant_count`.

## Motivation

Instead of learning a meta-query language (like tree-sitter S-expressions), users write patterns in the target language:

```sql
-- Find eval calls with any argument
SELECT * FROM ast_match('python_code', 'eval(_)');

-- Find if statements with specific structure
SELECT * FROM ast_match('python_code', 'if _: return _');

-- Find method calls on specific object
SELECT * FROM ast_match('python_code', 'request.get(_)');
```

## Core Insight

Our flat AST table is **isomorphic to TSTree**:
- `node_id` - DFS pre-order position
- `depth` - tree depth
- `sibling_index` - position among siblings
- `descendant_count` - subtree size (enables O(1) subtree extraction)

We can match tree patterns entirely in SQL using these columns.

## Placeholder Syntax

### User-Facing Syntax

Use `__UPPERCASE__` identifiers as wildcards. These parse correctly in all languages and are easy to recognize:

| Placeholder | Meaning |
|-------------|---------|
| `__X__` | Match any single node (and its subtree) |
| `__NAME__` | Named capture (same name = same value constraint) |
| `__ARGS__` | Match zero or more nodes (variadic, ends with `S`) |

### Examples

```python
# Pattern: eval(__X__)
# Matches: eval(x), eval("str"), eval(func()), eval(a + b)
# Won't match: eval(x, y), eval()

# Pattern: __X__.method(__Y__)
# Matches: obj.method(arg), self.method(x)

# Pattern: if __X__: return __Y__
# Matches: if cond: return val

# Pattern: for __X__ in __Y__: __STMTS__
# Matches: any for loop (variadic body)

# Pattern with captures: __OBJ__.get(__KEY__)
# Captures: OBJ=identifier, KEY=argument
```

## Wildcard Implementation

### The Cross-Language Problem

Wildcards must be valid identifiers that parse correctly in all target languages. Testing revealed:

| Syntax | Python | JavaScript | Go | Rust | Java | C++ |
|--------|--------|------------|-----|------|------|-----|
| `_` | ✓ | ✓ | ERROR | ERROR | ERROR | ✓ |
| `_1` | ✓ | ✓ | ERROR | ✓ | ERROR | ✓ |
| `$x` | ERROR | ✓ | ERROR | ERROR | ERROR | ERROR |
| `__X__` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |

**Key finding**: Dunder-style identifiers (`__UPPERCASE__`) work universally across all tested languages.

### Complete Language Compatibility

Tested across all 27 supported languages:

| Language | `__X__` Works | Context/Notes |
|----------|--------------|---------------|
| **Programming Languages** | | |
| Python | ✓ | `identifier` |
| JavaScript | ✓ | `identifier` |
| TypeScript | ✓ | `identifier` |
| Go | ✓ | `identifier` |
| Rust | ✓ | `identifier` |
| Java | ✓ | `identifier` |
| C | ✓ | `identifier` |
| C++ | ✓ | `identifier` |
| C# | ✓ | `identifier` (needs statement context) |
| Ruby | ✓ | `identifier` |
| PHP | ✓ | `name` (needs `<?php` prefix) |
| Kotlin | ✓ | `simple_identifier` |
| Swift | ✓ | `simple_identifier` |
| Dart | ✓ | `identifier` |
| Lua | ✓ | `identifier` |
| R | ✓ | `identifier` |
| Zig | ✓ | `identifier` |
| Bash | ✓ | `word` |
| **Query Languages** | | |
| SQL | ✓ | `identifier` |
| DuckDB | ✓ | `table_reference` |
| GraphQL | ✓ | `name` |
| **Config Languages** | | |
| HCL | ✓ | `identifier` |
| TOML | ✓ | `bare_key` |
| JSON | ✓ | `string_content` (in string keys) |
| **Markup Languages** | | |
| Markdown | ✓ | `inline` (in text) |
| CSS | ✓ | `identifier` (class/property names) |
| HTML | ⚠ | Attribute values only; tag names need `UPPERCASE` |

**HTML Note**: HTML tag names cannot start with underscores. Use `XPLACEHOLDER` style for tag patterns:
```html
<!-- Works: attribute values -->
<div id="__X__" class="__Y__">

<!-- Fails: tag names -->
<__X__>  <!-- ERROR -->

<!-- Use for tag patterns -->
<XTAG>content</XTAG>
```

### Design: Wildcard Detection

Two approaches for detecting wildcards in patterns:

#### Option 1: Fixed Convention (Recommended)

Use `__UPPERCASE__` as the universal wildcard convention. No configuration needed:

```sql
-- All __UPPERCASE__ identifiers are automatically wildcards
ast_match('codebase', 'eval(__X__)')
ast_match('codebase', '__OBJ__.__METHOD__(__ARG__)')

-- For HTML tag patterns, use XUPPERCASE (no underscores)
ast_match('html_code', '<XTAG __ATTR__="__VAL__">__BODY__</XTAG>')
```

**Recognition regex**: `name ~ '^__[A-Z][A-Z0-9_]*__$'`

#### Option 2: User-Specified Patterns

Allow users to specify custom wildcard patterns via an optional `wildcards` parameter:

```sql
-- Single pattern (string)
ast_match('codebase', 'eval(__X__)', wildcards := '__X__')

-- Multiple patterns (array) for mixed contexts
ast_match('html_code', '<XTAG id="__X__">', wildcards := ['__X__', 'XTAG'])

-- Regex pattern
ast_match('codebase', 'eval($arg)', wildcards := '^\$')  -- starts with $
```

The `wildcards` parameter accepts:
- `VARCHAR`: A single example wildcard (pattern derived from prefix/suffix)
- `VARCHAR[]`: Multiple example wildcards
- `VARCHAR` starting with `^`: Regex pattern for matching names

#### Recommendation

Start with **Option 1** (fixed convention) for simplicity. The `__UPPERCASE__` pattern:
- Works in 26/27 languages without issues
- Never conflicts with real code (Python dunders are lowercase)
- Requires no user configuration
- Can be extended to Option 2 later if needed

### Wildcard Discovery via Parsing

The key insight: **parse the wildcard parameter itself** to discover what wildcards look like:

```sql
-- The wildcard parameter
wildcard := '__X__'

-- Parse it:
SELECT type, name FROM parse_ast('__X__', 'python');
-- Result: identifier, '__X__'

-- Learn: wildcards are identifiers whose names start with '__' and end with '__'
```

This approach:
1. **No regex needed** - the parser tells us the wildcard's structure
2. **Language-aware** - the wildcard is validated against the target language
3. **Flexible** - users can define any parseable identifier pattern

### Wildcard Recognition Algorithm

```sql
-- Given wildcard parameter '__X__', extract the pattern
WITH wildcard_info AS (
    SELECT name as wildcard_name
    FROM parse_ast(wildcard_param, language)
    WHERE type = 'identifier'
    LIMIT 1
),
wildcard_pattern AS (
    -- Extract prefix and suffix from the wildcard name
    -- For '__X__': prefix = '__', suffix = '__', variable_part = 'X'
    SELECT
        substring(wildcard_name, 1, position('X' IN wildcard_name) - 1) as prefix,
        substring(wildcard_name, position('X' IN wildcard_name) + 1) as suffix
    FROM wildcard_info
)
-- Then match any identifier with same prefix/suffix pattern
SELECT * FROM pattern_ast
WHERE type = 'identifier'
  AND name LIKE (SELECT prefix || '%' || suffix FROM wildcard_pattern)
```

### Simplified: Fixed Dunder Convention

For simplicity, we can use a fixed convention where any `__UPPERCASE__` identifier is a wildcard:

```sql
-- Pattern nodes where name matches __[A-Z]+[A-Z0-9]*__ are wildcards
name ~ '^__[A-Z][A-Z0-9]*__$'

-- Examples of wildcards:
-- __X__, __ARG__, __FUNC__, __OBJ1__, __A2__

-- Examples of non-wildcards (won't be treated as wildcards):
-- __init__, __name__, x, _private
```

This is simple, predictable, and unlikely to conflict with real identifiers:
- Python dunders are lowercase (`__init__`, `__name__`)
- User code rarely has `__UPPERCASE__` identifiers

### Wildcard Types

| Pattern | Type | Meaning |
|---------|------|---------|
| `__X__` | Single | Match exactly one node (any type) |
| `__ARGS__` | Variadic | Match zero or more siblings (ends with `S`) |
| Same name | Capture | Same wildcard name = same value constraint |

```sql
-- Single wildcard: one argument
eval(__X__)  -- matches eval(x), eval(1+2), NOT eval(x, y)

-- Variadic: any number of arguments (name ends with S)
print(__ARGS__)  -- matches print(), print(x), print(x, y, z)

-- Capture constraint: same wildcard = same value
__X__ == __X__  -- matches: a == a, x == x, NOT: a == b
```

### Example: How Parsing Works

User pattern: `__OBJ__.__METHOD__(__ARG__)`

```sql
SELECT node_id, depth, sibling_index, type, name
FROM parse_ast('__OBJ__.__METHOD__(__ARG__)', 'python')
WHERE NOT is_syntax_only(flags);
```

Result:
```
| node_id | depth | sibling | type            | name        | is_wildcard |
|---------|-------|---------|-----------------|-------------|-------------|
| 1       | 0     | 0       | call            | __METHOD__  | true        |
| 2       | 1     | 0       | attribute       | __METHOD__  | true        |
| 3       | 2     | 0       | identifier      | __OBJ__     | true        |
| 4       | 2     | 1       | identifier      | __METHOD__  | true        |
| 5       | 1     | 1       | argument_list   | NULL        | false       |
| 6       | 2     | 0       | identifier      | __ARG__     | true        |
```

All `__UPPERCASE__` identifiers are marked as wildcards and will match any node in the target.

## Matching Algorithm

### Step 1: Parse Pattern

```sql
CREATE TEMP TABLE pattern AS
SELECT
    node_id,
    depth - (SELECT MIN(depth) FROM p WHERE is_significant(type)) as rel_depth,
    sibling_index,
    type,
    name,
    descendant_count,
    name ~ '^__[A-Z][A-Z0-9_]*__$' as is_wildcard,
    name ~ '^__[A-Z][A-Z0-9_]*S__$' as is_variadic
FROM parse_ast('eval(__X__)', 'python') p
WHERE type NOT IN ('module', 'expression_statement');  -- Strip wrapper
```

Result for `eval(__X__)`:
```
| rel_depth | sibling | type          | name   | is_wildcard |
|-----------|---------|---------------|--------|-------------|
| 0         | 0       | call          | eval   | false       |
| 1         | 0       | identifier    | eval   | false       |
| 1         | 1       | argument_list |        | false       |
| 2         | 0       | (             |        | false       |
| 2         | 1       | identifier    | __X__  | true        | ← wildcard
| 2         | 2       | )             |        | false       |
```

### Step 2: Find Candidate Roots

Find nodes in target matching pattern root:

```sql
SELECT node_id, descendant_count
FROM target
WHERE type = (SELECT type FROM pattern WHERE rel_depth = 0)
  AND (name = (SELECT name FROM pattern WHERE rel_depth = 0)
       OR (SELECT name FROM pattern WHERE rel_depth = 0) IS NULL);
```

### Step 3: Subtree Match

For each candidate, check if subtree matches pattern structure:

```sql
WITH candidate_subtree AS (
    SELECT
        t.*,
        t.depth - c.depth as rel_depth
    FROM target t, candidate c
    WHERE t.node_id >= c.node_id
      AND t.node_id <= c.node_id + c.descendant_count
)
-- Match non-wildcard pattern nodes
SELECT ...
```

### Step 4: Wildcard Handling

When a pattern node is a wildcard:
- **Single wildcard (`__X__`)**: Skip matching this position; it absorbs one logical node
- **Named wildcard (`__ARG__`)**: Same, but capture the matched subtree (can reference same name for equality)
- **Variadic (`__ARGS__`)**: Absorbs remaining siblings at that depth (name ends with `S`)

**Key insight**: A wildcard at position (rel_depth=D, sibling=S) matches the target node at the same relative position, plus all its descendants (via `descendant_count`).

## Proposed API

### Primary Function: `ast_match`

```sql
-- Returns matching subtree roots with captures
ast_match(table VARCHAR, pattern VARCHAR, language VARCHAR := NULL) -> TABLE

-- Output columns
| Column | Type | Description |
|--------|------|-------------|
| match_id | BIGINT | Unique match identifier |
| root_node_id | BIGINT | node_id of matched subtree root |
| file_path | VARCHAR | Source file |
| start_line | INTEGER | Match start line |
| end_line | INTEGER | Match end line |
| peek | VARCHAR | Matched code preview |
| captures | MAP(VARCHAR, STRUCT) | Named wildcard captures |
```

### Usage Examples

```sql
-- Find all eval calls
SELECT * FROM ast_match('codebase', 'eval(__X__)');

-- Find calls with captures
SELECT
    root_node_id,
    captures['FUNC'].peek as function_name,
    captures['ARG'].peek as first_arg
FROM ast_match('codebase', '__FUNC__(__ARG__)');

-- Combined with other filters
SELECT m.*, a.semantic_type
FROM ast_match('codebase', 'if __X__: return __Y__') m
JOIN codebase a ON a.node_id = m.root_node_id
WHERE a.file_path LIKE 'src/%';
```

### Convenience Predicates

```sql
-- Scalar predicate for WHERE clauses
ast_matches(node_id, pattern, ast_table) -> BOOLEAN

-- Example usage
SELECT * FROM codebase
WHERE type = 'call'
  AND ast_matches(node_id, 'eval(__X__)', 'codebase');
```

## Implementation Approach

### Option A: Pure SQL Macro

Implement entirely as SQL macros using CTEs and window functions.

**Pros**: No C++ code, easier to maintain
**Cons**: Complex SQL, potential performance issues

### Option B: Hybrid (Recommended)

1. **C++ function to parse pattern** → returns normalized pattern table
2. **SQL macro for matching logic** → uses pattern table + target table

```sql
-- C++ function
ast_pattern_parse(pattern VARCHAR, language VARCHAR) -> TABLE

-- SQL macro uses it
CREATE MACRO ast_match(ast_table, pattern) AS TABLE
    WITH pattern AS (SELECT * FROM ast_pattern_parse(pattern, ...))
    ...matching logic...
```

### Option C: Full C++ Implementation

Implement matching in C++ table function.

**Pros**: Maximum performance, full control
**Cons**: More code to maintain

## Matching Rules

### Structural Matching

1. **Same relative depth**: Pattern node at rel_depth D must match target node at same relative depth
2. **Same sibling order**: Pattern node at sibling S must match target node at same sibling position
3. **Type match**: Node types must be identical
4. **Name match**: Names must match (unless pattern name is NULL or wildcard)

### Wildcard Semantics

```
Pattern: eval(__X__)
         ~~~~  ~~~~
         |     |
         |     +-- wildcard matches ANY single argument node + descendants
         +-- literal: must match exactly

Target: eval(complex_expression(a, b) + other)
        ~~~~|__________________________|
        |   +-- this entire subtree matches the wildcard
        +-- literal match ✓
```

### Syntax Node Handling

Option to skip syntax-only nodes (`(`, `)`, `:`, etc.) in matching:

```sql
ast_match('code', 'eval(__X__)', ignore_syntax := true)
-- Pattern: call → identifier, argument_list → *
-- Skips: (, ), operators, etc.
```

## Edge Cases

### 1. Multiple matches in same subtree
Return all matches, even if nested.

### 2. Overlapping patterns
`__X__.__Y__(__Z__)` could match `a.b.c()` at multiple levels. Return all.

### 3. Optional elements
`try: __X__ except: __STMTS__` - should `except` clause be optional?
→ Start with exact matching; add `?` suffix later for optional.

### 4. Cross-language patterns
Same pattern works across languages where syntax is similar:
- `eval(__X__)` works in Python, JavaScript, Ruby
- Language-specific patterns like `def __NAME__(__ARGS__): __BODY__` only for Python

## Performance Considerations

1. **Pattern caching**: Parse pattern once, reuse
2. **Root filtering**: Filter candidates by root type/name first
3. **Early termination**: Stop matching subtree on first mismatch
4. **Subtree windows**: Use `descendant_count` for O(1) subtree bounds

## Comparison with Tree-sitter Queries

| Aspect | Pattern-by-Example | Tree-sitter Queries |
|--------|-------------------|---------------------|
| Syntax | Natural (write code) | S-expressions |
| Learning curve | Minimal | Moderate |
| Precision | Medium (implicit wildcards) | High (explicit) |
| Cross-language | Natural (similar syntax) | Language-specific |
| Predicates | Via SQL WHERE | Built-in (#eq?, #match?) |
| Performance | Good (pure SQL) | Excellent (compiled) |

## Example Implementation Sketch

```sql
CREATE OR REPLACE MACRO ast_match(ast_table, pattern_str) AS TABLE
WITH
    -- Parse pattern and normalize
    pattern AS (
        SELECT
            ROW_NUMBER() OVER (ORDER BY node_id) as pattern_idx,
            depth - MIN(depth) OVER () as rel_depth,
            sibling_index,
            type,
            name,
            descendant_count,
            -- Wildcard: __UPPERCASE__ pattern
            name ~ '^__[A-Z][A-Z0-9_]*__$' as is_wildcard,
            -- Variadic if ends with S (e.g., __ARGS__, __STMTS__)
            name ~ '^__[A-Z][A-Z0-9_]*S__$' as is_variadic
        FROM parse_ast(pattern_str,
            (SELECT DISTINCT language FROM query_table(ast_table) LIMIT 1))
        WHERE NOT is_syntax_only(flags)
    ),
    pattern_root AS (
        SELECT type, name, is_wildcard FROM pattern WHERE rel_depth = 0
    ),
    -- Find candidate roots in target
    candidates AS (
        SELECT node_id, depth, descendant_count, file_path
        FROM query_table(ast_table)
        WHERE type = (SELECT type FROM pattern_root)
          AND ((SELECT name FROM pattern_root) IS NULL
               OR (SELECT is_wildcard FROM pattern_root)
               OR name = (SELECT name FROM pattern_root))
    ),
    -- Check each candidate's subtree against pattern
    matches AS (
        SELECT c.node_id as root_node_id, c.file_path
        FROM candidates c
        WHERE NOT EXISTS (
            -- Find any non-wildcard pattern node that doesn't match
            SELECT 1 FROM pattern p
            WHERE p.is_wildcard = false
              AND NOT EXISTS (
                  SELECT 1 FROM query_table(ast_table) t
                  WHERE t.node_id >= c.node_id
                    AND t.node_id <= c.node_id + c.descendant_count
                    AND t.depth - c.depth = p.rel_depth
                    AND t.sibling_index = p.sibling_index
                    AND t.type = p.type
                    AND (p.name IS NULL OR t.name = p.name)
              )
        )
    )
SELECT * FROM matches;
```

## Future Extensions

1. **Capture groups**: `ast_match('code', 'def __NAME__(__PARAMS__): __BODY__')` → extract function name and params
2. **Pattern alternatives**: `eval(__X__) | exec(__X__)`
3. **Optional nodes**: `if __X__: __Y__ else?: __Z__`
4. **Semantic constraints**: `__FUNC:function__(__X__)` (constrain by semantic type)
5. **Negation**: `call NOT(eval|exec)(__X__)`

## Related

- #028-tree-sitter-pattern-matching (alternative approach)
- #analysis-helpers-v2 §3 (original proposal)
