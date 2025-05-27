# ast_find_references() - Find All Uses of a Symbol

**Source**: Peer Review Feedback
**Priority**: P1 (High - Core navigation feature)
**Status**: Design Phase

## Overview

Find all references to a name within a given scope. Essential for understanding code dependencies and refactoring.

## API Design

```sql
-- Function signatures
ast_find_references(name, scope_id := NULL) → TABLE(node_id, type, line, context)
ast_find_references(nodes, name, scope_id := NULL) → JSON[]

-- Macro version
.find_references(name, scope_id := NULL)
```

## Usage Examples

```sql
-- Find all calls to a function
SELECT * FROM ast_find_references('calculate_tax', NULL)
FROM read_ast_objects('billing/*.py', 'python');

-- Find references within a specific class
WITH class_scope AS (
    SELECT node_id 
    FROM read_ast_objects('models.py', 'python')
    WHERE type = 'class_definition' AND name = 'Order'
)
SELECT * FROM ast_find_references('total_amount', class_scope.node_id)
FROM read_ast_objects('models.py', 'python'), class_scope;

-- Chain syntax
SELECT ast(nodes)
    .find_references('user_id')
    .get_source(1) as usage_contexts
FROM read_ast_objects('api.py', 'python');
```

## Implementation Challenges

1. **Name Resolution**: Same name can refer to different things in different scopes
2. **Language Specific**: Each language has different scoping rules
3. **Performance**: Need efficient scope-aware search

## Implementation Strategy

### Phase 1: Simple Text Matching
```sql
-- Basic implementation: find by name match
CREATE FUNCTION ast_find_references_simple(
    nodes JSON,
    target_name VARCHAR,
    scope_id INTEGER DEFAULT NULL
) AS (
    SELECT node
    FROM json_array_elements(nodes) as node
    WHERE node->>'name' = target_name
      AND node->>'type' IN ('identifier', 'variable_reference', 'function_call')
      AND (scope_id IS NULL OR is_within_scope(node, scope_id))
);
```

### Phase 2: Scope-Aware Search
- Track variable definitions and their scopes
- Resolve names based on language scoping rules
- Handle imports and qualified names

### Phase 3: Smart References
- Understand method calls vs function calls
- Handle renamed imports
- Track type information where available

## Benefits

- **Navigation**: Jump to all uses of a variable/function
- **Refactoring**: Find what needs updating
- **Understanding**: See how code is used
- **Analysis**: Identify unused code

## Related Features

- Works with `ast_get_parent_chain()` for scope analysis
- Combines with `ast_get_source()` to show usage context
- Enhanced by annotation system for better accuracy

## Next Steps

1. Implement basic name matching
2. Add scope containment checking  
3. Create language-specific refinements
4. Consider semantic analysis integration