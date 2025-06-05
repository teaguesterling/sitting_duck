# Unified Function Architecture Implementation

## Status: IN PROGRESS 🚧
**Branch:** `unified-function-architecture`  
**Started:** Current session  
**Priority:** Critical - Foundational architecture

## Overview
Unify all 5 AST functions around a single parsing backend while providing clean input/output matrix:

| Input \ Output | Flat Table | AST Struct |
|----------------|------------|------------|
| **String** | `parse_ast()` | `parse_ast_objects()` |
| **File(s)** | `read_ast()` | `read_ast_objects()` |
| **Scalar** | N/A | `to_ast()` |

## Implementation Phases

### Phase 1: Unified Backend ⏳ IN PROGRESS
- [ ] Create `ParseToASTResult()` core function
- [ ] Extract common parsing logic from existing functions
- [ ] Add taxonomy field population (`kind`, `universal_flags`, `semantic_id`)
- [ ] Implement helper functions (`ProjectToTable`, `CreateASTStruct`)
- [ ] Test with simple cases

### Phase 2: Update Existing Functions ⏳ PENDING
- [ ] Update `parse_ast()` to return table instead of JSON
- [ ] Update `read_ast()` to include taxonomy fields
- [ ] Update `read_ast_objects()` to return proper AST struct
- [ ] Remove JSON serialization code
- [ ] Test backward compatibility

### Phase 3: New Functions ⏳ PENDING  
- [ ] Implement `parse_ast_objects()` 
- [ ] Implement `to_ast()` scalar function
- [ ] Test method chaining works
- [ ] Validate consistency across all 5 functions

### Phase 4: Validation ⏳ PENDING
- [ ] Test taxonomy macros with exposed fields
- [ ] Validate cross-language consistency  
- [ ] Performance testing
- [ ] Documentation updates

## Success Criteria
```sql
-- These should all work after implementation:
SELECT parse_ast_objects('def hello(): pass', 'python').get_functions().to_names();
SELECT ast.get_functions().to_locations() FROM read_ast_objects('*.py');
SELECT to_ast('def hello(): pass', 'python').nodes[1].name;
SELECT filter_by_kind(ast.nodes, 4) FROM parse_ast_objects('code', 'python');
```

## Dependencies
- ✅ Parser ownership refactor (completed)
- ✅ O(1) descendant counting (completed)
- ✅ SQL macro redesign (completed)
- ⏳ Taxonomy field exposure (this task)

## Blocks
- **All taxonomy macros** - can't work without exposed fields
- **Method chaining** - need proper AST struct format
- **Self-analysis** - need consistent function behavior
- **Advanced features** - everything builds on this foundation

## Files to Modify
- `src/read_ast_objects_hybrid.cpp` - Add taxonomy fields to schema
- `src/parse_ast_function.cpp` - Switch from JSON to table
- `src/read_ast_function.cpp` - Add taxonomy fields  
- New files for `parse_ast_objects` and `to_ast`
- Update all schema definitions

## Testing Strategy
- Unit tests for `ParseToASTResult()`
- Integration tests for all 5 functions with same input
- Validate taxonomy field population
- Test method chaining works naturally
- Performance benchmarks

## Documentation
- ✅ `docs/UNIFIED_FUNCTION_ARCHITECTURE.md` - Complete architectural plan
- [ ] Update `docs/FUNCTION_ARCHITECTURE.md` - Function purposes
- [ ] Update API documentation
- [ ] Create migration guide for users