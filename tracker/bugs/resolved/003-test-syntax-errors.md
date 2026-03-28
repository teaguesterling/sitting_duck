# Bug: Multiple Test Syntax Errors

**Status:** Fixed
**Priority:** High
**Discovered:** 2025-05-26

## Description
Several tests have syntax errors causing failures:

### 1. Alias Reference in SELECT with Subquery
**File:** method_syntax_comprehensive.test:180
**Error:** "Alias 'func_count' referenced in a SELECT clause - but the expression has a subquery"
**Issue:** Cannot use alias in ORDER BY when it contains a subquery

### 2. Missing Query Declaration
**File:** syntax_equivalence.test:95
**Error:** "syntax error at or near 'query'"
**Issue:** Missing newline before `query III` statement after WITH clause

### 3. Invalid Column Reference
**File:** comprehensive_chain_tests.test:273
**Error:** Similar alias/subquery issue

## Fix
1. Use full expression in ORDER BY instead of alias
2. Add proper newline before query declarations
3. Review all test files for similar issues