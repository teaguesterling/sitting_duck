# Native Implementation Strategy for DuckDB AST Extension

## The Recursive Type Challenge

DuckDB's type system doesn't support recursive struct definitions, which presents a challenge for representing tree structures natively. This document explores solutions.

## Option 1: ID-Based Flattened Representation

### Schema
```sql
-- Core node type without recursion
CREATE TYPE ast_node AS STRUCT(
    node_id UINTEGER,
    parent_id UINTEGER,
    depth UINTEGER,
    child_index UINTEGER,  -- Position among siblings
    type VARCHAR,
    position STRUCT(
        start_byte UBIGINT,
        end_byte UBIGINT,
        start_row UINTEGER,
        start_column UINTEGER,
        end_row UINTEGER,
        end_column UINTEGER
    ),
    text VARCHAR,
    annotations MAP(VARCHAR, VARCHAR)  -- Or JSON for complex annotations
);

-- Table function returns flat list
CREATE FUNCTION parse_ast_native(code VARCHAR, language VARCHAR)
RETURNS TABLE(node ast_node);
```

### Usage
```sql
-- Get all functions with their parent context
WITH ast AS (
    SELECT * FROM parse_ast_native('class Foo:\n  def bar(): pass', 'python')
)
SELECT 
    child.annotations['extracted_name'] as method_name,
    parent.annotations['extracted_name'] as class_name
FROM ast child
JOIN ast parent ON child.parent_id = parent.node_id
WHERE child.annotations['normalized_type'] = 'function_declaration';
```

### Pros & Cons
✅ Native performance  
✅ Efficient joins for parent/child queries  
✅ Can leverage DuckDB's columnar storage  
❌ Loses natural tree structure  
❌ Requires joins for hierarchy navigation  

## Option 2: Adjacency List with Array Children

### Schema
```sql
CREATE TYPE ast_node_with_children AS STRUCT(
    node_id UINTEGER,
    type VARCHAR,
    position STRUCT(...),
    text VARCHAR,
    annotations MAP(VARCHAR, VARCHAR),
    child_ids UINTEGER[]  -- Array of child node IDs
);

-- Return both nodes and edges
CREATE FUNCTION parse_ast_native(code VARCHAR, language VARCHAR)
RETURNS TABLE(
    nodes ast_node_with_children[],
    root_id UINTEGER
);
```

### Usage
```sql
-- Traverse using recursive CTE
WITH RECURSIVE ast AS (
    SELECT nodes, root_id FROM parse_ast_native('...', 'python')
),
traversal AS (
    -- Start at root
    SELECT 
        nodes[root_id + 1] as node,  -- Array is 1-indexed
        0 as depth
    FROM ast
    
    UNION ALL
    
    -- Recurse through children
    SELECT 
        ast.nodes[child_id + 1] as node,
        t.depth + 1 as depth
    FROM traversal t, 
         UNNEST(t.node.child_ids) as child_id,
         ast
)
SELECT * FROM traversal;
```

### Pros & Cons
✅ More natural tree representation  
✅ Single array access for node data  
❌ Complex recursive queries  
❌ Array indexing overhead  

## Option 3: Hybrid JSON/Native Approach (Recommended)

### Schema
```sql
-- Native types for frequently accessed fields
CREATE TYPE ast_node_summary AS STRUCT(
    node_id UINTEGER,
    parent_id UINTEGER,
    type VARCHAR,
    normalized_type VARCHAR,
    name VARCHAR,
    start_line UINTEGER,
    end_line UINTEGER,
    -- Keep full node as JSON for flexibility
    full_node JSON
);

-- Dual return: structured summary + full JSON
CREATE FUNCTION parse_ast_hybrid(
    code VARCHAR, 
    language VARCHAR,
    extract_summaries BOOLEAN = true
) RETURNS TABLE(
    summary ast_node_summary,
    json_tree JSON  -- Full tree for complex queries
);
```

### Usage
```sql
-- Fast filtering on native columns
SELECT 
    summary.name,
    summary.start_line,
    summary.full_node->'annotations'->'signature' as signature
FROM parse_ast_hybrid(read_file('api.py'), 'python')
WHERE summary.normalized_type = 'function_declaration'
  AND summary.name LIKE 'handle_%';

-- Complex tree operations on JSON
SELECT ast(json_tree)
    .with_annotations('complexity')
    .get_definitions()
FROM parse_ast_hybrid(read_file('api.py'), 'python', false);
```

### Pros & Cons
✅ Best of both worlds  
✅ Native performance for common queries  
✅ JSON flexibility for complex operations  
✅ Gradual migration path  
❌ Some data duplication  

## Option 4: Virtual Table Implementation

### Concept
```cpp
// C++ Implementation
class ASTVirtualTable : public VirtualTable {
    // Lazy parsing - only materialize requested nodes
    unique_ptr<TSTree> tree;
    
    TableFunction GetTableFunction() override {
        return TableFunction("ast_nodes", {code, language}, {
            {"node_id", LogicalType::UINTEGER},
            {"parent_id", LogicalType::UINTEGER},
            {"type", LogicalType::VARCHAR},
            // ... other columns
        }, ASTScanFunction);
    }
};
```

### Usage
```sql
-- Virtual table appears as regular table
CREATE VIRTUAL TABLE code_ast 
USING ast_parser('def foo(): pass', 'python');

SELECT * FROM code_ast WHERE type = 'function_definition';
```

### Pros & Cons
✅ Lazy evaluation  
✅ Memory efficient  
✅ Natural SQL interface  
❌ More complex implementation  
❌ Limited by virtual table API  

## Recommended Implementation Path

### Phase 1: JSON with Native Helpers
1. Keep current JSON implementation
2. Add native helper functions for common operations:
   ```sql
   -- Native function for fast filtering
   filter_ast_json(json_ast JSON, node_types VARCHAR[]) 
   RETURNS JSON;
   
   -- Native function for signature extraction
   extract_signatures_native(json_ast JSON, language VARCHAR) 
   RETURNS TABLE(name VARCHAR, return_type VARCHAR, ...);
   ```

### Phase 2: Hybrid Implementation
1. Implement Option 3 (Hybrid JSON/Native)
2. Benchmark against pure JSON
3. Provide migration utilities

### Phase 3: Full Native (If Needed)
1. Based on usage patterns, implement Option 1 or 2
2. Consider virtual table for large files
3. Maintain JSON compatibility layer

## Performance Optimization Strategies

### 1. Selective Materialization
```sql
-- Only materialize nodes matching criteria
CREATE FUNCTION parse_ast_filtered(
    code VARCHAR,
    language VARCHAR,
    node_types VARCHAR[],
    max_depth INTEGER = NULL
) RETURNS TABLE(...);
```

### 2. Streaming Parser
```sql
-- For large files, stream results
CREATE FUNCTION parse_ast_stream(
    file_path VARCHAR,
    language VARCHAR,
    chunk_size INTEGER = 1000
) RETURNS TABLE(...);
```

### 3. Caching Layer
```sql
-- Cache parsed ASTs with TTL
CREATE TABLE ast_cache (
    file_hash BYTES,
    language VARCHAR,
    parsed_at TIMESTAMP,
    node_summaries ast_node_summary[],
    json_tree JSON,
    PRIMARY KEY (file_hash, language)
);
```

## Conclusion

The hybrid approach (Option 3) provides the best balance:
- Immediate performance benefits
- Maintains flexibility
- Clear migration path
- Leverages DuckDB's strengths

We should start with JSON + native helpers, then move to hybrid based on real-world usage patterns.