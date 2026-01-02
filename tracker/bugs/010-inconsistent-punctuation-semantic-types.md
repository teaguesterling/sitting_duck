# Bug: Inconsistent Semantic Types for Punctuation Across Languages

**Status:** Open
**Priority:** Medium
**Discovered:** 2025-01-01

## Description

The same punctuation characters are assigned different semantic types in different languages:

- Python: `(` and `)` → `PARSER_DELIMITER`
- JavaScript: `(` and `)` → `PARSER_PUNCTUATION`

This inconsistency breaks cross-language pattern matching when `match_syntax := true`.

## Reproduction

```sql
SELECT 'Python' as lang, type, semantic_type_to_string(semantic_type) as sem
FROM parse_ast('f(x)', 'python')
WHERE type = '(';

SELECT 'JavaScript' as lang, type, semantic_type_to_string(semantic_type) as sem
FROM parse_ast('f(x)', 'javascript')
WHERE type = '(';
```

**Actual:**
```
│ Python     │ ( │ PARSER_DELIMITER   │
│ JavaScript │ ( │ PARSER_PUNCTUATION │
```

**Expected:**
Both should use the same semantic type (likely `PARSER_DELIMITER` since parentheses delimit argument lists).

## Impact

- Cross-language pattern matching with `match_syntax := true` fails
- `is_punctuation()` predicate works (catches both types), but semantic types don't match
- Inconsistent developer experience when querying ASTs

## Scope

Need to audit all language semantic type mappings for consistency:
- Parentheses: `(`, `)`
- Brackets: `[`, `]`
- Braces: `{`, `}`
- Separators: `,`, `;`, `:`

## Suggested Fix

1. Define clear rules for PARSER_DELIMITER vs PARSER_PUNCTUATION:
   - PARSER_DELIMITER: Characters that delimit/group (parens, brackets, braces)
   - PARSER_PUNCTUATION: Statement/expression separators (semicolons, commas)
2. Audit and update all language mapping files
3. Add cross-language consistency tests
