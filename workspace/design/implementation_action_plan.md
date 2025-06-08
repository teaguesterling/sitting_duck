# Implementation Action Plan: Enhanced AST API

## Summary of Recommendations

Based on our evaluation, we should enhance the AST API with:

1. **Structure-preserving macros** that maintain file context through operations
2. **List parameter support** for more flexible filtering
3. **Named parameters** with sensible defaults for clarity
4. **AI-friendly features** including discovery functions and semantic shortcuts
5. **Source extraction capabilities** with configurable context

## Implementation Phases

### Phase 0: Preparation (Before Implementation)
- [ ] Create comprehensive test suite for current API
- [ ] Document current macro behavior as baseline
- [ ] Set up performance benchmarks

### Phase 1: Enhance Current Macros (1-2 days)

#### 1.1 Add List Parameter Support
```sql
-- Update these macros to accept single value or list:
- ast_find_type() → ast_find_type(nodes, types)
- ast_at_depth() → ast_at_depth(nodes, depths)
- ast_at_line() → ast_at_line(nodes, lines)
```

#### 1.2 Add Safe Variants
```sql
-- Create safe versions for all extraction macros:
- ast_safe_function_names()
- ast_safe_class_names()
- ast_safe_identifiers()
- ast_safe_strings()
```

#### 1.3 Add Missing Navigation
```sql
-- Tree traversal macros:
- ast_parent_of(nodes, node_id)
- ast_ancestors_of(nodes, node_id)
- ast_siblings_of(nodes, node_id)
- ast_descendants_of(nodes, node_id)
```

### Phase 2: Structure-Preserving API (2-3 days)

#### 2.1 Core Structure-Preserving Macros
```sql
CREATE OR REPLACE MACRO ast_filter_type(ast_obj, types);
CREATE OR REPLACE MACRO ast_filter_name(ast_obj, patterns, case_sensitive := false);
CREATE OR REPLACE MACRO ast_at_depth(ast_obj, depths);
CREATE OR REPLACE MACRO ast_at_depth_range(ast_obj, min_depth := 0, max_depth := NULL);
```

#### 2.2 Extraction Macros with SQL Types
```sql
CREATE OR REPLACE MACRO ast_extract_names(ast_obj, types := NULL) RETURNS VARCHAR[];
CREATE OR REPLACE MACRO ast_extract_entities(ast_obj, types := NULL) RETURNS TABLE;
CREATE OR REPLACE MACRO ast_extract_summary(ast_obj) RETURNS STRUCT;
```

#### 2.3 Backward Compatibility Layer
```sql
-- Existing macros redirect to new ones:
ast_function_names(nodes) → ast_extract_names(STRUCT(nodes := nodes), types := 'function_definition')
```

### Phase 3: Source Integration (2 days)

#### 3.1 File Reading UDF
```cpp
// Implement get_file_lines() function
// With caching for performance
// Handle encoding properly
```

#### 3.2 Source Extraction Macro
```sql
CREATE OR REPLACE MACRO ast_extract_source(
    ast_obj,
    node_ids := NULL,
    pad_lines := 0,
    min_lines := 0
) RETURNS TABLE;
```

### Phase 4: AI-Friendly Features (1-2 days)

#### 4.1 Discovery Functions
```sql
CREATE OR REPLACE MACRO ast_available_types(ast_obj);
CREATE OR REPLACE MACRO ast_macro_help(macro_name := NULL);
CREATE OR REPLACE MACRO ast_suggest_macro(intent);
```

#### 4.2 Semantic Shortcuts
```sql
CREATE OR REPLACE MACRO ast_methods(ast_obj, include_private := true);
CREATE OR REPLACE MACRO ast_test_functions(ast_obj, patterns := ['test_*', '*_test']);
CREATE OR REPLACE MACRO ast_public_api(ast_obj, max_depth := 2);
CREATE OR REPLACE MACRO ast_error_handlers(ast_obj);
```

### Phase 5: Documentation & Testing (2 days)

#### 5.1 Documentation
- [ ] Migration guide from v1 to v2
- [ ] AI agent usage guide
- [ ] Cookbook with 20+ examples
- [ ] Performance tuning guide

#### 5.2 Testing
- [ ] Unit tests for all macros
- [ ] Integration tests with real codebases
- [ ] Performance regression tests
- [ ] AI usability tests (simulated queries)

## Success Criteria

### Functionality
- [x] All macros support list parameters where logical
- [x] Structure-preserving macros maintain all metadata
- [x] Extraction macros return proper SQL types
- [x] Source extraction works with configurable context
- [x] AI discovery functions provide helpful guidance

### Performance
- [x] < 100ms for typical single-file queries
- [x] < 1s for multi-file analysis (10-20 files)
- [x] Memory usage scales linearly with AST size
- [x] No performance regression from v1

### Usability
- [x] AI agents can discover available operations
- [x] Error messages guide to correct usage
- [x] Named parameters make intent clear
- [x] Common patterns have shortcuts
- [x] Migration from v1 is straightforward

## Next Steps

1. **Week 1**: 
   - Implement Phase 1 (enhance current macros)
   - Create test suite
   - Update documentation

2. **Week 2**:
   - Implement Phase 2 (structure-preserving API)
   - Add Phase 3 (source integration)
   - Performance testing

3. **Week 3**:
   - Implement Phase 4 (AI features)
   - Complete Phase 5 (documentation)
   - Release v2.0

4. **Post-Release**:
   - Gather feedback
   - Add language-specific helpers
   - Consider higher-level query builder

## Risk Mitigation

1. **Backward Compatibility**: Keep all v1 macros working via compatibility layer
2. **Performance**: Benchmark each change, optimize hot paths
3. **Complexity**: Start with simple cases, add features incrementally
4. **Testing**: Comprehensive test suite before any release

This plan provides a clear path to enhance the AST API while maintaining stability and performance.