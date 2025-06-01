# Phase 3: Immediate Next Steps

## Priority 1: Complete Language Handler Implementations
Currently only Python has a full ParseFile implementation. Need to implement for:
- JavaScript
- C++  
- Rust

Each needs:
- Full taxonomy support (KIND, semantic IDs)
- Proper name extraction based on language patterns
- Node type normalization

## Priority 2: Test & Validate Core Functionality
Create comprehensive tests for:
- Cross-language function finding
- Subtree extraction correctness
- Performance with large files
- Edge cases (empty files, syntax errors)

## Priority 3: Implement "Find Function X" Demo
The canonical use case that shows the value:
```sql
-- Find all implementations of 'authenticate' across languages
SELECT 
    file_path,
    language,
    ast_extract_subtree(nodes, function_node) as implementation
FROM read_ast_objects('src/**/*')
WHERE /* function named authenticate exists */
```

## Priority 4: AST Struct Upgrade
Update `read_ast_objects` to return the new nested AST struct format:
```
ast: {
    source: {file_path, language},
    nodes: [{
        node_id,
        type: {raw, normalized},
        name: {qualified, simple, anonymous},
        file_position: {...},
        tree_position: {...},
        subtree: {children_count, descendant_count},
        kind, universal_flags, semantic_id
    }]
}
```

## Priority 5: Documentation & Examples
- Create simple examples for common use cases
- Document the KIND taxonomy
- Explain the monad pattern with AST structs
- Show performance comparisons vs grep

## Future Considerations
- Global AST index across entire codebase
- Integration with git for temporal analysis
- Query optimization for large-scale analysis
- VSCode/IDE integration for "find similar code"