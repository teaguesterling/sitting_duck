# ast_get_source() - Extract Source Code with Context

**Source**: Peer Review Feedback
**Priority**: P1 (High - Critical for practical use)
**Status**: Ready for Implementation

## Overview

Extract source code for any AST node with surrounding context lines. This is the #1 feature from peer review that would make the extension immediately more practical.

## API Design

```sql
-- Function signature
ast_get_source(node_id, context_lines := 0) → TEXT

-- Macro version for chaining
.get_source(context_lines := 0)
```

## Usage Examples

```sql
-- Get source code for a specific function
SELECT 
    name,
    ast_get_source(node_id, 3) as source_with_context
FROM read_ast_objects('file.py', 'python')
WHERE type = 'function_definition'
  AND name = 'process_payment';

-- Chain syntax
SELECT ast(nodes)
    .filter_pattern('%test%')
    .get_source(2) as test_functions
FROM read_ast_objects('tests/*.py', 'python');
```

## Implementation Strategy

We already have everything needed:
1. Source code is available during parsing
2. Node positions (start_row, end_row) are in our AST
3. Just need to:
   - Store source content during parse
   - Extract lines based on position
   - Add context lines before/after

## Benefits

- **Immediate Value**: Users can see actual code, not just metadata
- **AI/LLM Friendly**: Perfect for code analysis workflows  
- **Simple Implementation**: Leverages existing position data
- **High Impact**: Transforms usability of the extension

## Next Steps

1. Add source storage to parser context
2. Implement line extraction logic
3. Create both function and macro versions
4. Add tests with various context sizes