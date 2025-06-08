# Clear Implementation Path for AST v2 API

## Overview

Yes, I have a clear path! Starting with test cases is the right approach. Here's the systematic plan:

## Phase 1: Test-Driven Foundation (Day 1)

### 1.1 Create Comprehensive Test Suite
✅ **Created**: `test/sql/ast_v2_macros.test`
- Tests for list parameters
- Tests for structure preservation
- Tests for extraction types
- Tests for named parameters
- Tests for safe variants
- Tests for source extraction

### 1.2 Create Mock Implementations
✅ **Created**: `src/ast_sql_macros_v4_source.sql`
- Improved source reading using `read_text()` and `string_split()`
- Line numbering with `generate_series()`
- Configurable padding and formatting

### 1.3 Run Baseline Tests
```bash
# Document current behavior
./build/release/duckdb test/sql/duckdb_ast.test > baseline_results.txt
# Run new tests (will fail initially)
./build/release/duckdb test/sql/ast_v2_macros.test
```

## Phase 2: Incremental Implementation (Days 2-4)

### 2.1 Enhance Existing Macros (Day 2)
```sql
-- File: src/ast_sql_macros_v4_enhanced.sql

-- Step 1: Add list parameter support
CREATE OR REPLACE MACRO ast_find_type(nodes, types) AS (
    CASE 
        WHEN pg_typeof(types) = 'VARCHAR[]' THEN
            (SELECT json_group_array(je.value)
             FROM json_each(nodes) AS je
             WHERE list_contains(types, json_extract_string(je.value, '$.type')))
        ELSE
            (SELECT json_group_array(je.value)
             FROM json_each(nodes) AS je
             WHERE json_extract_string(je.value, '$.type') = types)
    END
);

-- Step 2: Add safe variants
CREATE OR REPLACE MACRO ast_safe_function_names(nodes) AS (
    COALESCE(ast_function_names(nodes), '[]'::JSON)
);

-- Step 3: Add navigation macros
CREATE OR REPLACE MACRO ast_parent_of(nodes, node_id) AS (...);
CREATE OR REPLACE MACRO ast_ancestors_of(nodes, node_id) AS (...);
```

### 2.2 Implement Structure-Preserving Macros (Day 3)
```sql
-- File: src/ast_sql_macros_v4_structure.sql

-- Core structure-preserving filter
CREATE OR REPLACE MACRO ast_filter_type(ast_obj, types) AS (
    WITH type_list AS (
        SELECT CASE 
            WHEN pg_typeof(types) = 'VARCHAR' THEN [types]
            WHEN pg_typeof(types) = 'VARCHAR[]' THEN types
            ELSE [types::VARCHAR]
        END as type_array
    ),
    filtered AS (
        SELECT json_group_array(je.value) as nodes
        FROM json_each(ast_obj.nodes) AS je, type_list
        WHERE list_contains(type_array, json_extract_string(je.value, '$.type'))
    )
    SELECT STRUCT_PACK(
        file_path := ast_obj.file_path,
        language := ast_obj.language,
        node_count := json_array_length(filtered.nodes),
        max_depth := COALESCE(
            (SELECT MAX(json_extract(n.value, '$.depth')::INTEGER)
             FROM json_each(filtered.nodes) as n), 
            0
        ),
        nodes := COALESCE(filtered.nodes, '[]'::JSON)
    )
    FROM filtered
);
```

### 2.3 Implement Extraction Macros (Day 4)
```sql
-- File: src/ast_sql_macros_v4_extract.sql

-- Extract names as proper VARCHAR[]
CREATE OR REPLACE MACRO ast_extract_names(ast_obj, types DEFAULT NULL) AS (
    SELECT ARRAY_AGG(DISTINCT json_extract_string(je.value, '$.name'))
    FROM json_each(ast_obj.nodes) AS je
    WHERE json_extract_string(je.value, '$.name') IS NOT NULL
      AND (types IS NULL OR 
           list_contains(
               CASE WHEN pg_typeof(types) = 'VARCHAR[]' THEN types ELSE [types] END,
               json_extract_string(je.value, '$.type')
           ))
);

-- Extract entities as table
CREATE OR REPLACE MACRO ast_extract_entities(ast_obj, types DEFAULT NULL) AS (
    SELECT 
        ast_obj.file_path as file_path,
        json_extract(je.value, '$.start.line')::INTEGER as start_line,
        json_extract(je.value, '$.end.line')::INTEGER as end_line,
        json_extract_string(je.value, '$.type') as entity_type,
        json_extract_string(je.value, '$.name') as entity_name,
        json_extract(je.value, '$.parent_id')::BIGINT as parent_id,
        json_extract(je.value, '$.id')::BIGINT as node_id
    FROM json_each(ast_obj.nodes) AS je
    WHERE types IS NULL OR 
          list_contains(
              CASE WHEN pg_typeof(types) = 'VARCHAR[]' THEN types ELSE [types] END,
              json_extract_string(je.value, '$.type')
          )
);
```

## Phase 3: Integration & Testing (Day 5)

### 3.1 Update Extension Loading
```cpp
// In duckdb_ast_extension.cpp
void DuckDBAstExtension::Load(DuckDB &db) {
    // ... existing code ...
    
    // Load v4 macros
    LoadSQLMacros(db, "ast_sql_macros_v4_enhanced.sql");
    LoadSQLMacros(db, "ast_sql_macros_v4_structure.sql");
    LoadSQLMacros(db, "ast_sql_macros_v4_extract.sql");
    LoadSQLMacros(db, "ast_sql_macros_v4_source.sql");
}
```

### 3.2 Run Progressive Tests
```bash
# Test each phase incrementally
make test TEST_PATTERN="ast_v2_macros"

# Fix failures one by one
# Update implementation based on test results
```

### 3.3 Add Backward Compatibility
```sql
-- File: src/ast_sql_macros_v4_compat.sql

-- Redirect old macros to new implementation
CREATE OR REPLACE MACRO ast_function_names(nodes) AS (
    ast_extract_names(
        STRUCT_PACK(nodes := nodes),
        types := 'function_definition'
    )
);
```

## Phase 4: AI Features & Polish (Day 6)

### 4.1 Discovery Functions
```sql
-- Available types in AST
CREATE OR REPLACE MACRO ast_available_types(ast_obj) AS (
    SELECT ARRAY_AGG(DISTINCT json_extract_string(je.value, '$.type'))
    FROM json_each(ast_obj.nodes) AS je
);

-- Semantic shortcuts
CREATE OR REPLACE MACRO ast_test_functions(
    ast_obj, 
    patterns DEFAULT ['test_*', '*_test', 'Test*']
) AS (
    WITH filtered_types AS (
        SELECT ast_filter_type(ast_obj, ['function_definition', 'method_definition']) as filtered
    )
    SELECT ast_filter_name_pattern(filtered, patterns)
    FROM filtered_types
);
```

### 4.2 Performance Testing
```bash
# Create performance benchmark
./build/release/duckdb -c "
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';
.timer on
-- Test queries on large files
"
```

## Phase 5: Documentation & Release (Day 7)

### 5.1 Update Documentation
- Migration guide: `docs/MIGRATION_V2.md`
- Cookbook: `docs/COOKBOOK.md`
- API reference: `docs/API_REFERENCE.md`

### 5.2 Final Testing
```bash
# Run all tests
make test

# Run performance benchmarks
./scripts/benchmark_v2.sh

# Test backward compatibility
./scripts/test_compatibility.sh
```

## Implementation Checklist

- [x] Create comprehensive test suite
- [x] Design source reading implementation
- [ ] Implement list parameter support
- [ ] Implement safe variants
- [ ] Implement structure-preserving macros
- [ ] Implement extraction macros
- [ ] Integrate source reading
- [ ] Add AI-friendly features
- [ ] Update extension loader
- [ ] Test backward compatibility
- [ ] Performance benchmarks
- [ ] Documentation update
- [ ] Release preparation

## Key Implementation Notes

1. **Test First**: Each feature has a test before implementation
2. **Incremental**: Build functionality piece by piece
3. **Backward Compatible**: Old queries continue to work
4. **Performance Aware**: Benchmark after each phase
5. **User Friendly**: Clear errors and helpful defaults

This path ensures we build exactly what we designed, with tests validating each step.