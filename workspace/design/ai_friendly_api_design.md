# AI-Friendly AST API Design

## Core Principles for AI Usability

1. **Self-Documenting**: Parameter names should explain their purpose
2. **Sensible Defaults**: Most queries should work with minimal parameters
3. **Flexible Inputs**: Accept both single values and lists where logical
4. **Predictable Patterns**: Similar operations should have similar signatures
5. **Clear Errors**: Guide toward correct usage when something fails

## Proposed Improvements

### 1. List Parameters for All Filter Operations

```sql
-- OLD: Multiple calls needed
ast_combine(
    ast_filter_type(ast_obj, 'function_definition'),
    ast_filter_type(ast_obj, 'method_definition')
)

-- NEW: Accept list or single value
ast_filter_type(ast_obj, ['function_definition', 'method_definition'])
ast_filter_type(ast_obj, 'function_definition')  -- Still works

-- Implementation handles both:
CREATE OR REPLACE MACRO ast_filter_type(ast_obj, type_filter) AS (
    CASE 
        WHEN type_filter IS LIST THEN
            -- Filter by any type in list
            ast_filter_types_impl(ast_obj, type_filter)
        ELSE
            -- Single type filter
            ast_filter_type_impl(ast_obj, type_filter)
    END
);
```

### 2. Named Parameters with Defaults

```sql
-- OLD: Unclear what '5' means
ast_extract_source(ast_obj, 5)

-- NEW: Self-documenting with defaults
ast_extract_source(ast_obj, pad_lines := 5)
ast_extract_source(ast_obj)  -- Uses default pad_lines := 0

-- More examples with named parameters:
ast_at_depth_range(ast_obj, min_depth := 2, max_depth := 4)
ast_filter_name_pattern(ast_obj, pattern := '%test%', case_sensitive := false)
ast_extract_entities(ast_obj, include_anonymous := false)
ast_descendants_of(ast_obj, node_id := 42, max_depth := 3)
```

### 3. Consistent Parameter Ordering

Always follow: `(ast_object, primary_filter, options...)`

```sql
-- Good: AST object first, then what to find, then how
ast_filter_type(ast_obj, types := ['function_definition'])
ast_at_line(ast_obj, line := 42, include_partial := true)
ast_extract_source(ast_obj, node_ids := [1, 2, 3], pad_lines := 5)

-- Bad: Inconsistent ordering
ast_filter_type(types := ['function_definition'], ast_obj)  -- Don't do this
```

### 4. Intelligent Type Coercion

```sql
-- All of these should work:
ast_filter_type(ast_obj, 'function_definition')
ast_filter_type(ast_obj, ['function_definition'])
ast_filter_type(ast_obj, types := 'function_definition')
ast_filter_type(ast_obj, types := ['function_definition', 'class_definition'])

-- For node IDs, accept various formats:
ast_children_of(ast_obj, 42)
ast_children_of(ast_obj, '42')
ast_children_of(ast_obj, node_id := 42)
ast_children_of(ast_obj, parent_id := 42)  -- Alias for clarity
```

### 5. Common Pattern Shortcuts

```sql
-- Instead of complex chains, provide semantic shortcuts:
ast_methods(ast_obj)  
-- Equivalent to: ast_filter_types(ast_obj, ['method_definition', 'function_definition']).ast_has_parent_type('class_definition')

ast_public_api(ast_obj)
-- Equivalent to: ast_filter_types(ast_obj, ['function_definition', 'class_definition']).ast_at_depth_range(0, 2).ast_not_pattern('_*')

ast_test_functions(ast_obj)
-- Equivalent to: ast_filter_type(ast_obj, 'function_definition').ast_filter_name_pattern('test_*')

ast_error_handlers(ast_obj)
-- Equivalent to: ast_filter_types(ast_obj, ['try_statement', 'except_clause'])
```

### 6. Helpful Error Messages

```sql
-- When AI makes common mistakes:
ast_filter_type(ast_obj, 'function')
-- ERROR: Unknown node type 'function'. Did you mean 'function_definition'? 
-- Available types: function_definition, class_definition, module, ...

ast_extract_source(ast_obj, 5)
-- WARNING: Positional parameter deprecated. Use ast_extract_source(ast_obj, pad_lines := 5)

ast_at_line(ast_obj, line := -1)
-- ERROR: Line number must be positive. Use line := 1 for first line.
```

### 7. Discovery Functions

Help AI agents learn what's available:

```sql
-- Discover available node types in current AST
SELECT ast_available_types(ast_obj) as types
FROM read_ast_objects('file.py', 'python') as ast_obj;
-- Returns: ['module', 'function_definition', 'class_definition', ...]

-- Get macro documentation
SELECT ast_macro_help('ast_filter_type');
-- Returns: description, parameters, examples

-- Suggest macros based on intent
SELECT ast_suggest_macro('find functions');
-- Returns: ['ast_filter_type', 'ast_methods', 'ast_extract_function_names']
```

## Complete API Examples for AI Agents

### Example 1: Find Test Functions
```sql
-- AI Intent: "Find all test functions in the codebase"
-- Clear, self-documenting query:
SELECT 
    file_path,
    ast_extract_function_names(test_functions) as test_names
FROM read_ast_objects('**/*.py', 'python') as ast_obj,
     LATERAL ast_filter_name_pattern(
         ast_obj, 
         pattern := 'test_*',
         types := ['function_definition', 'method_definition']
     ) as test_functions
WHERE test_functions.node_count > 0;
```

### Example 2: Extract Complex Code
```sql
-- AI Intent: "Show me complex functions with their source code"
WITH complex_functions AS (
    SELECT 
        ast_obj.file_path,
        ast_filter_type(ast_obj, 'function_definition') as funcs
    FROM read_ast_objects('src/**/*.py', 'python') as ast_obj
)
SELECT 
    src.file_path,
    src.node_name as function_name,
    src.line_count,
    src.source_with_context
FROM complex_functions cf,
     TABLE(ast_extract_source(
         cf.funcs,
         pad_lines := 3,
         min_lines := 20  -- Only functions with 20+ lines
     )) as src
ORDER BY src.line_count DESC;
```

### Example 3: API Documentation
```sql
-- AI Intent: "Find all public API functions with their docstrings"
SELECT 
    api.file_path,
    api.entity_name,
    api.entity_type,
    docs.docstring
FROM read_ast_objects('lib/**/*.py', 'python') as ast_obj,
     TABLE(ast_extract_entities(
         ast_public_api(ast_obj),
         include_docstrings := true
     )) as api,
     TABLE(ast_extract_docstrings(ast_obj, node_id := api.node_id)) as docs
WHERE api.entity_name NOT LIKE '_%';  -- Public names only
```

## Recommended Macro Signatures

```sql
-- Filtering (structure-preserving)
ast_filter_type(ast_obj, types, include_subtypes := false)
ast_filter_name_pattern(ast_obj, pattern, case_sensitive := false, types := NULL)
ast_at_depth_range(ast_obj, min_depth := 0, max_depth := NULL)
ast_at_line_range(ast_obj, start_line, end_line := NULL, include_partial := true)

-- Extraction (type-converting)  
ast_extract_source(ast_obj, node_ids := NULL, pad_lines := 0, min_lines := 0)
ast_extract_entities(ast_obj, types := NULL, include_anonymous := false, include_docstrings := false)
ast_extract_summary(ast_obj, include_metrics := false)

-- Navigation (structure-preserving)
ast_children_of(ast_obj, parent_id, direct_only := true)
ast_ancestors_of(ast_obj, node_id, max_levels := NULL)
ast_siblings_of(ast_obj, node_id, include_self := false)

-- Shortcuts (semantic operations)
ast_methods(ast_obj, include_private := true)
ast_public_api(ast_obj, max_depth := 2)
ast_test_functions(ast_obj, patterns := ['test_*', '*_test'])
```

## Benefits for AI Agents

1. **Discoverable**: Can query for available operations and types
2. **Forgiving**: Accepts multiple input formats, coerces types
3. **Self-Documenting**: Named parameters explain intent
4. **Predictable**: Consistent patterns across similar operations
5. **Helpful**: Error messages guide toward correct usage
6. **Semantic**: High-level shortcuts for common tasks

This design significantly reduces the learning curve and makes the API more intuitive for AI agents to use correctly.