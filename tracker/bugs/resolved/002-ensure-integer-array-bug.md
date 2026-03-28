# Bug: ensure_integer_array Returns Wrong Length for [NULL]

**Status:** Fixed
**Priority:** High
**Discovered:** 2025-05-26

## Description
The `ensure_integer_array` macro returns length 0 for an array containing a single NULL value, but should return 1.

## Current Behavior
```sql
array_length(ensure_integer_array([NULL])) -- returns 0, should return 1
```

## Expected Behavior
Should match behavior of ensure_varchar_array which correctly returns 1.

## Test Case
In edge_cases_and_errors.test:90

## Fix
Review the macro implementation to handle NULL values in arrays correctly.