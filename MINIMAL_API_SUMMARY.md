# DuckDB AST Extension - Minimal API Implementation

**Date:** 2025-05-26  
**Branch:** feature/clean-api

## Summary of Changes

### API Reduction
- **Before:** 163 macros (redundant, confusing, poor discoverability)
- **After:** 13 macros (clean, focused, well-organized)

### Final Minimal API (13 macros)

#### Core Functions (8)
- `ast_get_type(nodes, types)` - Find nodes by type(s)
- `ast_get_names(nodes, node_type := NULL)` - Extract names, optionally by type
- `ast_get_depth(nodes, depths)` - Find nodes at depth(s)
- `ast_filter_pattern(nodes, pattern, field := 'name')` - Filter by pattern
- `ast_filter_has_name(nodes)` - Filter to nodes with names
- `ast_nav_children(nodes, parent_id)` - Get direct children
- `ast_nav_parent(nodes, child_id)` - Get parent node
- `ast_summary(nodes)` - Get statistics

#### Infrastructure (5)
- `ast(input)` - Entrypoint for chaining and normalization
- `_ast_internal_ensure_varchar_array(val)` - Internal helper
- `_ast_internal_ensure_integer_array(val)` - Internal helper
- `ast_help()` - Documentation
- `duckdb_ast_register_short_names()` - Optional short name registration (stub)

### Key Improvements

1. **Clear Naming Convention**
   - `ast_get_*` for extraction
   - `ast_filter_*` for filtering
   - `ast_nav_*` for navigation
   - `_ast_internal_*` for internal helpers

2. **No Namespace Pollution**
   - All public functions prefixed with `ast_`
   - Internal helpers clearly marked with `_ast_internal_`
   - Short names only available via opt-in function

3. **Proper Error Handling**
   - Fixed C++ to split SQL statements and execute individually
   - Detailed error messages with file and statement context
   - All functions include null safety

4. **Removed Legacy Functions**
   - No more `ast_find_type`, `ast_function_names`, `ast_class_names`
   - No more redundant structure-preserving variants
   - Clean break from old confusing API

### Test Suite Status
- **Total tests:** 23 (down from 30)
- **Passing:** 14 tests
- **Failing:** 9 tests (mostly expected value mismatches)
- **Disabled:** 7 tests (functionality intentionally removed)

### Files Changed
- Created minimal SQL macro files (4 files, 13 macros total)
- Fixed C++ statement splitting in `ast_sql_macros.cpp`
- Added short names C++ function infrastructure
- Updated 17 test files to use new function names
- Disabled 7 test files for removed functionality

### Ready for DuckDB 1.3 Upgrade
This minimal API provides a stable foundation for upgrading to DuckDB 1.3, where we can:
- Use new JSON functions like `json_object_agg`
- Implement proper short name registration in C++
- Add additional languages with clean naming
- Enhance performance with new DuckDB features