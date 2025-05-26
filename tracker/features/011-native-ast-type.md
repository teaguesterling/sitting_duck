# Native AST Type Implementation

**Source**: Peer Review Architecture Analysis
**Priority**: P3 (Long-term improvement)
**Status**: Research

## Problem Statement

Current heavy reliance on JSON operations causes performance bottlenecks. Each `json_each()` call deserializes the entire node array.

## Proposed Solution

Implement a native AST type in DuckDB with custom operators for efficient traversal.

```cpp
// Register a native AST type
class ASTNodeType : public LogicalType {
    // Efficient in-memory representation
    // Custom operators for traversal
};
```

## Benefits

1. **Performance**: Direct memory access instead of JSON parsing
2. **Type Safety**: Compile-time checking for AST operations
3. **Memory Efficiency**: Optimized storage format
4. **Query Optimization**: DuckDB optimizer can understand AST operations

## Implementation Approach

### Phase 1: Design Native Type
- Define in-memory representation
- Plan migration from JSON
- Design operator set

### Phase 2: Implement Core Operations
- Traversal operators
- Filter operations
- Aggregation support

### Phase 3: Migration Strategy
- Backward compatibility layer
- Gradual migration path
- Performance benchmarks

## Storage vs. Computation Trade-off

Implement hybrid strategy:
```sql
-- Temporary materialization for complex queries
WITH MATERIALIZED ast_cache AS (
    SELECT * FROM read_ast('large_project/', 'python')
)
SELECT type, COUNT(*) 
FROM ast_cache 
GROUP BY type;
```

## Considerations

- Maintains compatibility with existing API
- Allows gradual migration
- Significant engineering effort required
- Best suited for after immediate performance fixes