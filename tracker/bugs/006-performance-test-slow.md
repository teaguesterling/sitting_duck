# Bug: Performance Tests Take Too Long

**Status:** Open
**Priority:** Medium
**Discovered:** 2025-05-26

## Description
The performance test suite takes 26 seconds to run, compared to 0.1 seconds for basic tests. This significantly slows down development iteration.

## Details
- Test creates a synthetic AST with 10,000 nodes
- 9 queries run against this large dataset
- Each query involves complex JSON operations and recursive macro calls

## Measurements
```
performance_tests.test: 26.083s (43.336s user time)
duckdb_ast.test: 0.113s (0.100s user time)
```

## Potential Optimizations
1. Reduce test data size (e.g., 1,000 nodes instead of 10,000)
2. Cache intermediate results within macros
3. Use more efficient JSON operations
4. Consider creating a smaller "quick performance" test and a separate "full performance" test

## Impact
- Slows down test-driven development
- Makes CI/CD pipelines slower
- May indicate that macros are not as performant as expected for large ASTs

## Notes
This is not blocking current development but should be addressed before v1.0 release.