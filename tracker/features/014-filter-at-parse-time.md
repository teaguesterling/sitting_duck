# Parse-Time Filtering - Performance at Scale

**Source**: Peer Review Feedback  
**Priority**: P1 (High - Essential for performance)
**Status**: Design Phase

## Overview

Add filtering capabilities directly to `read_ast` and `read_ast_objects` functions to reduce the number of nodes loaded into memory. Critical for handling large codebases efficiently.

## API Design

```sql
-- Enhanced read_ast with filtering
read_ast_objects(
    file_pattern,
    language,
    only_types := ['function_definition', 'class_definition'],  -- New!
    max_depth := 3  -- New!
)
```

## Detailed Filter Options

```sql
-- Using our designed node_filter struct
read_ast_objects(
    'src/**/*.py',
    'python', 
    node_filter := {
        include: ['function_*', 'class_*'],  -- Pattern support
        exclude: ['*_test', 'test_*'],       -- Exclude patterns
        preset: 'definitions',               -- Or use presets
        max_depth: 5
    }
)
```

## Language Presets

```python
PRESETS = {
    'definitions': ['function_definition', 'class_definition', 'variable_declaration'],
    'signatures': ['function_definition', 'method_definition'],  
    'imports': ['import_statement', 'import_from_statement'],
    'structure': ['class_definition', 'module', 'namespace']
}
```

## Performance Impact

- **Current**: Parse all nodes, filter in SQL → O(n) memory
- **With Filtering**: Skip unwanted nodes during parse → O(m) memory where m << n
- **Expected**: 5-50x memory reduction for focused queries

## Implementation Notes

1. Modify tree-sitter traversal to skip non-matching nodes
2. Still need to traverse for structure (can't skip children of skipped nodes)
3. Early termination at max_depth
4. Language handlers define their presets

## Example Impact

```sql
-- Before: Loads 50,000 nodes, filters to 500
SELECT * FROM read_ast_objects('large_project/', 'python')
WHERE type IN ('function_definition', 'class_definition');

-- After: Loads only ~2,000 nodes (functions + structure)
SELECT * FROM read_ast_objects('large_project/', 'python',
    only_types := ['function_definition', 'class_definition']);
```

## Next Steps

1. Modify parser traversal logic
2. Add filter parameter to read functions
3. Implement pattern matching for types
4. Define language-specific presets