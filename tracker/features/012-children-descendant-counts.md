# Feature: Children and Descendant Counts

## Summary
Added `children_count` and `descendant_count` fields to the AST node struct to enable O(1) subtree operations.

## Implementation Details

### Fields Added
- `children_count`: Direct number of children for each node
- `descendant_count`: Total number of descendants (children + their descendants recursively)

### Algorithm
Used a two-pass approach with the existing stack-based traversal:
1. First pass: Create nodes and set `children_count` directly from tree-sitter
2. Second pass: Calculate `descendant_count` by summing children + their descendants

### Changes Made
1. Updated struct definition in both `Bind` functions to include new fields
2. Modified `ParseFileToStructs` to use two-pass traversal with `processed` flag
3. Updated struct value creation to include the new fields
4. Fixed compilation error in `duckdb_ast_extension.cpp` (Fetch API change)

## Benefits
- Enables O(1) filtering of subtrees by size
- Allows efficient identification of leaf nodes (children_count = 0)
- Supports finding complex vs simple nodes (descendant_count thresholds)
- No performance overhead - calculated during initial parse

## Usage Examples
```sql
-- Find all leaf nodes
SELECT * FROM nodes WHERE children_count = 0;

-- Find complex functions (more than 50 descendants)
SELECT * FROM nodes 
WHERE type = 'function_definition' 
AND descendant_count > 50;

-- Get subtree size for any node
SELECT node_id, type, descendant_count + 1 as subtree_size
FROM nodes;
```

## Status
Implementation complete. Fields are now available in the struct output of `read_ast_objects`.