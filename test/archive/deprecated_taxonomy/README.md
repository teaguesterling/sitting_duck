# Deprecated Taxonomy Tests Archive

This directory contains test files that use the old KIND/normalized_type taxonomy system, which has been replaced by the unified semantic_type system.

## Archived Files

### From `needs_update/` directory:
- **`duckdb_ast.test`**: Basic tests using old schema
- **`parsing/normalized_types.test`**: Comprehensive tests of the old normalized_type field
- **`parsing/cpp.test`**: C++ tests using normalized_type
- **`parsing/javascript.test`**: JavaScript tests using normalized_type  
- **`parsing/language_aliases.test`**: Language alias tests
- **`parsing/python_classes.test`**: Python class tests using normalized_type
- **`parsing/python_functions.test`**: Python function tests using normalized_type

### From `sql/` directory:
- **`basic_unified_backend.test`**: Schema tests referencing kind, normalized_type, super_type
- **`language_adapters.test`**: Language adapter tests using normalized_type

## Why These Were Archived

These tests were designed for the original taxonomy system that used:
- `kind` field (TINYINT) - Broad semantic category
- `normalized_type` field (VARCHAR) - Cross-language type normalization  
- `super_type` field (TINYINT) - Subcategory within kind

This system has been replaced by a unified `semantic_type` field (TINYINT) that uses 8-bit encoding to combine all taxonomy information in a single efficient field.

## Current Test Approach

New tests should use:
- `test/sql/test_semantic_types.test` - Core semantic type functionality
- `test/sql/semantic_type_validation.test` - Validation and correctness
- `test/performance/semantic_type_performance.test` - Performance benchmarks

## Migration Information

If needed, these tests could be migrated using:
```bash
python scripts/migrate_tests_to_semantic_types.py
```

However, the new semantic type tests are more comprehensive and performant, so migration is only recommended for specific edge cases that might not be covered by the new test suite.

## Reference Value

These archived tests serve as:
1. **Historical reference** for the old taxonomy design
2. **Migration verification** - ensuring new system covers same use cases
3. **Regression testing** - if needed to verify behavior changes

## Schema Differences

### Old Schema (archived tests):
```sql
SELECT node_id, type, normalized_type, name, kind, super_type, universal_flags
FROM parse_ast('code', 'language');
```

### New Schema (current tests):
```sql  
SELECT node_id, type, semantic_type, name, universal_flags
FROM parse_ast('code', 'language');
```

The `semantic_type` field replaces `kind`, `normalized_type`, and `super_type` with a single 8-bit encoded value that provides the same semantic information more efficiently.