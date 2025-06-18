# Final Extension Function Count

## Exact Count After Cleanup

### **C++ Functions Registered: 16**
1. **`read_ast`** (1-arg and 2-arg variants) = 2 functions
2. **`parse_ast`** = 1 function  
3. **Semantic type functions** = 12 functions
   - `semantic_type_to_string`, `get_super_kind`, `get_kind`
   - `is_semantic_type`, `semantic_type_code`, `kind_code`, `is_kind`
   - `is_definition`, `is_call`, `is_control_flow`, `is_identifier`
   - `get_searchable_types`
4. **`ast_supported_languages`** = 1 function

**Note:** `read_ast_objects` functions completely removed - not used by CLI or queries.

### **SQL Macros Loaded: 55**
Loaded automatically from 5 embedded SQL files:
- **01_core_primitives.sql**: 10 macros (indexing, filtering, AST operations)
- **02_ast_get.sql**: 8 macros (tree-preserving operations)  
- **03_ast_find.sql**: 11 macros (node extraction)
- **04_ast_to.sql**: 11 macros (format transformations)
- **05_taxonomy.sql**: 15 macros (semantic classification)

### **Total: 71 Functions**
- **16 registered C++ functions** (core table/scalar functions)
- **55 SQL macros** (analysis/navigation library)

## Functions Removed

### **Deprecated C++ Functions Removed:**
- `read_ast_streaming` (redundant duplicate)
- `read_ast_objects` + `read_ast_objects_hybrid` (unused by CLI/queries)
- `ast_functions`, `ast_classes`, `ast_imports` (legacy JSON-based)
- Short names pragma function
- **~10 functions removed**

### **SQL Macros Removed:**
- **42 short-name aliases** (`get_functions` vs `ast_get_functions`)
- Auto-loading and configuration complexity
- **42 macros removed**

## Before vs After

| Category | Before Cleanup | After Cleanup | Reduction |
|----------|---------------|---------------|-----------|
| C++ Functions | ~24 | 16 | -33% |
| SQL Macros | 97 | 55 | -43% |
| **Total Functions** | **~121** | **71** | **-41%** |

## API Consistency

**All functions now use consistent naming:**
- C++ functions: `read_ast()`, `parse_ast()`, `semantic_type_to_string()`
- SQL macros: `ast_get_functions()`, `ast_find_calls()`, `ast_to_names()`

**No more:**
- Multiple names for same function (`read_ast` vs `read_ast_streaming`)
- Short name confusion (`get_functions` vs `ast_get_functions`)
- Conditional loading (pragma-based macro loading)
- Unused object-based APIs

The extension now ships a **clean, focused set of 71 functions** with consistent naming and no redundancy.