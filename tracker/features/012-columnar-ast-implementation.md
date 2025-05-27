# Columnar AST Implementation Strategy

**Priority**: P2 (High - Performance Critical)
**Status**: Designed, Ready for Implementation
**Related**: #011-native-ast-type

## Overview

Implement a columnar-oriented native AST representation that aligns with DuckDB's columnar architecture for maximum performance while solving the recursive type limitation.

## Key Innovation: Columnar Storage

Instead of traditional tree structures, use parallel arrays for different node properties:

```sql
CREATE TYPE ast_flat AS STRUCT(
    -- Node data columns (can be NULL for unused data)
    types VARCHAR[],              -- Node types (always present)
    texts VARCHAR[],              -- Optional: node text content
    start_rows UINTEGER[],        -- Line numbers
    end_rows UINTEGER[],
    
    -- Topology arrays (always present)
    parents UINTEGER[],           -- parent[i] = parent index of node i
    depths UINTEGER[],            -- Pre-computed for fast filtering
    
    -- Annotations (optional)
    annotation_keys VARCHAR[][],
    annotation_values JSON[][],
    
    -- Metadata
    root UINTEGER,
    node_count UINTEGER
);
```

## Benefits

1. **Vectorized Operations**: Type filtering becomes a simple array scan
2. **Memory Efficiency**: Load only needed columns
3. **Cache Locality**: Sequential memory access
4. **Null Optimization**: Don't pay for unused columns
5. **DuckDB Native**: Leverages columnar engine

## Implementation Approach

### Phase 1: Core Infrastructure
- [ ] Define `ast_flat` type
- [ ] Implement columnar parser output
- [ ] Create C++ accessor functions
- [ ] Add basic filtering operations

### Phase 2: Integration
- [ ] SQL function wrappers
- [ ] Migration from JSON format
- [ ] Compatibility layer for existing macros
- [ ] Performance benchmarks

### Phase 3: Optimization
- [ ] Vectorized type checking
- [ ] Memory-mapped large files
- [ ] Parallel parsing
- [ ] Query optimization

## Usage Example

```sql
-- Parse with minimal overhead
WITH ast AS (
    SELECT parse_ast_native(
        read_file('app.py'), 
        'python',
        include_text := false  -- Don't load text column
    ) as tree
)
-- Vectorized filtering on types array
SELECT idx, tree.types[idx + 1] as type
FROM ast, UNNEST(range(tree.node_count)) as idx  
WHERE tree.types[idx + 1] = 'function_definition';
```

## Performance Targets

- 10-100x faster type filtering vs JSON
- 5-10x less memory for large ASTs
- Sub-millisecond filtering on 100K node ASTs

## Design Documents

- Full design: `/workspace/design/columnar_ast_design.md`
- Implementation strategy: `/workspace/design/native_implementation_strategy.md`

## Next Steps

1. Prototype vectorized type filtering
2. Benchmark against current JSON implementation  
3. Design migration utilities
4. Plan incremental rollout