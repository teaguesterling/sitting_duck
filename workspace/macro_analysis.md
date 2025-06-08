# AST Macro Analysis & Conventions

## Our 4 Naming Conventions

### 1. **Internal Helpers** (`_ast_internal_*`)
- **Purpose**: Private utility functions, not part of public API
- **Pattern**: `_ast_internal_<function_name>`
- **Usage**: Called by other macros, not by users directly

### 2. **Standalone Functions** (`ast_*`) 
- **Purpose**: Full-featured standalone functions
- **Pattern**: `ast_<verb>_<object>` or `ast_<action>`
- **Usage**: Direct calls, can be used independently

### 3. **Entrypoint Function** (`ast()`)
- **Purpose**: Single normalized entry point for method chaining
- **Pattern**: Just `ast(input)`
- **Usage**: `ast(nodes).get_type('function')`

### 4. **Chain Methods** (unprefixed)
- **Purpose**: Chainable methods that work with `ast()` output
- **Pattern**: `<verb>` or `<verb>_<object>`
- **Usage**: Used after `ast()`: `ast(nodes).get_type('function')`

## Current Macro Inventory

### Internal Helpers (`_ast_internal_*`)
```sql
_ast_internal_ensure_varchar_array(val)
_ast_internal_ensure_integer_array(val)
```

### Standalone Functions (`ast_*`)

#### Core Extraction
```sql
ast_get_type(nodes, types)           # Find nodes by type(s)
ast_to_names(nodes, node_type)       # Extract names, optionally by type
ast_get_names(nodes, node_type)      # Alias for ast_to_names
ast_get_depth(nodes, depths)         # Find nodes at specific depth(s)
```

#### Filtering  
```sql
ast_filter_pattern(nodes, pattern, field)  # Filter by pattern (LIKE)
ast_filter_has_name(nodes)                 # Filter to named nodes only
```

#### Navigation
```sql
ast_nav_children(nodes, parent_id)   # Get direct children
ast_nav_parent(nodes, child_id)      # Get parent node
```

#### Analysis
```sql
ast_summary(nodes)                   # Get summary statistics
```

#### Advanced Features (from recent files)
```sql
ast_get_locations(nodes)
ast_get_calls(nodes, root_node_id)
ast_get_parent_chain(nodes, target_node_id, max_depth)
ast_extract_subtree_by_id(nodes, node_id)
ast_extract_subtree_by_index(nodes, start_index, count)
```

### Entrypoint Function
```sql
ast(input)                          # Normalize input for chaining
```

### Chain Methods (unprefixed)

#### Core Methods
```sql
get_type(nodes, types)              # = ast_get_type
get_names(nodes, node_type)         # = ast_get_names  
get_depth(nodes, depths)            # = ast_get_depth
```

#### Filtering Methods
```sql
filter_pattern(nodes, pattern, field)  # = ast_filter_pattern
filter_has_name(nodes)                 # = ast_filter_has_name
```

#### Navigation Methods
```sql
nav_children(nodes, parent_id)      # = ast_nav_children
nav_parent(nodes, child_id)         # = ast_nav_parent
children(nodes, parent_id)          # Alias for nav_children
parent(nodes, child_id)             # Alias for nav_parent
```

#### Analysis Methods
```sql
summary(nodes)                      # = ast_summary
```

#### Utility Methods
```sql
count_nodes(nodes)                  # = json_array_length(nodes)
first_node(nodes)                   # Get first node
last_node(nodes)                    # Get last node
len(nodes)                          # = json_array_length(nodes)
size(nodes)                         # = json_array_length(nodes)
```

#### Alternative Names
```sql
find_type(nodes, types)             # = get_type
find_depth(nodes, depths)           # = get_depth
extract_names(nodes, node_type)     # = get_names
```

#### Advanced Chain Methods
```sql
get_locations(nodes)
get_calls(nodes, root_node_id)
get_parent_chain(nodes, target_node_id, max_depth)
```

## Issues with New Macros

Our new struct-based macros broke these conventions:

1. **Wrong API Pattern**: Created `ast_load()`, `ast_nodes()` instead of working with existing `ast()` entrypoint
2. **Inconsistent Naming**: Used patterns like `ast_functions()` instead of `ast_get_functions()`
3. **No Chain Methods**: Didn't create unprefixed versions for chaining
4. **Different Input**: Expected AST struct instead of nodes array

## Correct Approach

We should:
1. **Keep `ast()` entrypoint** - it should work with new struct format
2. **Add `ast_*` functions** following existing verb/object patterns
3. **Create unprefixed chain methods** for each new function
4. **Maintain input/output consistency** with existing API

## Action Plan

1. Update `ast()` to handle new struct format
2. Create proper `ast_get_*` functions that work with nodes arrays
3. Add corresponding unprefixed chain methods
4. Ensure all functions follow established input/output patterns