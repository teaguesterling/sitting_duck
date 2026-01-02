# Bug: is_syntax_only Flag Not Set for PARSER_DELIMITER Nodes

**Status:** Open
**Priority:** Medium
**Discovered:** 2025-01-01

## Description

Nodes with semantic type `PARSER_DELIMITER` (e.g., `(`, `)`, `,`, `:`, `;`) have `flags = 0` and `is_syntax_only(flags)` returns `false`. This is inconsistent - these nodes are purely syntactic and should have the syntax-only flag set.

## Reproduction

```sql
SELECT type, semantic_type_to_string(semantic_type), flags, is_syntax_only(flags)
FROM parse_ast('eval(x)', 'python')
WHERE type IN ('(', ')');
```

**Actual:**
```
│ (  │ PARSER_DELIMITER │ 0 │ false │
│ )  │ PARSER_DELIMITER │ 0 │ false │
```

**Expected:**
```
│ (  │ PARSER_DELIMITER │ 1 │ true  │
│ )  │ PARSER_DELIMITER │ 1 │ true  │
```

## Impact

- Pattern matching code must hardcode punctuation types instead of using `is_syntax_only()`
- Users cannot reliably filter out syntax-only nodes using the intended API
- Inconsistency between semantic type (PARSER_DELIMITER) and flags

## Affected Code

The issue is likely in the semantic type assignment code where flags should be set when semantic type is `PARSER_DELIMITER` (or other syntax-only types like `PARSER_KEYWORD`, `PARSER_OPERATOR`).

## Workaround

For now, check semantic type directly:

```sql
-- Instead of:
WHERE NOT is_syntax_only(flags)

-- Use:
WHERE semantic_type NOT IN (
    semantic_type_code('PARSER_DELIMITER'),
    semantic_type_code('PARSER_KEYWORD'),
    semantic_type_code('PARSER_OPERATOR')
)
```

Or add a new predicate `is_parser_syntax(semantic_type)`.

## Suggested Fix

1. In semantic type assignment, set the syntax-only flag when semantic type is a PARSER_* type
2. Or create `is_syntax(semantic_type)` predicate that checks semantic type directly
