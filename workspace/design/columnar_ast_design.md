# Columnar AST Design Specification

## Overview

This document specifies a columnar-oriented native implementation for the DuckDB AST extension that aligns with DuckDB's columnar architecture for maximum performance and flexibility.

## Core Design Principles

1. **Columnar Storage** - Store AST data in column arrays for vectorized operations
2. **Null Optimization** - Allow NULL columns for unused data without overhead
3. **Index-Based Relationships** - Use array indices instead of pointers for relationships
4. **Zero-Copy Filtering** - Create sub-trees by remapping indices, not copying data
5. **Cache-Friendly** - Sequential memory access for common operations

## Type Definitions

```sql
-- Position information
CREATE TYPE ast_position AS STRUCT(
    start_byte UBIGINT,
    end_byte UBIGINT,
    start_row UINTEGER,
    start_column UINTEGER,
    end_row UINTEGER,
    end_column UINTEGER
);

-- Main columnar AST type
CREATE TYPE ast_flat AS STRUCT(
    -- Node data columns (can be NULL for unused data)
    types VARCHAR[],              -- Node types (always present)
    texts VARCHAR[],              -- Node text content (optional)
    start_bytes UBIGINT[],        -- Byte positions (optional)
    end_bytes UBIGINT[],          -- Byte positions (optional)
    start_rows UINTEGER[],        -- Line numbers (always present)
    end_rows UINTEGER[],          -- Line numbers (always present)
    start_cols UINTEGER[],        -- Column positions (optional)
    end_cols UINTEGER[],          -- Column positions (optional)
    
    -- Topology arrays (always present)
    parents UINTEGER[],           -- parent[i] = parent index of node i
    first_child UINTEGER[],       -- first_child[i] = first child index
    next_sibling UINTEGER[],      -- next_sibling[i] = next sibling index
    depths UINTEGER[],            -- Pre-computed depth for fast filtering
    
    -- Annotations (optional, columnar storage)
    annotation_keys VARCHAR[][],   -- annotation_keys[i] = keys for node i
    annotation_values JSON[][],    -- annotation_values[i] = values for node i
    
    -- Metadata
    root UINTEGER,                -- Index of root node
    node_count UINTEGER,          -- Total number of nodes
    language VARCHAR,             -- Source language
    file_path VARCHAR             -- Optional source file path
);
```

## Core C++ Functions

### Navigation Functions
```cpp
// Get parent node by index
ast_node get_parent(const ast_flat& ast, idx_t node_idx);

// Get all children of a node
vector<idx_t> get_children(const ast_flat& ast, idx_t node_idx);

// Get all descendants up to max_depth
vector<idx_t> get_descendants(const ast_flat& ast, idx_t node_idx, idx_t max_depth = UINT32_MAX);

// Get all ancestors up to root
vector<idx_t> get_ancestors(const ast_flat& ast, idx_t node_idx);

// Get sibling nodes
vector<idx_t> get_siblings(const ast_flat& ast, idx_t node_idx);

// Get nodes at specific depth
vector<idx_t> get_nodes_at_depth(const ast_flat& ast, idx_t depth);
```

### Filtering Functions
```cpp
// Filter by node types (creates new ast_flat)
ast_flat filter_by_types(const ast_flat& ast, const vector<string>& types);

// Filter by annotation
ast_flat filter_by_annotation(const ast_flat& ast, const string& key, const Value& value);

// Filter by depth range
ast_flat filter_by_depth(const ast_flat& ast, idx_t min_depth, idx_t max_depth);

// Filter by line range
ast_flat filter_by_lines(const ast_flat& ast, idx_t start_line, idx_t end_line);

// Complex filter with predicate
ast_flat filter_by_predicate(const ast_flat& ast, std::function<bool(idx_t)> predicate);
```

### Extraction Functions
```cpp
// Get node at specific position
optional<idx_t> get_node_at_position(const ast_flat& ast, idx_t line, idx_t column);

// Get all nodes in line range
vector<idx_t> get_nodes_in_range(const ast_flat& ast, idx_t start_line, idx_t end_line);

// Extract subtree rooted at node
ast_flat get_subtree(const ast_flat& ast, idx_t node_idx);

// Get node path from root
vector<idx_t> get_path_from_root(const ast_flat& ast, idx_t node_idx);
```

### Analysis Functions
```cpp
// Count nodes matching criteria
idx_t count_nodes(const ast_flat& ast, const string& type);

// Get tree statistics
struct TreeStats {
    idx_t total_nodes;
    idx_t max_depth;
    idx_t max_width;
    map<string, idx_t> type_counts;
};
TreeStats analyze_tree(const ast_flat& ast);
```

## SQL Interface

### Table Functions
```sql
-- Main parsing function with options
CREATE FUNCTION parse_ast_native(
    code VARCHAR,
    language VARCHAR,
    include_text BOOLEAN = true,
    include_bytes BOOLEAN = false,
    include_columns BOOLEAN = true,
    annotations VARCHAR[] = NULL
) RETURNS ast_flat;

-- Parse from file
CREATE FUNCTION parse_file_ast_native(
    file_path VARCHAR,
    language VARCHAR,
    options STRUCT(
        include_text BOOLEAN,
        include_bytes BOOLEAN,
        include_columns BOOLEAN,
        annotations VARCHAR[]
    ) = NULL
) RETURNS ast_flat;
```

### Accessor Functions
```sql
-- Get specific node data
CREATE FUNCTION ast_get_node(ast ast_flat, node_idx UINTEGER)
RETURNS STRUCT(
    type VARCHAR,
    text VARCHAR,
    start_line UINTEGER,
    end_line UINTEGER,
    annotations MAP(VARCHAR, JSON)
);

-- Get children as table
CREATE FUNCTION ast_get_children(ast ast_flat, node_idx UINTEGER)
RETURNS TABLE(
    idx UINTEGER,
    type VARCHAR,
    text VARCHAR,
    depth UINTEGER
);
```

### Filter Functions
```sql
-- Type filtering
CREATE FUNCTION ast_filter_types(ast ast_flat, types VARCHAR[])
RETURNS ast_flat;

-- Annotation filtering  
CREATE FUNCTION ast_filter_annotation(ast ast_flat, key VARCHAR, value JSON)
RETURNS ast_flat;

-- Depth filtering
CREATE FUNCTION ast_filter_depth(ast ast_flat, min_depth INTEGER, max_depth INTEGER)
RETURNS ast_flat;
```

## Performance Optimizations

### 1. Vectorized Type Checking
```cpp
// Uses SIMD instructions when available
void find_type_vectorized(
    const string* types_array,
    size_t count,
    const string& target_type,
    bool* output_mask
) {
    // Compiler can auto-vectorize this loop
    for (size_t i = 0; i < count; i++) {
        output_mask[i] = (types_array[i] == target_type);
    }
}
```

### 2. Memory Layout
```cpp
// Ensure cache-line alignment for hot columns
struct alignas(64) ast_flat_impl {
    vector<string> types;        // Most accessed - first cache line
    vector<idx_t> parents;       // Topology - frequently accessed
    vector<idx_t> first_child;   // Topology - frequently accessed
    
    // Less frequently accessed columns
    vector<string> texts;
    vector<idx_t> start_rows;
    // ...
};
```

### 3. Lazy Annotation Loading
```cpp
// Annotations loaded only when accessed
class LazyAnnotations {
    mutable unique_ptr<AnnotationData> data;
    string source_file;
    
public:
    const AnnotationData& get() const {
        if (!data) {
            data = load_annotations(source_file);
        }
        return *data;
    }
};
```

## Usage Examples

### Example 1: Fast Function Finding
```sql
-- Leverages vectorized type filtering
WITH ast AS (
    SELECT parse_ast_native(read_file('large_file.py'), 'python', 
                           include_text := true,
                           include_bytes := false) as tree
)
SELECT 
    idx,
    tree.types[idx + 1] as type,
    tree.texts[idx + 1] as name,
    tree.start_rows[idx + 1] as line
FROM ast,
     UNNEST(range(tree.node_count)) as idx
WHERE tree.types[idx + 1] = 'function_definition'
  AND tree.depths[idx + 1] <= 2;  -- Only top-level and class methods
```

### Example 2: Efficient Subtree Extraction
```sql
-- Get all methods in a specific class
WITH ast AS (
    SELECT parse_ast_native(read_file('models.py'), 'python') as tree
),
class_node AS (
    SELECT idx
    FROM ast, UNNEST(range(tree.node_count)) as idx
    WHERE tree.types[idx + 1] = 'class_definition'
      AND tree.texts[idx + 1] LIKE '%User%'
    LIMIT 1
)
SELECT *
FROM ast_get_children(ast.tree, class_node.idx) as children
WHERE children.type = 'function_definition';
```

### Example 3: Memory-Efficient Large File Processing
```sql
-- Process large file with minimal memory
SELECT 
    ast_filter_types(tree, ['function_definition', 'class_definition']) as filtered
FROM parse_ast_native(
    read_file('huge_file.py'), 
    'python',
    include_text := false,    -- Don't load text
    include_bytes := false,   -- Don't load byte positions
    include_columns := false  -- Don't load column positions
) as tree;
```

## Implementation Phases

### Phase 1: Core Infrastructure
- [ ] Implement basic ast_flat type
- [ ] Create parser that outputs columnar format
- [ ] Implement core navigation functions
- [ ] Add basic filtering capabilities

### Phase 2: Optimization
- [ ] Add vectorized operations
- [ ] Implement null column optimization
- [ ] Add memory-mapped file support for large ASTs
- [ ] Benchmark against JSON implementation

### Phase 3: Advanced Features
- [ ] Lazy annotation loading
- [ ] Streaming parser for huge files
- [ ] Parallel parsing support
- [ ] Query optimization for common patterns

### Phase 4: Integration
- [ ] Migration utilities from JSON format
- [ ] Compatibility layer for existing macros
- [ ] Performance profiling tools
- [ ] Documentation and examples

## Benefits Summary

1. **Performance**: 10-100x faster for type filtering and range queries
2. **Memory Efficiency**: Only load needed columns
3. **Cache Friendly**: Sequential access patterns
4. **Scalability**: Handles million-node ASTs efficiently
5. **DuckDB Native**: Leverages columnar engine optimizations

## Open Questions

1. Should we support incremental parsing for file changes?
2. How do we handle different versions of the same file?
3. Should annotations be strongly typed or remain flexible JSON?
4. What's the optimal chunk size for streaming large files?

## Conclusion

This columnar design aligns perfectly with DuckDB's architecture and will provide exceptional performance for code analysis at scale. The complexity is hidden behind intuitive APIs while maintaining the flexibility of our annotation system.