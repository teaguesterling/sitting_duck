# Implementation Plan: AST API v2 with Structure Preservation

## Overview
Implement dual macro types to preserve AST metadata through transformations while providing clean extraction APIs for final results.

## Phase 1: Core Structure-Preserving Macros (2-3 days)

### 1.1 Define AST Object Structure
```sql
-- Ensure AST object has consistent structure
-- Current: file_path, language, node_count, max_depth, nodes
-- Consider adding: parse_time, error_count, file_size
```

### 1.2 Implement Basic Filters
```sql
-- Structure-preserving filters that return full AST objects
CREATE OR REPLACE MACRO ast_filter_type(ast_obj, node_type) AS (...);
CREATE OR REPLACE MACRO ast_filter_types(ast_obj, type_list) AS (...);
CREATE OR REPLACE MACRO ast_at_depth(ast_obj, target_depth) AS (...);
CREATE OR REPLACE MACRO ast_at_depth_range(ast_obj, min_depth, max_depth) AS (...);
CREATE OR REPLACE MACRO ast_at_line(ast_obj, line_number) AS (...);
CREATE OR REPLACE MACRO ast_in_line_range(ast_obj, start_line, end_line) AS (...);
```

### 1.3 Implement Navigation
```sql
-- Tree navigation that preserves structure
CREATE OR REPLACE MACRO ast_children_of(ast_obj, parent_id) AS (...);
CREATE OR REPLACE MACRO ast_parent_of(ast_obj, child_id) AS (...);
CREATE OR REPLACE MACRO ast_ancestors_of(ast_obj, node_id) AS (...);
CREATE OR REPLACE MACRO ast_descendants_of(ast_obj, node_id) AS (...);
```

## Phase 2: Extraction Macros with Proper Types (2 days)

### 2.1 Simple Extractors
```sql
-- Return native SQL types, not JSON
CREATE OR REPLACE MACRO ast_extract_function_names(ast_obj) AS VARCHAR[];
CREATE OR REPLACE MACRO ast_extract_class_names(ast_obj) AS VARCHAR[];
CREATE OR REPLACE MACRO ast_extract_identifiers(ast_obj) AS VARCHAR[];
CREATE OR REPLACE MACRO ast_extract_strings(ast_obj) AS VARCHAR[];
```

### 2.2 Table-Returning Extractors
```sql
-- Return structured tables for complex data
CREATE OR REPLACE MACRO ast_extract_entities(ast_obj) AS TABLE (
    file VARCHAR,
    start_line INTEGER,
    end_line INTEGER,
    entity_type VARCHAR,
    entity_name VARCHAR,
    parent_name VARCHAR,
    node_id BIGINT
);

CREATE OR REPLACE MACRO ast_extract_functions(ast_obj) AS TABLE (
    file VARCHAR,
    name VARCHAR,
    start_line INTEGER,
    end_line INTEGER,
    parameter_count INTEGER,
    is_async BOOLEAN,
    decorators VARCHAR[],
    node_id BIGINT
);
```

### 2.3 Summary Extractors
```sql
-- Return structured summaries
CREATE OR REPLACE MACRO ast_extract_summary(ast_obj) AS STRUCT(
    total_nodes INTEGER,
    function_count INTEGER,
    class_count INTEGER,
    max_depth INTEGER,
    avg_depth DOUBLE,
    lines_of_code INTEGER
);
```

## Phase 3: Source Code Integration (3 days)

### 3.1 Implement File Reading UDF
```cpp
// C++ UDF to read file lines with caching
class GetFileLines : public ScalarFunction {
    static string Execute(string file_path, int start_line, int end_line) {
        // Cache file contents within query execution
        // Return requested lines with line numbers
    }
};
```

### 3.2 Source Extraction Macro
```sql
CREATE OR REPLACE MACRO ast_extract_source(ast_obj, pad_lines DEFAULT 0) AS TABLE (
    file VARCHAR,
    node_id BIGINT,
    node_type VARCHAR,
    node_name VARCHAR,
    start_line INTEGER,
    end_line INTEGER,
    context_start INTEGER,
    context_end INTEGER,
    source_text VARCHAR,
    source_with_context VARCHAR
);
```

### 3.3 Advanced Source Operations
```sql
-- Extract source for specific node types with context
CREATE OR REPLACE MACRO ast_extract_typed_source(ast_obj, node_type, pad_lines) AS (...);

-- Extract source matching pattern
CREATE OR REPLACE MACRO ast_extract_matching_source(ast_obj, name_pattern, pad_lines) AS (...);
```

## Phase 4: Advanced Features (2 days)

### 4.1 Pattern Matching
```sql
-- Filter by name pattern
CREATE OR REPLACE MACRO ast_filter_name_pattern(ast_obj, pattern) AS (...);

-- Filter by source content
CREATE OR REPLACE MACRO ast_filter_source_pattern(ast_obj, pattern) AS (...);

-- Complex pattern matching (pseudo-CSS selector style)
CREATE OR REPLACE MACRO ast_filter_pattern(ast_obj, selector) AS (...);
-- Examples: 'function_definition[name~="test_"]'
--          'class_definition > function_definition[is_async=true]'
```

### 4.2 Metadata Enhancement
```sql
-- Add computed metadata to nodes
CREATE OR REPLACE MACRO ast_enrich_with_metrics(ast_obj) AS (...);
-- Adds: complexity, line_count, token_count to each node

-- Add file-level metadata
CREATE OR REPLACE MACRO ast_add_file_metadata(ast_obj) AS (...);
-- Adds: file_size, last_modified, git_hash
```

## Phase 5: Language-Specific Helpers (1 day per language)

### 5.1 Python-Specific
```sql
CREATE OR REPLACE MACRO ast_python_imports(ast_obj) AS TABLE(...);
CREATE OR REPLACE MACRO ast_python_decorators(ast_obj) AS TABLE(...);
CREATE OR REPLACE MACRO ast_python_docstrings(ast_obj) AS TABLE(...);
```

### 5.2 JavaScript/TypeScript-Specific
```sql
CREATE OR REPLACE MACRO ast_js_exports(ast_obj) AS TABLE(...);
CREATE OR REPLACE MACRO ast_js_imports(ast_obj) AS TABLE(...);
CREATE OR REPLACE MACRO ast_ts_interfaces(ast_obj) AS TABLE(...);
```

## Testing Strategy

### Unit Tests
- Test each macro with empty AST
- Test with single-node AST
- Test with complex nested AST
- Test metadata preservation through chains

### Integration Tests  
- Test complex chained operations
- Test cross-file analysis
- Test performance with large codebases
- Test memory usage with deep chains

### Example Test Cases
```sql
-- Test 1: Metadata preservation
WITH original AS (
    SELECT * FROM read_ast_objects('test.py', 'python')
),
filtered AS (
    SELECT ast_filter_type(ast_obj, 'function_definition') as result
    FROM original
)
SELECT 
    original.file_path = filtered.result.file_path as path_preserved,
    original.language = filtered.result.language as lang_preserved
FROM original, filtered;

-- Test 2: Chaining operations
SELECT 
    ast_obj
        .ast_filter_type('class_definition')
        .ast_descendants_of(node_id)
        .ast_filter_type('function_definition')
        .node_count as methods_in_class
FROM read_ast_objects('test.py', 'python') as ast_obj;
```

## Documentation Updates

1. **Migration Guide**: Show how to update queries from v1 to v2
2. **Cookbook**: Common patterns and recipes
3. **Performance Guide**: Best practices for large codebases
4. **API Reference**: Complete macro documentation

## Success Metrics

- [ ] All structure-preserving macros maintain metadata
- [ ] Extraction macros return proper SQL types
- [ ] Source extraction works with proper context
- [ ] Chaining operations feel natural
- [ ] Performance remains < 100ms for typical queries
- [ ] Memory usage scales linearly with AST size
- [ ] Documentation covers 80% of use cases

This implementation plan addresses both of your considerations by:
1. Preserving AST metadata through transformations
2. Enabling source tracking and extraction with context
3. Supporting the comprehensive analysis table use case
4. Maintaining backward compatibility while improving the API