# DuckDB AST Extension - Development Priorities

**Last Updated:** 2025-05-27

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
- `parse_ast()` function implementation
- Python, JavaScript, C++ language support
- Rust temporarily disabled (ABI issues)

🚧 **In Design:**
- Annotation system architecture
- Columnar native AST implementation

## Immediate Priorities (Phase 1)

### P0 - Critical Fixes
1. **[BUG] Rust Parser ABI Compatibility** - Fix tree-sitter-rust integration
   - Currently causes system freeze
   - Temporarily disabled
   
### P1 - Core Functionality (Peer Review Priority)
1. **[FEATURE] ast_get_source()** - Extract source code with context (#013)
   - HIGHEST PRIORITY from peer review
   - Extract code snippets with surrounding lines
   - Immediate value for users
   
2. **[FEATURE] Parse-Time Filtering** - Performance at scale (#014)
   - Add `only_types` parameter to read_ast functions  
   - Essential for large codebases
   - 5-50x memory reduction
   
3. **[FEATURE] ast_get_parent_chain()** - Navigate scope hierarchy (#015)
   - Get all ancestors of a node
   - Critical for scope understanding
   - Enables qualified name extraction
   
4. **[FEATURE] ast_find_references()** - Find symbol usage (#016)
   - Find all uses of a variable/function
   - Core navigation feature
   - Scope-aware search
   
5. **[FEATURE] ast_get_calls()** - Extract function calls (#017)
   - Find dependencies within functions
   - Essential for call graph analysis
   - Complexity metrics

### P1 - Architecture Foundation
1. **[FEATURE] Annotation System Implementation** - Flexible enrichment framework
   - Separate pure AST from enrichments
   - Enable lazy evaluation
   - Support user-defined annotations
   
2. **[FEATURE] Core Annotations** - Essential enrichments
   - `ast_get_locations()` - Extract position info
   - `ast_get_signatures()` - Function/method signatures  
   - `ast_get_definitions()` - Combined location + signature

### P2 - Performance & Quality
1. **[FEATURE] Parse-time Filtering** - Reduce memory usage
   - Add node_filter parameter to read_ast functions
   - Support include/exclude patterns
   - Language-specific presets
   
2. **[FEATURE] Source Code Extraction** - ast_get_source with annotations
   - Extract code snippets with context
   - Leverage position annotations

## Medium-term Priorities (Phase 2)

### P1 - Columnar Native Implementation
1. **[FEATURE] Columnar AST Type** - High-performance native type
   - Implement ast_flat columnar structure
   - 10-100x faster type filtering
   - Memory-efficient for large ASTs
   - See design: `/workspace/design/columnar_ast_design.md`
   
2. **[FEATURE] Vectorized Operations** - Leverage DuckDB's columnar engine
   - SIMD-optimized filtering
   - Cache-friendly access patterns
   - Null column optimization

### P2 - Advanced Annotations
1. **[FEATURE] Semantic Annotations** - Higher-level analysis
   - Complexity metrics
   - Dependency tracking
   - Scope resolution
   
2. **[FEATURE] User-defined Annotations** - Extensibility
   - Macro-based annotation functions
   - Composable annotation pipeline
   - Domain-specific enrichments

## Long-term Vision (Phase 3)

### P2 - Integration Features  
1. **[FEATURE] Incremental Parsing** - Parse only changes
   - Track file modifications
   - Update AST incrementally
   - Cache invalidation
   
2. **[FEATURE] Cross-file Analysis** - Repository-wide queries
   - Module dependency graphs
   - Call flow analysis
   - Impact assessment

### P3 - Ecosystem
1. **[FEATURE] Language Server Protocol** - IDE integration
   - Real-time AST updates
   - Code navigation support
   - Refactoring assistance
   
2. **[FEATURE] Cloud Deployment** - Distributed processing
   - Handle massive codebases
   - Parallel parsing
   - Result aggregation

## Implementation Roadmap

### Sprint 1 (Current): Annotation Foundation
- [x] Design annotation system architecture
- [x] Design columnar AST implementation
- [ ] Implement base annotation infrastructure
- [ ] Add ast_get_locations() macro
- [ ] Test with existing languages

### Sprint 2: Core Annotations
- [ ] Implement signature extraction
- [ ] Add scope calculation
- [ ] Create annotation presets
- [ ] Update documentation

### Sprint 3: Columnar Prototype
- [ ] Implement ast_flat type
- [ ] Create columnar parser output
- [ ] Add vectorized filtering
- [ ] Benchmark vs JSON

### Sprint 4: Integration
- [ ] Migration utilities
- [ ] Compatibility layer
- [ ] Performance testing
- [ ] User documentation

## Success Metrics

1. **Performance**: 
   - <100ms query on 100K node files
   - 10x improvement in type filtering
   - 50% memory reduction

2. **Usability**: 
   - 90% queries use chainable API
   - Annotation system adopted by users
   - Clear migration path

3. **Reliability**: 
   - All languages working (including Rust)
   - Zero regression in test suite
   - Comprehensive error handling

4. **Extensibility**:
   - User-created annotations in use
   - Community language handlers
   - Integration with other tools