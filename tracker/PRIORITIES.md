# DuckDB AST Extension - Development Priorities

**Last Updated:** 2025-05-26

## Priority Levels

- **P0 (Critical)**: Blocking issues that prevent basic functionality
- **P1 (High)**: Important features or bugs affecting user experience  
- **P2 (Medium)**: Nice-to-have features or minor bugs
- **P3 (Low)**: Future enhancements or cosmetic issues

## Current Status

✅ **Completed:**
- Clean API implementation (13 core macros)
- Test suite fixes for DuckDB 1.3
- All tests passing (19 test cases, 387 assertions)

## Immediate Priorities (Phase 1)

### P1 - Core Functionality
1. **[FEATURE] Chain Methods Implementation** - C++ function for method chaining
   - Enable `ast(nodes).get_type().count()` syntax
   - Already have ast() entrypoint, need chain methods
   
2. **[FEATURE] Language Support** - Add JavaScript, C++, SQL
   - Core value proposition of the extension
   - Tree-sitter grammars ready
   
3. **[FEATURE] Performance Optimization** - C++ hot-path macros
   - Critical for 10K+ node files
   - 10-100x improvement expected

### P2 - Quality & UX
1. **[FEATURE] Error Context Tracking** - Enhanced error messages
   - Add file path, line numbers to errors
   - Critical for debugging
   
2. **[FEATURE] Source Code Extraction** - ast_source_* functions
   - Extract code snippets with context
   - Highly requested by AI agents
   
3. **[FEATURE] Short Names Registration** - Unprefixed aliases
   - Better ergonomics for interactive use
   - Optional feature

## Medium-term Priorities (Phase 2)

### P1 - Performance at Scale
1. **[FEATURE] Filter Pushdown** - Parse-time filtering
   - Reduce nodes loaded into memory
   - Essential for large codebases
   
2. **[FEATURE] AST Caching** - Session-level cache
   - Avoid re-parsing same files
   - Major UX improvement

### P2 - AI Agent Experience  
1. **[FEATURE] Semantic Functions** - High-level helpers
   - `code_functions()`, `code_classes()`, etc.
   - 90% of AI agent use cases
   
2. **[FEATURE] Progressive Disclosure** - Layered API
   - Simple queries for common cases
   - Full power when needed

## Long-term Vision (Phase 3)

### P3 - Architecture Evolution
1. **[FEATURE] Native AST Type** - Custom DuckDB type
   - Eliminate JSON overhead
   - Requires significant engineering
   
2. **[FEATURE] Incremental Parsing** - Parse only changes
   - Essential for IDE integration
   - Complex implementation
   
3. **[FEATURE] Distributed Processing** - Multi-machine support
   - Handle entire repositories
   - Cloud deployment ready

## Implementation Roadmap

### Week 1-2: Foundation
- [ ] Implement chain methods in C++
- [ ] Add JavaScript language support
- [ ] Create performance benchmarks

### Week 3-4: Performance
- [ ] C++ implementation of core macros
- [ ] Add filter pushdown
- [ ] Implement basic caching

### Week 5-6: AI Agent UX
- [ ] Create semantic helper functions
- [ ] Add error context propagation
- [ ] Write AI agent cookbook

### Week 7-8: Polish
- [ ] Performance testing suite
- [ ] Documentation update
- [ ] Example notebooks

## Success Metrics

1. **Performance**: <1s query on 10K+ node files
2. **Usability**: 90% queries use high-level API
3. **Reliability**: Zero regression in test suite
4. **Adoption**: Positive AI agent feedback