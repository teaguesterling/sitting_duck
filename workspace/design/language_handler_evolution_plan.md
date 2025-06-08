# Language Handler Evolution Plan

## Current State Analysis

### What We've Learned So Far

1. **JavaScript** revealed:
   - Need for property_identifier support for method names
   - Languages can have distinct node types for methods vs functions

2. **C++** revealed:
   - Return types can appear before function names in AST
   - Need to look inside specific child nodes (function_declarator)
   - Operator overloads need special handling
   - Same node type (function_definition) serves multiple purposes

3. **Common Pattern**:
   - Each language has unique ways of representing similar concepts
   - Static type mappings are insufficient for context-dependent normalization

## SQL-Specific Challenges

SQL will be fundamentally different because:

1. **No Traditional Functions/Classes**:
   - CREATE FUNCTION/PROCEDURE (closest analog)
   - No classes, only schemas/tables
   - No variables in traditional sense, only columns/aliases

2. **New Concepts Need New Normalized Types**:
   - `table_declaration` (CREATE TABLE)
   - `view_declaration` (CREATE VIEW)
   - `index_declaration` (CREATE INDEX)
   - `column_definition` (within CREATE TABLE)
   - `table_reference` (in FROM/JOIN)
   - `column_reference` (in SELECT/WHERE)
   - `cte_declaration` (WITH clauses)
   - `query_statement` (SELECT/INSERT/UPDATE/DELETE)

3. **Name Extraction Complexity**:
   - Table names in different contexts (CREATE vs SELECT FROM)
   - Column names with table prefixes (table.column)
   - Aliases (table AS t, column AS c)
   - Schema-qualified names (schema.table)

## Proposed Interface Changes

### 1. Context-Aware Normalization (Major)

```cpp
// Current
virtual string GetNormalizedType(const string &node_type) const = 0;

// Proposed - Add context parameter
struct NodeContext {
    const string &node_type;
    TSNode node;
    TSNode parent;
    vector<TSNode> ancestors;  // Full path to root
    const string &source_text;
};

virtual string GetNormalizedType(const NodeContext &context) const = 0;
```

**Benefits**:
- Can differentiate method vs function based on parent
- Can handle SQL context (e.g., identifier in CREATE vs SELECT)
- More flexible for future languages

### 2. Multi-Part Name Support (Major)

```cpp
// Current
virtual string ExtractNodeName(TSNode node, const string &content) const = 0;

// Proposed - Return structured name
struct ExtractedName {
    string name;                    // Main name
    string qualifier;               // Schema/namespace/class
    string alias;                   // AS alias
    NameType type;                  // SIMPLE, QUALIFIED, ALIASED
};

virtual ExtractedName ExtractNodeName(TSNode node, const string &content) const = 0;
```

**Benefits**:
- Handle schema.table.column
- Preserve alias information
- Support C++ namespaces (utils::Container)

### 3. Language-Specific Normalized Types (Minor)

```cpp
// Add to base class
virtual bool HasCustomNormalizedTypes() const { return false; }
virtual vector<string> GetCustomNormalizedTypes() const { return {}; }

// SQL handler would override
bool HasCustomNormalizedTypes() const override { return true; }
vector<string> GetCustomNormalizedTypes() const override {
    return {
        "table_declaration",
        "column_definition",
        "table_reference",
        // etc.
    };
}
```

### 4. Relationship Hints (Minor)

```cpp
// Help identify relationships between nodes
enum RelationshipType {
    NONE,
    DEFINES,      // CREATE TABLE defines columns
    REFERENCES,   // SELECT references tables/columns
    ALIASES,      // AS creates alias
    CONTAINS      // Class contains methods
};

virtual RelationshipType GetRelationship(
    const string &parent_type, 
    const string &child_type
) const { return NONE; }
```

## Implementation Strategy

### Phase 1: Minimal SQL Support (Current Interface)
- Add SQL handler with current interface
- Use existing normalized types where possible
- Document limitations

### Phase 2: Context-Aware Normalization
- Update interface to support NodeContext
- Retrofit existing handlers
- Properly differentiate methods vs functions

### Phase 3: Multi-Part Names
- Implement ExtractedName structure
- Update all handlers
- Preserve full name context

### Phase 4: SQL-Specific Types
- Add new normalized types for SQL
- Update documentation
- Add cross-language query examples

## Migration Path

1. **Backward Compatibility**:
   - Keep old methods as deprecated wrappers
   - Gradually migrate callers
   - Remove after all uses updated

2. **Testing Strategy**:
   - Add tests for new functionality
   - Ensure existing tests still pass
   - Add cross-language comparison tests

3. **Documentation**:
   - Update normalized type documentation
   - Add examples for each language
   - Create migration guide

## Future Language Considerations

### Rust
- Modules and crates
- Traits vs structs vs impls
- Lifetime annotations

### Go
- Packages
- Methods on structs vs functions
- Interfaces

### TypeScript
- Type declarations
- Interfaces vs classes
- Decorators

Each will likely reveal new patterns and requirements.

## Recommendation

1. **Start with Phase 1** - Get SQL working with current interface
2. **Document all limitations** we encounter
3. **Implement Phase 2** before adding more languages
4. **Consider Phase 3 & 4** based on actual usage patterns

This evolutionary approach lets us learn from SQL without over-engineering upfront.