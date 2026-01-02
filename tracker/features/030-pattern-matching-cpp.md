# Feature: C++ Pattern Matching Implementation

**Status:** Proposed
**Priority:** Medium
**Related:** SQL macro prototype in `src/sql_macros/pattern_matching.sql`

## Summary

Move pattern matching from SQL macros to C++ for better performance, type flexibility, and advanced features like variadic wildcards.

## Current State (SQL Macros)

The SQL macro prototype (`ast_match`) provides:
- Pattern-by-example matching with `__WILDCARDS__`
- Anonymous wildcards (`__`) for ignore
- Cross-language matching via `match_by := 'semantic_type'`
- Capture extraction into MAP
- Configurable syntax/punctuation matching

**Limitations:**
- Performance: Multiple self-joins per pattern node
- Type system: Can't mix STRUCT and LIST(STRUCT) in same MAP
- Complexity: Parent-child tracking is awkward in flattened AST

## Proposed C++ Implementation

### Core Function

```sql
-- Table function returning matches
SELECT * FROM ast_pattern_match(
    'source_table',           -- AST table name
    'eval(__X__)',            -- Pattern string
    'python',                 -- Pattern language
    {                         -- Options struct
        match_by: 'type',
        match_syntax: false,
        wildcards: {
            'ARGS': {type: 'variadic'},
            'X': {semantic_type: 'identifier'}
        }
    }
);
```

### Output Schema

```sql
-- Single captures (current behavior)
captures['X'] → STRUCT(capture, node_id, type, name, peek, start_line, end_line)

-- Variadic captures (new)
captures['ARGS'] → LIST(STRUCT(...))
```

### Implementation Approach

1. **Pattern Compilation**
   - Parse pattern string into AST
   - Build pattern graph with wildcard metadata
   - Compile into efficient matching state machine

2. **Matching Engine**
   - Stream through target AST nodes
   - Track candidate matches with backtracking
   - Handle variadic by collecting sibling runs

3. **Capture Extraction**
   - Build capture map during matching
   - Variadic captures accumulate into lists
   - Return heterogeneous map (DuckDB supports this)

### Wildcard Modifiers (Full Design)

```sql
wildcards := {
    'X': 'single',                    -- Default: exactly one node
    'ARGS': 'variadic',               -- Zero or more siblings
    'IGNORE': 'ignore',               -- Match but don't capture
    'STMT': {
        type: 'variadic',
        max_count: 10,                -- Limit matches
        semantic_type: 'statement'    -- Filter by type
    }
}
```

### Performance Considerations

- **Pattern caching**: Compiled patterns can be cached by hash
- **Early termination**: Stop matching when pattern can't succeed
- **Index usage**: Consider B-tree on (depth, type) for candidate filtering
- **Streaming**: Process AST nodes in single pass where possible

## Migration Path

1. Keep SQL macros for simple patterns (they work well)
2. Add C++ function for advanced features
3. Eventually deprecate SQL macros or make them call C++

## Files to Create/Modify

- `src/pattern_matching/` - New directory for implementation
  - `pattern_compiler.cpp` - Parse and compile patterns
  - `pattern_matcher.cpp` - Core matching engine
  - `pattern_match_function.cpp` - DuckDB table function
- `src/include/pattern_matching.hpp` - Headers
- `test/sql/pattern_matching/` - Tests

## Open Questions

1. Should we support regex in wildcards? e.g., `__X[0-9]+__`
2. Should variadic have greedy vs lazy semantics?
3. How to handle overlapping matches?
4. Should we support backreferences? e.g., `__X__ + __X__` matches same identifier
