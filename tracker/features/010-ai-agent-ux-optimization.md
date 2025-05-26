# AI Agent UX Optimization

**Source**: Peer Review Recommendations
**Priority**: P2 (Important for adoption)
**Status**: Planning

## Simplified Entry Points

### Progressive Disclosure Pattern

```sql
-- Level 1: Simple queries (90% of AI agent use cases)
SELECT * FROM code_functions('project/');
SELECT * FROM code_classes('project/');

-- Level 2: Filtered queries
SELECT * FROM code_functions('project/') 
WHERE name LIKE 'test_%';

-- Level 3: Full AST access (when needed)
SELECT ast(nodes).find_type('function_definition')
FROM read_ast_objects('project/', 'python');
```

## Semantic Helper Functions

AI-friendly semantic functions to implement:

```sql
-- "What functions call database operations?"
code_find_callers(function_name, project_path)

-- "What classes implement this interface?"  
code_find_implementations(interface_name, project_path)

-- "What's the complexity of this function?"
code_complexity(function_name, file_path)

-- "Find all test functions"
code_test_functions(project_path)

-- "Find all API endpoints"
code_api_endpoints(project_path)
```

## Enhanced Error Context

```sql
-- Current
SELECT ast(nodes).find_type('function_definition').count_elements();

-- Proposed enhancement with error context
SELECT ast(nodes)
    .find_type('function_definition')
    .with_context()  -- Adds file_path, line numbers to results
    .count_elements();
```

## Query Explanation for Debugging

Add capability to explain what an AST query is doing:
- Show query plan
- Highlight performance bottlenecks
- Suggest optimizations

## Implementation Recommendations

1. Start with high-level semantic functions
2. Add progressive disclosure patterns
3. Implement error context propagation
4. Create query explanation system

## Success Metrics

- Time to first successful query for new users
- Query complexity distribution
- Error rate reduction
- AI agent task completion rate