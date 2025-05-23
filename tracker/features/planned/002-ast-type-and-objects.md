# AST Type and read_ast_objects Function

**Status**: Planned
**Priority**: Medium
**Estimated Effort**: Medium

## Description
Create a custom AST type in DuckDB and a corresponding `read_ast_objects` function that returns a single row per file with the complete AST as an object.

## Proposed Design
```sql
-- Returns one row per file with AST as an object
SELECT file_path, ast_object 
FROM read_ast_objects('*.py', 'python');

-- Access AST nodes via methods/operators
SELECT 
    file_path,
    ast_object.find_nodes('function_definition') as functions,
    ast_object.get_node_by_id(5) as specific_node
FROM read_ast_objects('*.py', 'python');
```

## Benefits
- More efficient for storing/passing complete ASTs
- Enables AST-specific operations and methods
- Better for cases where you want to analyze whole files rather than individual nodes
- Could support serialization/deserialization of ASTs

## Technical Considerations
- Need to implement custom DuckDB type
- Define useful methods/operators for AST manipulation
- Consider memory efficiency for large ASTs
- Integrate with existing read_ast function