# Peer Review Feature Integration Plan

## Overview

This document maps the peer review suggestions to our implementation plan, showing how each feature fits into our architecture.

## High Priority Features (Peer's Top 5)

### 1. ✅ `ast_get_source()` - **HIGHEST PRIORITY**
- **Status**: Ready to implement
- **Tracker**: `/tracker/features/013-ast-get-source.md`
- **Implementation**: Simple - we have positions, just need to store source
- **Impact**: Transforms usability immediately

### 2. ✅ Parse-Time Filtering (`only_types`)
- **Status**: Design complete
- **Tracker**: `/tracker/features/014-filter-at-parse-time.md`
- **Implementation**: Modify traversal to skip non-matching nodes
- **Impact**: 5-50x memory reduction on large files

### 3. ✅ `ast_get_parent_chain()`
- **Status**: Ready to implement
- **Tracker**: `/tracker/features/015-ast-get-parent-chain.md`
- **Implementation**: Add parent_id tracking, recursive CTE
- **Impact**: Enables scope understanding

### 4. ✅ `ast_find_references()`
- **Status**: Design phase
- **Tracker**: `/tracker/features/016-ast-find-references.md`
- **Implementation**: Start simple (name matching), evolve to scope-aware
- **Impact**: Core navigation feature

### 5. ✅ `ast_get_calls()`
- **Status**: Ready to implement
- **Tracker**: `/tracker/features/017-ast-get-calls.md`
- **Implementation**: Extract call expressions from subtree
- **Impact**: Dependency analysis, complexity metrics

## Integration with Our Architecture

### Annotation System Synergy

Most peer features become annotations:
```json
{
    "type": "function_definition",
    "annotations": {
        "source": "def foo():\n    pass",  // ast_get_source
        "parent_chain": ["module", "class"], // ast_get_parent_chain
        "calls": ["bar", "baz"],            // ast_get_calls
        "references": [...]                  // ast_find_references
    }
}
```

### Columnar AST Benefits

The columnar design accelerates these features:
- `ast_get_source()`: Direct array access by node index
- Parent chain: Follow parent indices without recursion
- Type filtering: Vectorized array operations
- References: Parallel name scanning

## Implementation Order

### Sprint 1: Immediate Value (Week 1)
1. `ast_get_source()` - Highest impact, easiest implementation
2. Basic `ast_get_calls()` - Simple subtree traversal

### Sprint 2: Navigation (Week 2)  
3. `ast_get_parent_chain()` - Enables scope features
4. Parse-time filtering - Performance improvement

### Sprint 3: Advanced (Week 3)
5. `ast_find_references()` - More complex, needs scope handling
6. Integration with annotation system

## Other Peer Suggestions to Consider

### Medium Priority
- `ast_get_comment_before()` - Extract preceding comments
- `ast_get_definitions()` - All definitions in scope
- `ast_get_imports()` - Normalized import extraction
- `ast_matches_pattern()` - Structural pattern matching

### Lower Priority  
- `ast_find_similar()` - Structural similarity
- `ast_get_coupling()` - Measure relationships
- `ast_find_clones()` - Duplicate detection

### Performance Features
- ✅ `ast_cache_create()` - Persistent caching
- `ast_explain()` - Query explanation

## Success Metrics

1. **ast_get_source()** ships in next release
2. 5 peer features implemented in 3 weeks
3. Performance: <100ms on 10K node files
4. User feedback: "This is what we needed!"

## Conclusion

The peer review perfectly complements our architecture:
- Annotation system provides the framework
- Columnar AST provides the performance
- These features provide the value

Focus on `ast_get_source()` first - it's the gateway drug that will get users hooked!