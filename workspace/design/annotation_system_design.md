# DuckDB AST Extension: Annotation System Design Specification

## Executive Summary

This document outlines a comprehensive redesign of the DuckDB AST extension to use an annotation-based architecture. This approach separates the pure tree-sitter AST from enrichment data, enabling lazy evaluation, user extensibility, and cleaner integration with DuckDB's type system.

## 1. Core Architecture

### 1.1 Node Structure

```json
{
    "type": "function_definition",          // Original tree-sitter type (immutable)
    "position": {                           // Original position data (immutable)
        "start_byte": 0,
        "end_byte": 42,
        "start_row": 1,
        "start_column": 0,
        "end_row": 3,
        "end_column": 10
    },
    "text": "def foo(x: int): return x",   // Node text (immutable)
    "children": [...],                      // Child nodes (immutable)
    "annotations": {                        // All enrichment data goes here
        "normalized_type": "function_declaration",
        "extracted_name": "foo",
        "signature": {
            "parameters": [{"name": "x", "type": "int"}],
            "return_type": "int"
        },
        "scope": {
            "module": "utils",
            "class": null,
            "qualified_name": "utils.foo"
        },
        "metrics": {
            "complexity": 1,
            "line_count": 3
        }
    }
}
```

### 1.2 Annotation Types

| Annotation | Description | Applied To | Computed |
|------------|-------------|------------|----------|
| `normalized_type` | Language-agnostic type mapping | All nodes | During parse |
| `extracted_name` | Extracted identifier | Named nodes | During parse |
| `signature` | Function/method signature info | Functions, methods | On request |
| `scope` | Scope and qualified name | Definitions | On request |
| `metrics` | Complexity, size metrics | Functions, classes | On request |
| `docstring` | Extracted documentation | Documented nodes | On request |
| `dependencies` | Called functions, imports | Functions, modules | On request |
| `custom.*` | User-defined annotations | Any | User-defined |

## 2. API Design

### 2.1 Parse-Time Annotations

```sql
-- Minimal parsing (fastest)
read_ast_objects('file.py', 'python', annotations := 'minimal');
-- Only includes: normalized_type

-- Standard parsing (default)
read_ast_objects('file.py', 'python', annotations := 'standard');
-- Includes: normalized_type, extracted_name

-- Full signature parsing
read_ast_objects('file.py', 'python', annotations := 'signatures');
-- Includes: normalized_type, extracted_name, signature, scope

-- Custom annotation selection
read_ast_objects('file.py', 'python', 
    annotations := ['normalized_type', 'signature', 'metrics']);
```

### 2.2 Post-Parse Annotation

```sql
-- Add annotations after parsing
SELECT ast(nodes)
    .with_annotations('signatures')      -- Built-in annotator
    .with_annotations('complexity')      -- Another built-in
    .with_annotations(my_custom_check)   -- User-defined macro
FROM read_ast_objects('file.py', 'python', annotations := 'minimal');

-- Filter by annotations
SELECT ast(nodes)
    .with_annotations('complexity')
    .filter_by_annotation('metrics.complexity', '> 10')
    .get_locations()
FROM cached_ast;
```

### 2.3 Annotation Presets

```sql
-- Language-specific presets
ANNOTATION_PRESETS = {
    'minimal': ['normalized_type'],
    'standard': ['normalized_type', 'extracted_name'],
    'signatures': ['normalized_type', 'extracted_name', 'signature', 'scope'],
    'analysis': ['normalized_type', 'extracted_name', 'signature', 'scope', 'metrics'],
    'all': ['normalized_type', 'extracted_name', 'signature', 'scope', 'metrics', 'docstring', 'dependencies']
}
```

## 3. Integration with Peer Review Features

### 3.1 Feature Mapping

| Peer Feature | Implementation via Annotations |
|--------------|--------------------------------|
| `ast_get_source()` | Use position annotation (always present) |
| `ast_get_parent_chain()` | Add `parent_id` annotation during parse |
| `ast_find_references()` | Use `references` annotation |
| `ast_get_comment_before()` | Add `preceding_comment` annotation |
| `ast_get_calls()` | Use `dependencies.calls` annotation |
| `ast_get_definitions()` | Filter by `normalized_type` annotation |
| `ast_get_imports()` | Use `dependencies.imports` annotation |
| `ast_matches_pattern()` | Pattern match on annotated fields |
| `ast_find_similar()` | Add `structural_hash` annotation |

### 3.2 Updated Feature Implementations

```sql
-- ast_get_locations using annotations
CREATE OR REPLACE MACRO ast_get_locations(nodes) AS (
    SELECT 
        annotations->>'extracted_name' as name,
        type,
        position->>'start_row' as start_line,
        position->>'end_row' as end_line,
        annotations->'scope'->>'qualified_name' as qualified_name
    FROM json_each(nodes) as t(node)
);

-- ast_get_signatures using annotations
CREATE OR REPLACE MACRO ast_get_signatures(nodes) AS (
    SELECT 
        annotations->>'extracted_name' as name,
        annotations->'signature'->>'return_type' as return_type,
        annotations->'signature'->'parameters' as parameters,
        annotations->'scope'->>'qualified_name' as qualified_name
    FROM json_each(nodes) as t(node)
    WHERE annotations->>'signature' IS NOT NULL
);
```

## 4. Native Struct Implementation Considerations

### 4.1 Recursive Type Challenge

DuckDB doesn't natively support recursive struct types, but we have several options:

#### Option 1: Flattened Structure with Parent References
```sql
CREATE TYPE ast_node AS STRUCT(
    node_id INTEGER,
    parent_id INTEGER,
    type VARCHAR,
    position STRUCT(
        start_row INTEGER,
        start_column INTEGER,
        end_row INTEGER,
        end_column INTEGER
    ),
    text VARCHAR,
    annotations MAP(VARCHAR, JSON)  -- Flexible annotation storage
);
```

#### Option 2: Separate Tables for Hierarchy
```sql
-- Nodes table
CREATE TABLE ast_nodes (
    node_id INTEGER,
    type VARCHAR,
    position_start_row INTEGER,
    position_end_row INTEGER,
    text VARCHAR,
    annotations JSON
);

-- Edges table
CREATE TABLE ast_edges (
    parent_id INTEGER,
    child_id INTEGER,
    child_index INTEGER
);
```

#### Option 3: JSON/Struct Hybrid
```sql
CREATE TYPE ast_node_flat AS STRUCT(
    -- Core fields as struct
    type VARCHAR,
    position STRUCT(start_row INT, end_row INT, start_col INT, end_col INT),
    text VARCHAR,
    -- Annotations as flexible JSON
    annotations JSON,
    -- Children as array of IDs
    child_ids INTEGER[]
);
```

### 4.2 Recommended Approach: Hybrid Model

```sql
-- Use structs for non-recursive parts
CREATE TYPE ast_position AS STRUCT(
    start_byte BIGINT,
    end_byte BIGINT,
    start_row INTEGER,
    start_column INTEGER,
    end_row INTEGER,
    end_column INTEGER
);

CREATE TYPE ast_signature AS STRUCT(
    return_type VARCHAR,
    parameter_count INTEGER,
    parameter_names VARCHAR[],
    parameter_types VARCHAR[]
);

-- Main function returns table with annotations as JSON for flexibility
CREATE FUNCTION read_ast_native(
    file_path VARCHAR,
    language VARCHAR,
    annotation_preset VARCHAR DEFAULT 'standard'
) RETURNS TABLE (
    node_id INTEGER,
    parent_id INTEGER,
    depth INTEGER,
    type VARCHAR,
    position ast_position,
    text VARCHAR,
    annotations JSON  -- Flexible for extensibility
);
```

## 5. Implementation Phases

### Phase 1: Annotation Infrastructure (Current Priority)
1. Refactor current JSON output to use annotation structure
2. Move normalized_type and extracted_name to annotations
3. Implement `with_annotations()` macro system
4. Create base annotation presets

### Phase 2: Core Annotations
1. Implement signature extraction for all languages
2. Add scope/qualified name calculation
3. Implement basic metrics (line count, complexity)
4. Add parse-time annotation options to read_ast functions

### Phase 3: Advanced Features
1. User-defined annotation macros
2. Annotation caching system
3. Incremental annotation updates
4. Cross-file dependency annotations

### Phase 4: Native Implementation (Future)
1. Design final struct schema
2. Implement native parser bindings
3. Create migration path from JSON
4. Optimize for recursive queries

## 6. Benefits Summary

1. **Performance**: Only compute what's needed
2. **Extensibility**: Users can add custom annotations
3. **Compatibility**: Preserves tree-sitter AST structure
4. **Flexibility**: JSON annotations allow any enrichment
5. **Composability**: Chain annotation operations
6. **Future-Proof**: Clear path to native implementation

## 7. Open Questions

1. Should we version the annotation schema?
2. How do we handle annotation conflicts (same name, different sources)?
3. Should annotations be strongly typed or remain flexible JSON?
4. What's the migration path for existing users?

## 8. Next Steps

1. Review and finalize annotation schema
2. Implement Phase 1 infrastructure
3. Create comprehensive test suite
4. Document annotation macro creation
5. Gather user feedback on API design