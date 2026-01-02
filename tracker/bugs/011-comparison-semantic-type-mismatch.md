# Bug: Comparison expressions have wrong semantic type in JS/Go

**Status:** Open
**Priority:** Medium
**Discovered:** 2025-01-02

## Description

JavaScript and Go mark comparison expressions (like `a < b`) with `OPERATOR_ARITHMETIC` instead of `OPERATOR_COMPARISON`, while Python correctly uses `OPERATOR_COMPARISON`.

This breaks cross-language pattern matching for comparisons.

## Reproduction

```sql
-- Python: correct
SELECT type, semantic_type_to_string(semantic_type)
FROM parse_ast('a < b', 'python') WHERE depth = 2;
-- comparison_operator | OPERATOR_COMPARISON

-- JavaScript: incorrect
SELECT type, semantic_type_to_string(semantic_type)
FROM parse_ast('a < b', 'javascript') WHERE depth = 2;
-- binary_expression | OPERATOR_ARITHMETIC  -- Should be OPERATOR_COMPARISON

-- Go: incorrect
SELECT type, semantic_type_to_string(semantic_type)
FROM parse_ast('a < b', 'go') WHERE depth = 2;
-- binary_expression | OPERATOR_ARITHMETIC  -- Should be OPERATOR_COMPARISON
```

## Expected

All languages should use `OPERATOR_COMPARISON` for comparison expressions.

## Impact

Cross-language pattern matching for comparisons fails:
```sql
-- Only matches Python, not JS/Go
SELECT * FROM ast_match('code', '__A__ < __B__', 'python', match_by := 'semantic_type');
```

## Suggested Fix

Update semantic type mappings for JavaScript and Go to check the operator token:
- `<`, `>`, `<=`, `>=`, `==`, `!=` → `OPERATOR_COMPARISON`
- `+`, `-`, `*`, `/` → `OPERATOR_ARITHMETIC`
