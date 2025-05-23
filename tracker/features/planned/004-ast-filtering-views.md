# AST Filtering Helper Functions

**Status**: Planned  
**Priority**: Low
**Estimated Effort**: Low

## Description
Create helper view functions that make common AST queries easier.

## Proposed Functions
```sql
-- Get all functions from a file
SELECT * FROM ast_functions('file.py', 'python');

-- Get all classes 
SELECT * FROM ast_classes('file.py', 'python');

-- Get all imports
SELECT * FROM ast_imports('file.py', 'python');

-- Get call graph
SELECT * FROM ast_call_graph('file.py', 'python');
```

## Implementation
These would be thin wrappers around read_ast() with built-in filters:
- ast_functions: WHERE type = 'function_definition'
- ast_classes: WHERE type = 'class_definition'  
- ast_imports: WHERE type IN ('import_statement', 'import_from_statement')
- ast_call_graph: Complex query joining function calls to definitions

## Benefits
- Easier to use for common queries
- Self-documenting API
- Can optimize internally if needed