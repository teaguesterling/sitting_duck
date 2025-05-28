# DuckDB AST Extension - AI Agent Usage Guide

**Last Updated:** 2025-05-27  
**Extension Version:** 1.1 (Clean API + Peer Review Features)

## Overview

The DuckDB AST Extension provides AI agents with powerful code analysis capabilities through a clean, chainable SQL interface. It supports Python, JavaScript, and C++ with plans for SQL support.

## Quick Start

### 1. Load the Extension
```sql
-- In DuckDB CLI or any SQL interface
LOAD 'duckdb_ast';
```

### 2. Basic Usage - Read and Analyze Code
```sql
-- Read Python file and get all function names
SELECT ast_get_names(nodes, node_type:='function_definition') as functions
FROM read_ast_objects('myfile.py', 'python');

-- Count total nodes in a JavaScript file
SELECT json_extract(ast_summary(nodes), '$.total_nodes') as node_count
FROM read_ast_objects('script.js', 'javascript');

-- Find all C++ classes
SELECT ast_get_names(nodes, node_type:='class_definition') as cpp_classes
FROM read_ast_objects('main.cpp', 'cpp');
```

## Key AI Features

### 1. Method Chaining (Most Important for AI)

Enable beautiful, readable queries with method chaining:

```sql
-- Enable short names and method chaining
SELECT duckdb_ast_register_short_names();

-- Now you can chain methods fluently
SELECT ast(nodes)
    .get_type('function_definition')     -- Find all functions
    .filter_pattern('%test%')           -- Filter to test functions
    .count_nodes()                      -- Count them
FROM read_ast_objects('test_file.py', 'python');

-- Real-world example: Find complex functions
SELECT ast(nodes)
    .get_type('function_definition')     -- Get functions
    .filter_has_name()                   -- Only named functions
    .get_names()                         -- Extract their names
FROM read_ast_objects('complex_code.py', 'python');
```

### 2. Supported Languages

| Language | File Extensions | Key Node Types |
|----------|----------------|----------------|
| **Python** | `.py` | `function_definition`, `class_definition`, `variable_declaration` |
| **JavaScript** | `.js`, `.jsx` | `function_declaration`, `method_definition`, `class_declaration` |
| **C++** | `.cpp`, `.hpp`, `.cc`, `.h` | `function_definition`, `class_specifier`, `variable_declaration` |

### 3. Core Functions (Always Available)

```sql
-- EXTRACTION FUNCTIONS
ast_get_type(nodes, 'function_definition')           -- Find nodes by type
ast_get_names(nodes, node_type:='class_definition')  -- Extract names (optionally filtered)
ast_get_depth(nodes, [1, 2])                        -- Find nodes at specific depths

-- FILTERING FUNCTIONS  
ast_filter_pattern(nodes, '%test%')                  -- Filter by name pattern
ast_filter_has_name(nodes)                           -- Only nodes with names

-- NAVIGATION FUNCTIONS
ast_nav_children(nodes, parent_id)                   -- Get child nodes
ast_nav_parent(nodes, child_id)                      -- Get parent node

-- ANALYSIS FUNCTIONS
ast_summary(nodes)                                   -- Get statistics and counts

-- SOURCE CODE EXTRACTION (NEW!)
ast_get_source(source_text, start_line, end_line, context_lines := 0)  -- Extract lines from source
ast_extract_source(file_path, start_line, end_line, context_lines := 0) -- Extract from file
parse_ast(code, language)                            -- Parse code string to JSON AST

-- LOCATION & REFERENCE FUNCTIONS (NEW!)
ast_get_locations(nodes)                             -- Extract location info from nodes
ast_get_calls(nodes, root_node_id := NULL)          -- Find all function calls
ast_get_parent_chain(nodes, target_node_id, max_depth := NULL) -- Get ancestors (placeholder)
```

### 4. Chain Methods (After `duckdb_ast_register_short_names()`)

```sql
-- These methods work with ast() entrypoint for chaining
get_type(nodes, types)                -- Same as ast_get_type
get_names(nodes, node_type)           -- Same as ast_get_names  
filter_pattern(nodes, pattern)        -- Same as ast_filter_pattern
count_nodes(nodes)                    -- Count array length
first_node(nodes)                     -- Get first element
last_node(nodes)                      -- Get last element

-- Alternative names for ergonomics
find_type(nodes, types)               -- Alias for get_type
extract_names(nodes, node_type)       -- Alias for get_names
len(nodes)                            -- Alias for count_nodes
```

## AI Agent Scenarios

### Scenario 1: Code Discovery
```sql
-- "What functions are in this Python file?"
SELECT ast_get_names(nodes, node_type:='function_definition') as functions
FROM read_ast_objects('unknown_file.py', 'python');

-- With chaining (more readable)
SELECT ast(nodes).get_type('function_definition').get_names() as functions  
FROM read_ast_objects('unknown_file.py', 'python');
```

### Scenario 2: Code Quality Analysis
```sql
-- "How complex is this codebase?"
SELECT 
    json_extract(ast_summary(nodes), '$.total_nodes') as total_nodes,
    json_array_length(ast_get_type(nodes, 'function_definition')) as function_count,
    json_array_length(ast_get_type(nodes, 'class_definition')) as class_count
FROM read_ast_objects('large_file.py', 'python');

-- With chaining
SELECT 
    ast(nodes).summary() as stats,
    ast(nodes).get_type('function_definition').count_nodes() as functions,
    ast(nodes).get_type('class_definition').count_nodes() as classes
FROM read_ast_objects('large_file.py', 'python');
```

### Scenario 3: Finding Specific Patterns
```sql
-- "Find all test functions"
SELECT ast(nodes)
    .get_type('function_definition')
    .filter_pattern('%test%')
    .get_names() as test_functions
FROM read_ast_objects('test_suite.py', 'python');

-- "Find all classes with 'Manager' in the name"
SELECT ast(nodes)
    .get_type('class_definition') 
    .filter_pattern('%Manager%')
    .get_names() as manager_classes
FROM read_ast_objects('business_logic.py', 'python');
```

### Scenario 4: Cross-Language Analysis
```sql
-- Compare function counts across languages
WITH python_functions AS (
    SELECT json_array_length(ast_get_type(nodes, 'function_definition')) as count
    FROM read_ast_objects('backend.py', 'python')
),
js_functions AS (
    SELECT json_array_length(ast_get_type(nodes, 'function_declaration')) as count  
    FROM read_ast_objects('frontend.js', 'javascript')
),
cpp_functions AS (
    SELECT json_array_length(ast_get_type(nodes, 'function_definition')) as count
    FROM read_ast_objects('core.cpp', 'cpp')
)
SELECT 
    python_functions.count as python_functions,
    js_functions.count as js_functions, 
    cpp_functions.count as cpp_functions
FROM python_functions, js_functions, cpp_functions;
```

### Scenario 5: Source Code Extraction (NEW!)
```sql
-- Extract specific lines from a file with context
SELECT ast_extract_source('complex_algo.py', 45, 52, context_lines := 2) as algorithm_impl;

-- Parse inline code and analyze it
WITH parsed AS (
    SELECT parse_ast('def calculate(x): return x * 2', 'python') as ast
)
SELECT json_extract_string(ast, '$.children[0].name') as function_name,
       json_extract_string(ast, '$.children[0].children[1].type') as return_type
FROM parsed;

-- Get function implementations with source code
SELECT 
    node->>'name' as function_name,
    ast_extract_source(
        'myfile.py', 
        (node->'position'->>'start_row')::INTEGER + 1,
        (node->'position'->>'end_row')::INTEGER + 1,
        context_lines := 1
    ) as source_code
FROM read_ast_objects('myfile.py', 'python'),
     json_each(ast_get_type(nodes, 'function_definition')) as t(node);
```

### Scenario 6: Location and Reference Analysis (NEW!)
```sql
-- Get location information for all named entities
SELECT * FROM ast_get_locations(nodes)
WHERE type = 'function_definition'
FROM read_ast_objects('module.py', 'python');

-- Find all function calls in a file
SELECT 
    called_function,
    call_type,
    line
FROM read_ast_objects('script.py', 'python'),
     ast_get_calls(nodes);

-- Combine with source extraction for context
WITH calls AS (
    SELECT * FROM read_ast_objects('script.py', 'python'),
                  ast_get_calls(nodes)
)
SELECT 
    called_function,
    ast_extract_source('script.py', line, line, context_lines := 1) as call_context
FROM calls;
```

### Scenario 7: Bulk Analysis
```sql
-- Analyze multiple files at once
SELECT 
    filename,
    json_array_length(ast_get_type(nodes, 'function_definition')) as functions,
    json_array_length(ast_get_type(nodes, 'class_definition')) as classes
FROM (
    SELECT 'file1.py' as filename, nodes FROM read_ast_objects('file1.py', 'python')
    UNION ALL
    SELECT 'file2.py' as filename, nodes FROM read_ast_objects('file2.py', 'python')  
    UNION ALL
    SELECT 'file3.py' as filename, nodes FROM read_ast_objects('file3.py', 'python')
);
```

## New Peer Review Features (v1.1)

### Source Code Extraction
The extension now provides powerful source code extraction capabilities:

```sql
-- Extract lines from source text (useful when you already have the code)
ast_get_source(source_text, start_line, end_line, context_lines := 0)

-- Extract lines directly from a file
ast_extract_source(file_path, start_line, end_line, context_lines := 0)

-- Parse code string into AST JSON
parse_ast(code, language)
```

### Location and Analysis Functions
```sql
-- Get location info for all named nodes
ast_get_locations(nodes) 
-- Returns: name, type, start_line, end_line, start_column, end_column

-- Find all function calls in AST
ast_get_calls(nodes, root_node_id := NULL)
-- Returns: called_function, call_type, line

-- Get parent chain (placeholder - requires parent tracking)
ast_get_parent_chain(nodes, target_node_id, max_depth := NULL)
```

### Integration Examples
```sql
-- Full workflow: Parse file and extract function implementations
WITH functions AS (
    SELECT 
        node->>'name' as name,
        (node->'position'->>'start_row')::INTEGER + 1 as start_line,
        (node->'position'->>'end_row')::INTEGER + 1 as end_line
    FROM read_ast_objects('module.py', 'python'),
         json_each(ast_get_type(nodes, 'function_definition')) as t(node)
)
SELECT 
    name,
    ast_extract_source('module.py', start_line, end_line) as implementation
FROM functions;

-- Use the built-in integration macro
SELECT * FROM ast_get_functions_with_source('module.py', 'python', context_lines := 2);
```

## Best Practices for AI Agents

### 1. Always Use Method Chaining
```sql
-- ✅ GOOD: Readable, fluent
SELECT ast(nodes).get_type('function_definition').filter_pattern('%api%').count_nodes()
FROM read_ast_objects('api.py', 'python');

-- ❌ AVOID: Nested function calls (hard to read)
SELECT json_array_length(ast_filter_pattern(ast_get_type(nodes, 'function_definition'), '%api%'))
FROM read_ast_objects('api.py', 'python');
```

### 2. Enable Short Names Once
```sql
-- Do this once per session
SELECT duckdb_ast_register_short_names();

-- Then use clean syntax everywhere
SELECT ast(nodes).get_type('function_definition').get_names();
```

### 3. Handle Edge Cases
```sql
-- Check if file has any functions before analyzing
SELECT 
    CASE 
        WHEN ast(nodes).get_type('function_definition').count_nodes() > 0 
        THEN ast(nodes).get_type('function_definition').get_names()
        ELSE '[]'::JSON 
    END as functions
FROM read_ast_objects('maybe_empty.py', 'python');
```

### 4. Use JSON Functions for Complex Queries
```sql
-- Extract specific fields from nodes
SELECT json_extract_string(node.value, '$.name') as function_name,
       json_extract_string(node.value, '$.start_line') as line_number
FROM read_ast_objects('code.py', 'python') t,
     json_each(ast_get_type(t.nodes, 'function_definition')) as node;
```

## File Locations

### Documentation
- **This guide**: `AI_USAGE.md` (top-level, most current)
- **API summary**: `MINIMAL_API_SUMMARY.md` (technical details)
- **Legacy docs**: `src/sql_macros~/USAGE.md` (outdated, in backup folder)

### Tests and Examples
- **Basic usage**: `test/sql/clean_api.test`
- **Method chaining**: `test/sql/ast_objects/chaining_syntax.test`  
- **AI scenarios**: `test/sql/ast_objects/ai_agent_scenarios.test`
- **Short names**: `test/sql/short_names.test`

### Source Code
- **Core macros**: `src/sql_macros/01_core.sql`
- **Entrypoint**: `src/sql_macros/02a_entrypoint.sql`
- **Chain methods**: `src/sql_macros/02b_chain_methods.sql`

## Common Pitfalls

### 1. Forgetting to Register Short Names
```sql
-- This will fail:
SELECT ast(nodes).get_type('function_definition');  -- ❌ get_type not defined

-- Do this first:
SELECT duckdb_ast_register_short_names();           -- ✅ Enable chaining
SELECT ast(nodes).get_type('function_definition');  -- ✅ Now works
```

### 2. Wrong Node Types for Language
```sql
-- JavaScript uses different node types:
SELECT ast(nodes).get_type('function_declaration')   -- ✅ JavaScript
FROM read_ast_objects('script.js', 'javascript');

SELECT ast(nodes).get_type('function_definition')    -- ❌ Python/C++ only  
FROM read_ast_objects('script.js', 'javascript');
```

### 3. Forgetting Language Parameter
```sql
-- Always specify the language:
SELECT * FROM read_ast_objects('file.py', 'python');     -- ✅ Correct
SELECT * FROM read_ast_objects('file.py');               -- ❌ Will fail
```

## Future Features (Coming Soon)

- **SQL Language Support**: Analyze DDL, DML, and DQL statements
- **Parent Tracking**: Full implementation of `ast_get_parent_chain` for scope analysis
- **Context-Aware Normalization**: Better method vs function differentiation  
- **Multi-Part Names**: Handle `schema.table.column` style references
- **Performance Optimizations**: C++ hot-path for large codebases
- **Columnar AST Format**: Native DuckDB storage for better performance

## Quick Reference Card

```sql
-- Setup (once per session)
LOAD 'duckdb_ast';
SELECT duckdb_ast_register_short_names();

-- Common patterns
ast(nodes).get_type('TYPE').get_names()                    -- Find all names of TYPE
ast(nodes).get_type('TYPE').filter_pattern('%PATTERN%')    -- Filter by pattern
ast(nodes).get_type('TYPE').count_nodes()                  -- Count results
ast(nodes).summary()                                       -- Get file statistics

-- Supported languages: 'python', 'javascript', 'cpp'
-- Key types: function_definition, class_definition, method_definition, variable_declaration
```

This extension transforms code analysis from complex file parsing into simple, readable SQL queries that any AI agent can easily use and understand.