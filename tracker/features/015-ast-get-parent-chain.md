# ast_get_parent_chain() - Navigate Scope Hierarchy

**Source**: Peer Review Feedback
**Priority**: P1 (High - Enables scope understanding)  
**Status**: Ready for Implementation

## Overview

Get all ancestors of a node up to the root, critical for understanding scope and context of any code element.

## API Design

```sql
-- Function signature
ast_get_parent_chain(node_id, max_depth := NULL) → JSON[]

-- Macro version for chaining
.get_parent_chain(max_depth := NULL)
```

## Usage Examples

```sql
-- Find the class containing a method
SELECT 
    node->>'name' as method_name,
    parent->>'name' as class_name
FROM read_ast_objects('models.py', 'python') ast,
     LATERAL json_array_elements(
         ast_get_parent_chain(node_id)
     ) as parent
WHERE node->>'type' = 'function_definition'
  AND parent->>'type' = 'class_definition';

-- Get full scope path
WITH method_ast AS (
    SELECT * FROM read_ast_objects('api.py', 'python')
    WHERE type = 'function_definition' 
      AND name = 'handle_request'
)
SELECT 
    name,
    array_agg(
        parent->>'name' 
        ORDER BY ordinality DESC
    ) as scope_path
FROM method_ast,
     LATERAL json_array_elements_with_ordinality(
         ast_get_parent_chain(node_id)
     ) as t(parent, ordinality)
WHERE parent->>'name' IS NOT NULL;
-- Returns: ['module_name', 'ClassName', 'handle_request']
```

## Implementation Strategy

### Option 1: Recursive CTE Macro (Simpler)
```sql
CREATE OR REPLACE MACRO ast_get_parent_chain(nodes, node_id, max_depth) AS (
    WITH RECURSIVE parents AS (
        -- Start with the node itself
        SELECT node, 0 as depth
        FROM json_array_elements(nodes) as node
        WHERE node->>'id' = node_id::VARCHAR
        
        UNION ALL
        
        -- Recursively find parents
        SELECT parent.node, p.depth + 1
        FROM parents p,
             json_array_elements(nodes) as parent(node)
        WHERE parent.node->>'id' = p.node->>'parent_id'
          AND (max_depth IS NULL OR p.depth < max_depth)
    )
    SELECT json_agg(node ORDER BY depth) FROM parents
);
```

### Option 2: With Parent ID Tracking (Better Performance)
- During parsing, store parent_id in each node
- Simple array scan to build parent chain
- O(depth) instead of O(n*depth)

## Benefits

- **Scope Understanding**: Know where any code element lives
- **Navigation**: Jump between related elements
- **Context**: Understand full hierarchical context
- **Filtering**: Find all methods in specific classes

## Integration with Annotations

Could be computed as annotation during parse:
```json
{
    "type": "function_definition",
    "name": "process",
    "annotations": {
        "parent_chain": ["module", "PaymentService", "process"],
        "scope_path": "payments.PaymentService.process"
    }
}
```

## Next Steps

1. Add parent_id tracking during parse
2. Implement recursive CTE version first
3. Optimize with direct parent lookup
4. Consider adding as default annotation