# AST Entrypoint Macro Design

## Concept

Create a single `ast()` macro that normalizes all inputs and serves as the starting point for method chaining.

## Syntax

```sql
-- All of these work and do the same thing:
ast(nodes).find_type('function_definition')
ast(table.nodes).find_type('function_definition') 
ast(struct_with_nodes).find_type('function_definition')
ast(some_json_array).find_type('function_definition')
```

## Implementation

```sql
CREATE OR REPLACE MACRO ast(input) AS (
    CASE 
        -- Input is already a JSON array (nodes)
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'ARRAY' THEN input
        
        -- Input is a struct with 'nodes' field  
        WHEN typeof(input) = 'STRUCT' AND struct_extract(input, 'nodes') IS NOT NULL 
        THEN struct_extract(input, 'nodes')
        
        -- Input is a VARCHAR that might be JSON
        WHEN typeof(input) = 'VARCHAR' THEN 
            TRY_CAST(input AS JSON)
            
        -- Input is a single node object, wrap in array
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'OBJECT' 
        THEN json_array(input)
        
        -- Fallback: try to convert to JSON
        ELSE TRY_CAST(input AS JSON)
    END
);
```

## Benefits

### 1. Unified API
```sql
-- Instead of remembering which field to use:
SELECT ast_find_type(nodes, 'function')           -- raw nodes
SELECT ast_find_type(result.nodes, 'function')    -- from struct
SELECT ast_find_type(some_json, 'function')       -- from JSON

-- Just use ast() everywhere:
SELECT ast(nodes).find_type('function')
SELECT ast(result).find_type('function')  
SELECT ast(some_json).find_type('function')
```

### 2. Clear Error Messages
```sql
-- We can add validation and helpful errors:
CREATE OR REPLACE MACRO ast(input) AS (
    CASE 
        WHEN input IS NULL THEN 
            error('AST input cannot be NULL')
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'ARRAY' THEN 
            input
        WHEN typeof(input) = 'STRUCT' AND struct_extract(input, 'nodes') IS NOT NULL THEN 
            struct_extract(input, 'nodes')
        ELSE 
            error('Invalid AST input type: ' || typeof(input) || '. Expected JSON array or struct with nodes field.')
    END
);
```

### 3. Future Extensibility
```sql
-- Easy to add new input types:
CREATE OR REPLACE MACRO ast(input) AS (
    CASE 
        -- Handle read_ast_objects() result directly
        WHEN typeof(input) = 'STRUCT' AND struct_has_field(input, 'nodes') 
        THEN struct_extract(input, 'nodes')
        
        -- Handle file path strings (auto-parse)
        WHEN typeof(input) = 'VARCHAR' AND input LIKE '%.py' 
        THEN (SELECT nodes FROM read_ast_objects(input, 'python'))
        
        -- Handle arrays of nodes
        WHEN typeof(input) = 'JSON' AND json_type(input) = 'ARRAY' 
        THEN input
        
        -- More cases as needed...
    END
);
```

## Usage Examples

### Basic Usage
```sql
-- Current syntax:
SELECT ast_find_type(nodes, 'function_definition')
FROM read_ast_objects('file.py', 'python');

-- New syntax:
SELECT ast(nodes).find_type('function_definition')
FROM read_ast_objects('file.py', 'python');
```

### With Structs
```sql
-- If we return structs from read_ast_objects:
SELECT ast(result).find_type('function_definition')
FROM read_ast_objects('file.py', 'python') AS result;
```

### Direct File Processing
```sql
-- Future: direct file processing
SELECT ast('file.py').find_type('function_definition');
```

### Chaining
```sql
SELECT 
    ast(nodes).find_type('function_definition').extract_names(),
    ast(nodes).find_type('class_definition').count(),
    ast(nodes).summary().max_depth
FROM read_ast_objects('file.py', 'python');
```

## Implementation Plan

1. Create the `ast()` entrypoint macro
2. Keep existing macros for backward compatibility  
3. Create new method-style macros that work with `ast()`
4. Update tests to show both syntaxes
5. Document the new pattern