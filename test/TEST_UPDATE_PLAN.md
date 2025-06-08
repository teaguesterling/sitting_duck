# Test Update Plan for New Semantic Type System

## Overview
This document outlines all test files that need updating to support the new semantic type system, which replaces the old KIND taxonomy with a more sophisticated semantic classification approach.

## Key Changes Required

### 1. Schema Updates
- Remove references to `kind` field
- Remove references to `kind_id` 
- Update to use new semantic fields:
  - `semantic_type` (VARCHAR) - Human-readable semantic category
  - `type_category` (TINYINT) - Numeric category ID
  - `semantic_kind` (VARCHAR) - Sub-category within type_category

### 2. Test Files Requiring Updates

#### A. Core Test Files Using Old Taxonomy

1. **test/needs_update/parsing/normalized_types.test**
   - Uses `normalized_type` extensively
   - Needs to be updated to use `semantic_type` instead
   - Test cross-language semantic equivalence

2. **test/sql/basic_unified_backend.test**
   - References `kind`, `normalized_type` in schema checks
   - Line 21: Lists old taxonomy fields
   - Line 34: Tests type normalization with old field

3. **test/sql/language_adapters.test**
   - Tests `normalized_type` across languages
   - Lines 9-41: All use `normalized_type`
   - Needs update to test `semantic_type` consistency

4. **test/needs_update/parsing/cpp.test**
   - May reference KIND values
   - Needs examination for taxonomy usage

5. **test/sql/basic/error_handling.test**
   - May contain KIND references in error tests

#### B. SQL Macro Files

1. **src/sql_macros/05_taxonomy.sql**
   - ENTIRE FILE needs rewriting
   - All functions use old KIND system
   - Functions to update:
     - `ast_filter_by_kind` → `ast_filter_by_semantic_type`
     - `ast_filter_functions_by_kind` → `ast_filter_functions_semantic`
     - Remove numeric KIND constants
     - Update to use semantic_type strings

#### C. Performance Test Files

1. **test/simple_performance_test.sql**
   - Struct creation needs new semantic fields
   - Currently missing taxonomy fields entirely

2. **test/performance_test.sql**
   - May need updates for semantic queries

3. **test/working_performance_test.sql**
   - Check for taxonomy usage

#### D. Demo and Example Files

1. **test/sql/find_function_demo.sql**
   - Likely uses function finding with old taxonomy

2. **test/demo_get_source.sql**
   - May reference taxonomy in examples

3. **test/demo_source_extraction.sql**
   - Check for taxonomy usage

#### E. Struct-based Tests

1. **test/sql/hybrid_ast_objects.test**
   - Uses old struct format without semantic fields
   - Tests struct access patterns that need updating

2. **test/sql/ast_struct.test**
   - Tests AST struct functionality
   - Needs updates for new semantic fields in struct

#### F. Data Files
- Test data files appear clean (no taxonomy references)
- May need to create new test cases for semantic types

## Update Strategy

### Phase 1: Core Schema Updates
1. Update `basic_unified_backend.test` to reflect new schema
2. Update `language_adapters.test` for semantic_type testing
3. Update `normalized_types.test` to test semantic classification

### Phase 2: SQL Macro Updates
1. Rewrite `05_taxonomy.sql` with new semantic functions
2. Create new macro functions:
   - `ast_filter_by_semantic_type(nodes, type_string)`
   - `ast_filter_by_type_category(nodes, category_id)`
   - `ast_group_by_semantic_type(nodes)`

### Phase 3: Integration Tests
1. Update all demo files
2. Update performance tests with new fields
3. Add new tests for semantic type features

### Phase 4: New Test Coverage
Create new test files:
1. `test/sql/semantic_types.test` - Core semantic type testing
2. `test/sql/type_categories.test` - Category-based filtering
3. `test/sql/cross_language_semantics.test` - Cross-language equivalence

## Testing Priorities

1. **Critical**: Files that will break immediately
   - `basic_unified_backend.test`
   - `05_taxonomy.sql`

2. **High**: Core functionality tests
   - `language_adapters.test`
   - `normalized_types.test`

3. **Medium**: Feature tests
   - Demo files
   - Performance tests

4. **Low**: Documentation and examples
   - README updates
   - Example queries

## Notes

- The `universal_flags` field appears to be retained
- The struct-based AST format needs updating in all relevant tests
- Cross-language semantic equivalence is a key testing goal
- Performance tests should validate that new system doesn't regress