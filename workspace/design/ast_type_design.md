# AST Type Design Document

## Overview
Implement a custom AST type in DuckDB that represents a complete Abstract Syntax Tree as a single value, enabling more sophisticated AST manipulation and analysis.

## Type Definition

### AST Type Structure
```cpp
struct ASTType {
    string file_path;
    string language;
    vector<ASTNode> nodes;
    unordered_map<int64_t, int64_t> parent_map;  // node_id -> parent_id
    TSTree* tree;  // Keep tree-sitter tree for efficiency
};
```

### Usage Examples
```sql
-- Get AST objects for files
SELECT file_path, ast 
FROM read_ast_objects('*.py', 'python');

-- Use dot notation to access methods
SELECT 
    file_path,
    ast.node_count() as total_nodes,
    ast.find_nodes('function_definition') as functions,
    ast.max_depth() as depth
FROM read_ast_objects('*.py', 'python');

-- Extract specific nodes
SELECT 
    file_path,
    func.name,
    func.start_line
FROM read_ast_objects('*.py', 'python'),
    UNNEST(ast.find_nodes('function_definition')) as func;
```

## Core Functions

### 1. read_ast_objects
Returns one row per file with an AST object.
```sql
CREATE TABLE FUNCTION read_ast_objects(
    file_pattern VARCHAR,
    language VARCHAR
) RETURNS TABLE (
    file_path VARCHAR,
    ast AST
);
```

### 2. AST Type Methods
Methods accessible via dot notation:
- `node_count()`: Total number of nodes
- `max_depth()`: Maximum tree depth
- `find_nodes(type)`: Returns array of nodes matching type
- `get_node_by_id(id)`: Returns specific node
- `get_children(node_id)`: Returns child nodes
- `get_parent(node_id)`: Returns parent node
- `to_json()`: Serialize AST to JSON

### 3. Helper Functions
Table functions that work with AST objects:
```sql
-- Extract all functions
CREATE TABLE FUNCTION ast_functions(ast AST) 
RETURNS TABLE (
    name VARCHAR,
    start_line INTEGER,
    end_line INTEGER,
    parameters VARCHAR[],
    is_async BOOLEAN
);

-- Extract all classes
CREATE TABLE FUNCTION ast_classes(ast AST)
RETURNS TABLE (
    name VARCHAR,
    start_line INTEGER,
    end_line INTEGER,
    methods VARCHAR[],
    base_classes VARCHAR[]
);

-- Extract imports
CREATE TABLE FUNCTION ast_imports(ast AST)
RETURNS TABLE (
    module VARCHAR,
    names VARCHAR[],
    alias VARCHAR,
    line INTEGER
);
```

## Implementation Plan

### Phase 1: Basic AST Type
1. Define AST type in DuckDB type system
2. Implement serialization/deserialization
3. Basic construction from tree-sitter parse

### Phase 2: Core Methods
1. Implement find_nodes method
2. Add navigation methods (parent, children)
3. Add utility methods (count, depth)

### Phase 3: Helper Functions
1. Implement ast_functions
2. Implement ast_classes
3. Implement ast_imports

### Phase 4: Advanced Features
1. AST comparison/diff
2. Pattern matching
3. AST transformation

## Technical Considerations

### Memory Management
- Keep tree-sitter tree alive while AST object exists
- Implement proper copy/move semantics
- Consider lazy loading for large ASTs

### Performance
- Index nodes by type for fast find_nodes
- Cache computed values (depth, count)
- Optimize serialization for storage

### Integration
- Ensure compatibility with existing read_ast function
- Support all languages already implemented
- Maintain backward compatibility