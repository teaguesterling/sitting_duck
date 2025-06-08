# DuckDB AST Extension: Clean API Design

## Design Principles

1. **Consistent Naming**: Clear prefixes indicate function category
2. **Single Pattern**: Extraction pattern only (remove structure-preserving variants)
3. **Null Safety**: All functions handle NULL inputs gracefully
4. **Parameter Flexibility**: Accept single values or arrays where sensible
5. **Composability**: Simple functions that work well together
6. **Discoverability**: Clear entry points and semantic naming

## Naming Conventions

### Core Prefixes
- `ast_get_*` - Simple extraction returning JSON arrays
- `ast_filter_*` - Filtering operations returning filtered JSON arrays
- `ast_nav_*` - Tree navigation (parent, children, siblings, etc.)
- `ast_analyze_*` - Analysis and metrics
- `ast_source_*` - Source code extraction (reads from files)

### Special Functions
- `ast()` - Main entrypoint for normalization and chaining
- `ast_help()` - Discovery and documentation

## Core API (20 functions)

### 1. Entrypoint & Utilities
```sql
ast(input) -> JSON                              -- Normalize input to JSON array
ast_help() -> TABLE                             -- Interactive help system
```

### 2. Basic Extraction (ast_get_*)
```sql
ast_get_type(nodes, types) -> JSON              -- Find nodes by type(s)
ast_get_names(nodes, node_type?) -> JSON        -- Extract names, optionally by type
ast_get_depth(nodes, depths) -> JSON            -- Find nodes at depth(s)
ast_get_line(nodes, lines) -> JSON              -- Find nodes at line(s)
ast_get_range(nodes, start_line, end_line) -> JSON  -- Find nodes in line range
```

### 3. Filtering (ast_filter_*)
```sql
ast_filter_pattern(nodes, pattern, field?) -> JSON     -- Filter by pattern (LIKE or REGEX)
ast_filter_depth_range(nodes, min, max) -> JSON        -- Filter by depth range
ast_filter_has_name(nodes) -> JSON                     -- Filter to nodes with names
```

### 4. Tree Navigation (ast_nav_*)
```sql
ast_nav_children(nodes, parent_id) -> JSON      -- Get direct children
ast_nav_parent(nodes, child_id) -> JSON         -- Get parent node
ast_nav_siblings(nodes, node_id) -> JSON        -- Get sibling nodes
ast_nav_ancestors(nodes, node_id, levels?) -> JSON      -- Get ancestors (with limit)
ast_nav_descendants(nodes, node_id, levels?) -> JSON    -- Get descendants (with limit)
```

### 5. Analysis (ast_analyze_*)
```sql
ast_analyze_summary(nodes) -> JSON              -- Overall statistics
ast_analyze_complexity(nodes) -> JSON           -- Complexity metrics
ast_analyze_types(nodes) -> JSON                -- Type counts
```

### 6. Source Code (ast_source_*)
```sql
ast_source_node(nodes, node_id, file_path) -> VARCHAR       -- Source for specific node
ast_source_function(nodes, name, file_path) -> VARCHAR      -- Source for function by name
ast_source_context(nodes, node_id, file_path, before, after) -> VARCHAR  -- Source with context
```

### 7. Chain Methods (when using ast() entrypoint)
```sql
.get_type(types)        -> continues chain
.get_names()           -> continues chain  
.filter_pattern(pattern) -> continues chain
.count()               -> INTEGER (terminates)
.first()               -> JSON (terminates)
.extract_names()       -> VARCHAR[] (terminates)
```

## Language-Specific Extensions

### Python-Specific
```sql
ast_get_imports(nodes) -> JSON           -- Import statements
ast_get_decorators(nodes) -> JSON        -- Function/class decorators  
ast_get_docstrings(nodes) -> JSON        -- Docstring contents
```

### JavaScript-Specific (future)
```sql
ast_get_exports(nodes) -> JSON           -- Export statements
ast_get_requires(nodes) -> JSON          -- Require statements
```

## Removed Functions

### Redundant Functions (74 -> 20)
- All structure-preserving variants (`ast_filter_type` returning structs)
- All "safe" variants (build null safety into main functions)
- Overlapping name extractors (consolidate into `ast_get_names`)
- Multiple depth functions (consolidate into `ast_get_depth` + `ast_filter_depth_range`)
- Arbitrary level functions (`ast_top_level`, `ast_mid_level`, `ast_deep_nodes`)
- Utility grouping functions (`ast_all_functions`, `ast_control_flow`) - use `ast_get_type` instead

### Broken Functions
- `ast_strings()` - strings don't have content field in current implementation

## Migration Guide

### Old -> New Mapping
```sql
-- Finding nodes
ast_find_type(nodes, 'function') -> ast_get_type(nodes, 'function')
ast_at_depth(nodes, 2) -> ast_get_depth(nodes, 2)
ast_at_line(nodes, 10) -> ast_get_line(nodes, 10)

-- Getting names  
ast_function_names(nodes) -> ast_get_names(nodes, 'function_definition')
ast_class_names(nodes) -> ast_get_names(nodes, 'class_definition')
ast_identifiers(nodes) -> ast_get_names(nodes, 'identifier')

-- Navigation
ast_children_of(nodes, 0) -> ast_nav_children(nodes, 0)
ast_parent_of(nodes, 42) -> ast_nav_parent(nodes, 42)

-- Analysis
ast_summary(nodes) -> ast_analyze_summary(nodes)
ast_type_counts(nodes) -> ast_analyze_types(nodes)

-- Chain style (with ast() entrypoint)
ast(nodes).find_type('function') -> ast(nodes).get_type('function')
ast(nodes).function_names() -> ast(nodes).get_names('function_definition')
```

## Implementation Plan

### Phase 1: Core Functions
1. Implement the 20 core functions with new naming
2. Update `ast()` entrypoint to support new chain methods
3. Add proper null safety to all functions

### Phase 2: Language Features  
1. Implement Python-specific extractors (imports, decorators, docstrings)
2. Fix string extraction (find actual string content representation)

### Phase 3: Enhanced Features
1. Add regex support to `ast_filter_pattern`
2. Implement better help system with examples
3. Add query suggestion system

### Phase 4: Cleanup
1. Remove old functions (keep aliases temporarily for backwards compatibility)
2. Update all tests to use new API
3. Update documentation

## Examples of Clean API Usage

```sql
-- Find all function names
SELECT ast_get_names(nodes, 'function_definition') 
FROM read_ast_objects('script.py', 'python');

-- Get functions at top level with their source code
SELECT 
    json_extract_string(func.value, '$.name') as name,
    ast_source_node(nodes, json_extract(func.value, '$.id'), 'script.py') as source
FROM read_ast_objects('script.py', 'python') ast,
     json_each(ast_filter_depth_range(ast_get_type(nodes, 'function_definition'), 0, 2)) func;

-- Chain style: find deeply nested functions
SELECT ast(nodes).get_type('function_definition').filter_depth_range(3, 5).count()
FROM read_ast_objects('script.py', 'python');

-- Analysis
SELECT ast_analyze_summary(nodes)
FROM read_ast_objects('script.py', 'python');
```

This design reduces complexity while maintaining all functionality through composition of simpler, well-named functions.