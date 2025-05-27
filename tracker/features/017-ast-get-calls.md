# ast_get_calls() - Extract Function Calls

**Source**: Peer Review Feedback
**Priority**: P1 (High - Essential for dependency analysis)
**Status**: Ready for Implementation

## Overview

Find all function/method calls within a node (usually a function). Critical for understanding dependencies and call graphs.

## API Design

```sql
-- Function signature  
ast_get_calls(node_id) → TABLE(called_name, call_type, line, arguments)
ast_get_calls(nodes) → JSON[]

-- Macro version
.get_calls()
```

## Usage Examples

```sql
-- Find what functions are called by each function
SELECT 
    f.name as function_name,
    c.called_name,
    COUNT(*) as call_count
FROM read_ast_objects('service.py', 'python') f,
     LATERAL ast_get_calls(f.node_id) c
WHERE f.type = 'function_definition'
GROUP BY f.name, c.called_name;

-- Find external dependencies
SELECT DISTINCT called_name
FROM read_ast_objects('module.py', 'python'),
     LATERAL ast_get_calls(node_id)
WHERE call_type = 'external'
ORDER BY called_name;

-- Chain syntax for complexity analysis
SELECT ast(nodes)
    .filter_type(['function_definition'])
    .with_annotations('calls')
    .filter_by_annotation('calls.count', '> 10') as complex_functions
FROM read_ast_objects('app.py', 'python');
```

## Implementation Strategy

### Direct Implementation
```sql
CREATE FUNCTION ast_get_calls_impl(nodes JSON, node_id INTEGER) AS (
    -- Find all call expressions within the subtree
    WITH RECURSIVE subtree AS (
        SELECT node FROM json_array_elements(nodes) node
        WHERE node->>'id' = node_id::VARCHAR
        
        UNION ALL
        
        SELECT child.node
        FROM subtree s,
             json_array_elements(nodes) child(node)
        WHERE child.node->>'parent_id' = s.node->>'id'
    )
    SELECT 
        node->>'name' as called_name,
        node->>'type' as call_type,
        node->'position'->>'start_row' as line
    FROM subtree
    WHERE node->>'normalized_type' = 'function_call'
);
```

### As Annotation
```json
{
    "type": "function_definition",
    "name": "process_order",
    "annotations": {
        "calls": {
            "internal": ["validate_order", "calculate_tax", "save_order"],
            "external": ["logging.info", "db.commit"],
            "count": 5
        }
    }
}
```

## Call Types to Track

1. **Function Calls**: `calculate_tax()`
2. **Method Calls**: `order.calculate_total()`  
3. **Static Calls**: `OrderService.process()`
4. **Module Calls**: `math.sqrt()`
5. **Constructor Calls**: `new User()` / `User()`

## Benefits

- **Dependency Graphs**: Build call relationships
- **Complexity Metrics**: Count external dependencies
- **Dead Code Detection**: Find uncalled functions
- **Impact Analysis**: What calls this function?

## Integration Ideas

```sql
-- Build a call graph
WITH call_data AS (
    SELECT 
        f.name as caller,
        c.called_name as callee
    FROM read_ast_objects('**/*.py', 'python') f,
         LATERAL ast_get_calls(f.node_id) c
    WHERE f.type = 'function_definition'
)
-- Find most called functions
SELECT 
    callee,
    COUNT(DISTINCT caller) as caller_count
FROM call_data
GROUP BY callee
ORDER BY caller_count DESC;
```

## Next Steps

1. Implement basic call extraction
2. Classify call types (internal/external/method)
3. Add as annotation option
4. Create call graph visualization helpers