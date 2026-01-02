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

Use valid identifiers as wildcards (parsed normally by tree-sitter):

| Placeholder | Meaning |
|-------------|---------|
| `_` | Match any single node (and its subtree) |
| `_1`, `_2`, ... | Named captures (can be referenced/compared) |
| `___` (triple underscore) | Match zero or more nodes (variadic) |

### Examples

```python
# Pattern: eval(_)
# Matches: eval(x), eval("str"), eval(func()), eval(a + b)
# Won't match: eval(x, y), eval()

# Pattern: _.method(_)
# Matches: obj.method(arg), self.method(x)

# Pattern: if _: return _
# Matches: if cond: return val

# Pattern: for _ in _: ___
# Matches: any for loop (variadic body)
```

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
    name IN ('_', '_1', '_2', '_3', '___') as is_wildcard,
    name = '___' as is_variadic
FROM parse_ast('eval(_)', 'python') p
WHERE type NOT IN ('module', 'expression_statement');  -- Strip wrapper
```

Result for `eval(_)`:
```
| rel_depth | sibling | type          | name | is_wildcard |
|-----------|---------|---------------|------|-------------|
| 0         | 0       | call          | eval | false       |
| 1         | 0       | identifier    | eval | false       |
| 1         | 1       | argument_list |      | false       |
| 2         | 0       | (             |      | false       |
| 2         | 1       | identifier    | _    | true        | ← wildcard
| 2         | 2       | )             |      | false       |
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
- **Single wildcard (`_`)**: Skip matching this position; it absorbs one logical node
- **Named wildcard (`_1`)**: Same, but capture the matched subtree
- **Variadic (`___`)**: Absorbs remaining siblings at that depth

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
SELECT * FROM ast_match('codebase', 'eval(_)');

-- Find calls with captures
SELECT
    root_node_id,
    captures['_1'].peek as function_name,
    captures['_2'].peek as first_arg
FROM ast_match('codebase', '_1(_2)');

-- Combined with other filters
SELECT m.*, a.semantic_type
FROM ast_match('codebase', 'if _: return _') m
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
  AND ast_matches(node_id, 'eval(_)', 'codebase');
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
Pattern: eval(_)
         ~~~~
         |  |
         |  +-- wildcard matches ANY single argument node + descendants
         +-- literal: must match exactly

Target: eval(complex_expression(a, b) + other)
        ~~~~|__________________________|
        |   +-- this entire subtree matches the wildcard
        +-- literal match ✓
```

### Syntax Node Handling

Option to skip syntax-only nodes (`(`, `)`, `:`, etc.) in matching:

```sql
ast_match('code', 'eval(_)', ignore_syntax := true)
-- Pattern: call → identifier, argument_list → *
-- Skips: (, ), operators, etc.
```

## Edge Cases

### 1. Multiple matches in same subtree
Return all matches, even if nested.

### 2. Overlapping patterns
`_._(_)` could match `a.b.c()` at multiple levels. Return all.

### 3. Optional elements
`try: _ except: ___` - should `except` clause be optional?
→ Start with exact matching; add `?` suffix later for optional.

### 4. Cross-language patterns
Same pattern works across languages where syntax is similar:
- `eval(_)` works in Python, JavaScript, Ruby
- Language-specific patterns like `def _(_): ___` only for Python

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
            name IN ('_') OR name LIKE '\_%' ESCAPE '\' as is_wildcard
        FROM parse_ast(pattern_str,
            (SELECT DISTINCT language FROM query_table(ast_table) LIMIT 1))
        WHERE NOT is_syntax_only(flags)
    ),
    pattern_root AS (
        SELECT type, name FROM pattern WHERE rel_depth = 0
    ),
    -- Find candidate roots in target
    candidates AS (
        SELECT node_id, depth, descendant_count, file_path
        FROM query_table(ast_table)
        WHERE type = (SELECT type FROM pattern_root)
          AND (name = (SELECT name FROM pattern_root)
               OR (SELECT name FROM pattern_root) IS NULL
               OR (SELECT is_wildcard FROM pattern WHERE rel_depth = 0))
    ),
    -- Check each candidate's subtree against pattern
    matches AS (
        SELECT c.node_id as root_node_id, c.file_path
        FROM candidates c
        WHERE NOT EXISTS (
            -- Find any pattern node that doesn't match
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

1. **Capture groups**: `ast_match('code', 'def _1(_2): ___')` → extract function name and params
2. **Pattern alternatives**: `eval(_) | exec(_)`
3. **Optional nodes**: `if _: _ else?: _`
4. **Semantic constraints**: `_:function(_)` (constrain by semantic type)
5. **Negation**: `call NOT(eval|exec)(_)`

## Related

- #028-tree-sitter-pattern-matching (alternative approach)
- #analysis-helpers-v2 §3 (original proposal)
