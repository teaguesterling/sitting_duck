# Test Status After Unified Backend Refactor

## ✅ ACTIVE TESTS (Should Work)
- `basic_unified_backend.test` - NEW: Core functionality
- `language_adapters.test` - NEW: Language-specific features  
- `basic/extension_loading.test` - Extension loading
- `basic/error_handling.test` - Error handling
- `basic/json_explicit_load.test` - JSON extension compatibility

## 🔧 NEEDS SCHEMA UPDATES (Move back after fixing)
Located in `test/needs_update/`:
- `duckdb_ast.test` - Main functionality tests (update column names)
- `parsing/` - Language-specific parsing tests (update schema)

**Required Changes:**
- Replace `source_text` → `peek`  
- Add new taxonomy columns: `kind`, `universal_flags`, `semantic_id`, `super_type`, `arity_bin`
- Update `parse_ast()` calls (now returns table, not JSON)

## 📦 ARCHIVED (Future Implementation)
Located in `test/archive/`:
- `clean_api.test` - Method chaining API (Phase 3+)
- `api_design.test` - Advanced API features
- `ast_objects/` - Complex AST struct operations

**Reason:** These test features not yet implemented in unified backend

## 🗑️ DELETED
- `test/sql/disabled/` - Completely obsolete tests

## 🤔 UNKNOWN STATUS (Review Needed)
- `ast_struct.test` - May work, needs testing
- `hybrid_ast_objects.test` - May work with unified backend
- `macro_*.test` - SQL macros may need updates
- `short_names.test` - Function registration system
- `count_fields.test`, `counts_feature.test` - Simple functionality

## TESTING PRIORITY

### Immediate (Post-Build):
1. Run `basic_unified_backend.test`
2. Run `language_adapters.test`  
3. Run `basic/` tests

### Next Sprint:
1. Fix and move back `needs_update/duckdb_ast.test`
2. Fix and move back `needs_update/parsing/`
3. Test unknown status files

### Future:
1. Implement features needed for `archive/` tests
2. Add performance regression tests
3. Add cross-language consistency tests

## QUICK VALIDATION
Run: `duckdb < test_basic_functionality.sql`