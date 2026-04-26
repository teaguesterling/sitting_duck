# Parse-Time Filtering - Performance at Scale

**Source**: Peer Review Feedback  
**Priority**: P1 (High - Essential for performance)
**Status**: Complete
**Completed**: 2026-04-26

## Overview

Add filtering capabilities directly to `read_ast` to reduce the number of nodes loaded into memory. Critical for handling large codebases efficiently.

## Implemented API

```sql
-- max_depth: cut off traversal at a given depth (root = 0)
SELECT * FROM read_ast('src/**/*.py', max_depth := 3);

-- prune: skip node categories at parse time (before SQL filtering)
SELECT * FROM read_ast('src/**/*.py', prune := ['syntax']);
SELECT * FROM read_ast('src/**/*.py', prune := ['comments']);
SELECT * FROM read_ast('src/**/*.py', prune := ['syntax', 'comments']);

-- Combined
SELECT * FROM read_ast('src/**/*.py', prune := ['syntax'], max_depth := 5);
```

## Implementation Summary

### Parameters Added
- `max_depth` (INTEGER, default -1): Maximum node depth to emit; -1 = unlimited. Boundary nodes have their `children_count` and `descendant_count` zeroed to reflect the truncated view.
- `prune` (VARCHAR[], default []): Named prune policies applied during DFS traversal before emitting nodes. Pruned subtrees are skipped entirely and parent `descendant_count` values are adjusted to remain consistent.

### Prune Policies
| Policy | Nodes Removed |
|--------|---------------|
| `syntax` | Nodes where `is_syntax_only(flags)` is true (punctuation, delimiters, keywords) |
| `comments` | Nodes where `(semantic_type & 252) = 32` (METADATA_COMMENT super-type) |

### Tree Healing
Pruning maintains tree consistency guarantees:
- Every `parent_id` references an emitted node
- `sibling_index` values are contiguous per parent (re-numbered after pruning)
- `descendant_count` reflects only emitted descendants

### Files Changed
- `src/include/ast_extraction_config.hpp` — `SemanticFilter` struct, `prune` field on `ExtractionConfig`
- `src/core/ast_extraction_config.cpp` — `CompilePrunePolicy()` implementation
- `src/functions/read_ast.cpp` — parameter registration and bind-time parsing
- `src/core/ast_reader.cpp` — `ShouldPruneNode()` helper, `max_depth` cutoff, tree-healing DFS integration

## Performance Impact

- **Current**: Parse all nodes, filter in SQL → O(n) memory
- **With `prune := ['syntax']`**: Typically removes 30-60% of nodes before SQL sees them
- **With `max_depth`**: Early cutoff for structure-only queries

## Design Notes

The original design proposed `only_types` include/exclude pattern filtering and language-specific presets. The implemented design chose named semantic policies (`syntax`, `comments`) instead, which are language-agnostic and align with the existing `is_syntax_only()` / semantic-type system. The include/exclude pattern approach remains viable as a future extension.