# Unified AST Function Architecture

## Overview
This document outlines the architectural plan to unify all 5 AST functions around a single parsing backend while maintaining clean separation between input sources (string vs file) and output formats (table vs struct).

## Current State Analysis
We have **5 functions** with inconsistent implementations:

1. `parse_ast(code, language)` → Currently returns JSON string ❌
2. `read_ast(filepath, language)` → Currently returns flat table ✅ 
3. `read_ast_objects(file_pattern)` → Currently returns wrong struct format ❌
4. `parse_ast_objects(code, language)` → **Not implemented** ❌
5. `to_ast(code, language)` → **Not implemented** ❌

## Problems with Current Architecture
- **Inconsistent backends** - functions use different parsing logic
- **Missing taxonomy fields** - new macros can't access `kind`, `universal_flags`, `semantic_id`
- **Wrong AST struct format** - `read_ast_objects` doesn't return the monad we designed
- **JSON legacy cruft** - `parse_ast` returns JSON instead of table
- **Incomplete function matrix** - missing 2 out of 5 functions

## Target Architecture

### Core Data Structures

#### ASTResult (Internal)
```cpp
struct ASTResult {
    ASTSource source;        // {file_path, language}
    vector<ASTNode> nodes;   // Complete nodes with taxonomy
    
    // Metadata
    std::chrono::time_point<std::chrono::system_clock> parse_time;
    uint32_t node_count;
    uint32_t max_depth;
};
```

#### Complete ASTNode
```cpp
struct ASTNode {
    // Core identity
    uint64_t node_id;           // Semantic ID encoding
    
    // Taxonomy fields (NEW - currently missing)
    uint8_t kind;               // ASTKind enum (0-15)
    uint8_t universal_flags;    // is_keyword, is_punctuation, is_builtin, is_public
    uint64_t semantic_id;       // Full 64-bit semantic encoding
    uint8_t super_type;         // Language-agnostic super category
    uint8_t arity_bin;          // Fibonacci-binned child count
    
    // Type information
    ASTTypeInfo type;           // {raw, normalized}
    ASTNameInfo name;           // {qualified, simple, anonymous}
    
    // Position information
    ASTFilePosition file_position;    // byte/line/column positions
    ASTTreePosition tree_position;    // depth, sibling_index, parent_id
    ASTSubtreeInfo subtree;           // children_count, descendant_count
    
    // Source text
    string peek;                // Node text content
};
```

#### AST Struct (Public API)
```sql
AST_STRUCT {
    source: STRUCT(file_path VARCHAR, language VARCHAR),
    nodes: ASTNode_STRUCT[]
}
```

### Single Parsing Backend

#### Core Parser Function
```cpp
// One function does all the heavy lifting
ASTResult ParseToASTResult(const string& content, 
                          const string& language, 
                          const string& file_path = "<inline>");
```

**Implementation:**
1. Get language handler via `LanguageHandlerRegistry::GetInstance()`
2. Use handler's `GetParser()` for tree-sitter parsing
3. Traverse tree-sitter output in DFS order using stack-based approach
4. For each node:
   - Extract basic info (type, name, position)
   - **Populate taxonomy fields using `GetNodeTypeConfig()`**
   - **Calculate semantic_id using `GenerateSemanticID()`**
   - **Compute descendant_count in O(1) using DFS ordering**
5. Return unified `ASTResult`

### Function Implementations

#### 1. `parse_ast(code, language)` → TABLE
**Purpose:** Parse string content into flat table for SQL analysis  
**Use Case:** Quick analysis of code snippets

```cpp
TableFunction parse_ast_func(...) {
    ASTResult result = ParseToASTResult(code, language, "<inline>");
    return ProjectToTable(result.nodes);
}
```

**Return Schema:**
```sql
TABLE(
    node_id BIGINT,
    type VARCHAR,
    normalized_type VARCHAR, 
    name VARCHAR,
    -- NEW: Taxonomy fields
    kind TINYINT,
    universal_flags TINYINT,
    semantic_id BIGINT,
    super_type TINYINT,
    arity_bin TINYINT,
    -- Position fields
    start_line INTEGER,
    end_line INTEGER,
    start_column INTEGER, 
    end_column INTEGER,
    parent_id BIGINT,
    depth INTEGER,
    sibling_index INTEGER,
    children_count INTEGER,
    descendant_count INTEGER,
    peek VARCHAR
)
```

#### 2. `read_ast(filepath, language)` → TABLE
**Purpose:** Parse file into flat table for SQL analysis  
**Use Case:** Traditional SQL analysis of single files

```cpp
TableFunction read_ast_func(...) {
    string content = ReadFile(filepath);
    ASTResult result = ParseToASTResult(content, language, filepath);
    return ProjectToTable(result.nodes);
}
```

**Return Schema:** Same as `parse_ast`

#### 3. `parse_ast_objects(code, language)` → AST_STRUCT *(NEW)*
**Purpose:** Parse string content into AST struct for method chaining  
**Use Case:** Fluent API on code snippets, scalar context

```cpp
TableFunction parse_ast_objects_func(...) {
    ASTResult result = ParseToASTResult(code, language, "<inline>");
    return CreateASTStruct(result);
}
```

**Return Schema:**
```sql
AST_STRUCT {
    source: STRUCT(file_path VARCHAR, language VARCHAR),
    nodes: STRUCT(
        node_id BIGINT,
        type STRUCT(raw VARCHAR, normalized VARCHAR),
        name STRUCT(qualified VARCHAR, simple VARCHAR, anonymous BOOLEAN),
        kind TINYINT,
        universal_flags TINYINT, 
        semantic_id BIGINT,
        file_position STRUCT(...),
        tree_position STRUCT(...),
        subtree STRUCT(children_count INTEGER, descendant_count INTEGER),
        peek VARCHAR
    )[]
}
```

#### 4. `read_ast_objects(file_pattern)` → AST_STRUCT *(FIXED)*
**Purpose:** Parse file(s) into AST struct for method chaining  
**Use Case:** Cross-file analysis, complex AST operations

```cpp
TableFunction read_ast_objects_func(...) {
    auto files = ExpandFilePattern(file_pattern);
    for (const auto& file : files) {
        string content = ReadFile(file);
        string language = DetectLanguage(file);
        ASTResult result = ParseToASTResult(content, language, file);
        yield CreateASTStruct(result);
    }
}
```

**Return Schema:** Same as `parse_ast_objects`

#### 5. `to_ast(code, language)` → AST_STRUCT *(NEW)*
**Purpose:** Inline AST creation for scalar contexts  
**Use Case:** Quick AST access in SELECT clauses, WHERE conditions

```cpp
ScalarFunction to_ast_func(...) {
    ASTResult result = ParseToASTResult(code, language, "<inline>");
    return CreateASTStructValue(result);
}
```

**Return Type:** Single AST_STRUCT value (not table)

## Function Matrix Summary

| Input \ Output | Flat Table | AST Struct |
|----------------|------------|------------|
| **String** | `parse_ast()` | `parse_ast_objects()` |
| **File(s)** | `read_ast()` | `read_ast_objects()` |
| **Scalar** | N/A | `to_ast()` |

## Implementation Plan

### Phase 1: Create Unified Backend ⏳
**Goal:** Extract and unify parsing logic

1. **Create `ParseToASTResult()` function**
   - Extract common logic from existing functions
   - Use existing `LanguageHandler::GetParser()` pattern
   - Implement O(1) descendant counting
   - **Add taxonomy field population**

2. **Update ASTNode structures**
   - Add missing taxonomy fields to C++ struct
   - Update DuckDB type mappings
   - Test with simple cases

3. **Create helper functions**
   - `ProjectToTable()` - convert nodes to flat table
   - `CreateASTStruct()` - convert to proper AST struct
   - `CreateASTStructValue()` - scalar version

### Phase 2: Update Existing Functions ⏳
**Goal:** Migrate functions to unified backend

1. **Update `parse_ast()`**
   - Remove JSON serialization
   - Use `ParseToASTResult()` + `ProjectToTable()`
   - Add taxonomy fields to schema
   - Test against existing usage

2. **Update `read_ast()`**
   - Use `ParseToASTResult()` + `ProjectToTable()`
   - Add taxonomy fields to schema
   - Maintain backward compatibility

3. **Update `read_ast_objects()`**
   - Use `ParseToASTResult()` + `CreateASTStruct()`
   - Return proper AST struct format
   - **This enables method chaining!**

### Phase 3: Implement Missing Functions ⏳
**Goal:** Complete the function matrix

1. **Implement `parse_ast_objects()`**
   - New table function
   - Use `ParseToASTResult()` + `CreateASTStruct()`
   - Test method chaining: `parse_ast_objects(...).get_functions()`

2. **Implement `to_ast()`**
   - New scalar function
   - Use `ParseToASTResult()` + `CreateASTStructValue()`
   - Test inline usage: `SELECT to_ast('x=1', 'py').nodes[1].name`

### Phase 4: Validation & Testing ⏳
**Goal:** Ensure everything works together

1. **Consistency testing**
   - Same input produces same node_ids across functions
   - Taxonomy fields populated correctly
   - Cross-language validation

2. **Feature validation**
   - Method chaining works: `ast.get_functions().to_locations()`
   - Taxonomy macros work: `filter_by_kind(nodes, 4)`
   - Complex queries across languages

3. **Performance testing**
   - Large file handling
   - Multiple file processing
   - Memory usage validation

## Success Criteria

### Functional Requirements
```sql
-- All these should work after implementation:

-- 1. Method chaining from parse_ast_objects
SELECT parse_ast_objects('def hello(): pass', 'python').get_functions().to_names();

-- 2. Method chaining from read_ast_objects  
SELECT ast.get_functions().to_locations() FROM read_ast_objects('*.py');

-- 3. Scalar usage with to_ast
SELECT to_ast('def hello(): pass', 'python').nodes[1].name;

-- 4. Taxonomy queries work
SELECT filter_by_kind(ast.nodes, 4) FROM parse_ast_objects('code', 'python');

-- 5. Cross-function consistency
WITH parsed AS (SELECT * FROM parse_ast('def foo(): pass', 'python')),
     objects AS (SELECT ast.nodes FROM parse_ast_objects('def foo(): pass', 'python'))
SELECT p.node_id = o.node_id FROM parsed p, unnest(objects) o WHERE p.type = o.type;
```

### Non-Functional Requirements
- **Performance:** No regression in parsing speed
- **Memory:** Reasonable memory usage for large files
- **Compatibility:** Existing queries continue to work
- **Maintainability:** Single place to fix parsing bugs

## Benefits

### For Users
- **Consistent API:** Same fields across all functions
- **Method chaining:** `ast.get_functions().filter_public().to_signatures()`
- **Taxonomy queries:** Cross-language semantic analysis
- **Complete function matrix:** Right tool for every use case

### For Developers
- **Single backend:** One place to add features/fix bugs
- **Clear architecture:** Input source × output format matrix
- **Testability:** Consistent behavior to validate
- **Extensibility:** Easy to add new languages/features

### For the Project
- **Foundation for advanced features:** Temporal analysis, semantic search
- **Dogfooding capability:** Analyze our own codebase
- **User adoption:** Clean, predictable API
- **Long-term maintainability:** Unified architecture

## Risk Mitigation

### Backward Compatibility
- Keep existing function signatures
- Add new fields without removing old ones
- Gradual migration path for users

### Performance Concerns
- Reuse existing optimizations (O(1) descendant counting)
- Profile before/after to ensure no regressions
- Optimize shared backend for common case

### Implementation Complexity
- Incremental approach - one function at a time
- Extensive testing at each phase
- Rollback plan if issues discovered

This architecture provides the foundation for all our advanced features while maintaining clean separation of concerns and consistent user experience across all functions.