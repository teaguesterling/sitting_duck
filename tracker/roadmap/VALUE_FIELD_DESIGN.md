# AST Value Field Implementation Design

## Overview

This document outlines the design for implementing a dedicated `value` field in the AST output to complement the existing `name` field. This will enable better semantic representation of associative nodes (key-value pairs) while maintaining backward compatibility.

## Phase 1: Literal Value Unification (COMPLETED)

**Goal**: Ensure all literals have their text content in both `name` and `value` fields.

**Implementation**:
- Updated all language `.def` files to use `NODE_TEXT` for both name and value strategies for literals
- Ensures consistency across all languages
- No data loss - literal values are accessible in both fields

**Status**: ✅ Complete

## Phase 2: Add Value Column to Output Schema

**Goal**: Expose the `value` field in the query output alongside the existing `name` field.

### Technical Requirements

1. **Modify Output Schema**:
   - Add `value VARCHAR` column to the table function output
   - Position after `name` column for logical grouping
   - Make nullable (most nodes won't have values)

2. **Update AST Backend**:
   - Modify `unified_ast_backend.cpp` to include value extraction
   - Update the table function bindings to include the new column
   - Ensure backward compatibility (existing queries should still work)

3. **Performance Considerations**:
   - Value extraction happens during parsing (no additional traversal)
   - Minimal memory overhead (only populated when needed)
   - No impact on existing `name` field performance

### Implementation Steps

1. Add value field to AST node structure
2. Update table function schema definition
3. Populate value field during node processing
4. Add unit tests for value extraction

## Phase 3: Implement Key-Value Extraction Strategies

**Goal**: Create specialized extraction strategies for associative nodes.

### New Extraction Strategies

```cpp
enum class ExtractionStrategy {
    NONE,
    NODE_TEXT,
    FIND_IDENTIFIER,
    FIND_PROPERTY,
    FIND_KEY,        // NEW: Extract key from associative nodes
    FIND_VALUE       // NEW: Extract value from associative nodes
};
```

### Implementation Approach

1. **FIND_KEY Strategy**:
   - For object properties: Find first identifier child before separator
   - For named arguments: Find parameter name node
   - For attributes: Find attribute name

2. **FIND_VALUE Strategy**:
   - For object properties: Find expression after separator (`:`, `=`)
   - For named arguments: Find argument value expression
   - For attributes: Find attribute value

### Example Implementations

```cpp
// JavaScript object property: { timeout: 5000 }
// Node: pair
// - key: identifier "timeout" 
// - value: number "5000"
string ExtractByFindKey(node) {
    return FindChildByType(node, "property_identifier") 
        || FindChildByType(node, "identifier");
}

string ExtractByFindValue(node) {
    // Find child after : or = separator
    return ExtractValueAfterSeparator(node);
}
```

## Phase 4: Language-Specific Implementations

**Goal**: Implement key-value extraction for each language's associative constructs.

### JavaScript/TypeScript

**Target Nodes**:
- `pair` (object properties)
- `assignment_pattern` (default parameters)
- `jsx_attribute` (JSX attributes)

**Example Configurations**:
```cpp
DEF_TYPE(pair, DEFINITION_PROPERTY, FIND_KEY, FIND_VALUE, 0)
DEF_TYPE(assignment_pattern, DEFINITION_PARAMETER, FIND_KEY, FIND_VALUE, 0)
```

### Python

**Target Nodes**:
- `keyword_argument` (function calls)
- `default_parameter` (function definitions)
- `decorator` (with arguments)

**Example Configurations**:
```cpp
DEF_TYPE(keyword_argument, COMPUTATION_ARGUMENT, FIND_KEY, FIND_VALUE, 0)
DEF_TYPE(default_parameter, DEFINITION_PARAMETER, FIND_KEY, FIND_VALUE, 0)
```

### Go

**Target Nodes**:
- `keyed_element` (struct literals)
- `field_declaration` (with tags)

### Ruby

**Target Nodes**:
- `pair` (hash literals)
- `keyword_parameter` (method definitions)

### SQL

**Target Nodes**:
- `column_definition` (with defaults)
- `assignment` (SET clauses)

### C++

**Fallback Approach**:
- Due to complex AST structure, initially only support literals
- Future: template parameters, initializer lists

## Migration Strategy

1. **Backward Compatibility**:
   - Existing queries using only `name` field continue to work
   - Value field is optional/nullable

2. **Documentation**:
   - Clear guidelines on when to use `name` vs `value`
   - Examples for common query patterns

3. **Phased Rollout**:
   - Phase 2: Infrastructure only (value column exists but mostly NULL)
   - Phase 3: Populate for literals (already done via Phase 1)
   - Phase 4: Gradually add associative nodes per language

## Query Examples

```sql
-- Find all timeout configurations > 1000ms
SELECT file_path, name, value
FROM read_ast('**/*.js')
WHERE type = 'pair' 
  AND name = 'timeout' 
  AND CAST(value AS INTEGER) > 1000;

-- Find all functions with default string parameters
SELECT file_path, name as param_name, value as default_value
FROM read_ast('**/*.py')
WHERE type = 'default_parameter'
  AND semantic_type = (SELECT code FROM semantic_types WHERE name = 'LITERAL_STRING');

-- Find all deprecated decorators with reasons
SELECT file_path, value as deprecation_reason
FROM read_ast('**/*.py')
WHERE type = 'decorator'
  AND name = 'deprecated';
```

## Success Metrics

1. **Completeness**: Value field populated for all literals and common associative nodes
2. **Performance**: No regression in query performance
3. **Usability**: Clear distinction between name and value use cases
4. **Adoption**: Users naturally use value field for appropriate queries

## Future Enhancements

1. **Computed Values**: Evaluate simple expressions (e.g., `1024 * 1024` → `1048576`)
2. **Type Inference**: Add inferred type information for values
3. **Value Validation**: Validate values against expected types
4. **Cross-Reference**: Link values to their declarations