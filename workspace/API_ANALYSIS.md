# DuckDB AST Extension API Analysis

## A. Completeness Assessment

### What's Working Well:
- Comprehensive tree navigation (parent, children, siblings, ancestors, descendants)
- Multiple ways to filter nodes (by type, depth, line, name pattern)
- Good metadata extraction (summary, complexity, type counts)
- Source code extraction with context
- Both structure-preserving and extraction patterns

### What's Missing:
1. **Language-specific queries** (currently generic):
   - No `ast_imports()` or `ast_dependencies()` 
   - No `ast_comments()` or `ast_docstrings()`
   - No `ast_decorators()` (Python-specific)
   - No `ast_exports()` (JS-specific)

2. **Advanced filtering**:
   - No regex support (only LIKE patterns)
   - No complex predicate filtering
   - No way to combine multiple filters easily

3. **Source code operations**:
   - Can't get source for arbitrary line ranges (only nodes)
   - No way to get source with syntax highlighting
   - No diff/comparison functions

4. **Broken functionality**:
   - `ast_strings()` returns empty array (strings don't have content field)

## B. Redundancy and Confusion

### Naming Confusion:
1. **Extract vs Find vs Filter**:
   - `ast_find_type()` returns JSON array
   - `ast_filter_type()` returns struct with metadata
   - `ast_extract_names()` returns VARCHAR[]
   - Users won't know which to use when

2. **Multiple ways to get names**:
   - `ast_function_names()` - just functions
   - `ast_extract_names()` - names from specific type
   - `ast_extract_all_names()` - all names
   - `extract_names()` - chain method
   
3. **Depth functions overlap**:
   - `ast_at_depth()` - specific depth(s)
   - `ast_at_depth_range()` - depth range
   - `ast_filter_depth()` - struct version
   - `ast_filter_depth_range()` - struct version
   - `ast_top_level()`, `ast_mid_level()`, `ast_deep_nodes()` - arbitrary boundaries

### Redundant "Safe" Variants:
- `ast_safe_find_type()`, `ast_safe_function_names()`, etc.
- These just add COALESCE - could be built into main functions

### Structure-Preserving vs Extraction Pattern:
- Having two versions of many functions is conceptually heavy
- The distinction isn't immediately clear from names

## C. Discoverability Issues

### For Humans:
1. **No clear starting point** - should they use `read_ast()` or `read_ast_objects()`?
2. **Method chaining not obvious** - the `ast()` entrypoint and `(nodes).macro()` syntax
3. **Parameter flexibility hidden** - many functions accept single value or array
4. **Return types not obvious** from names

### For AI:
1. **No semantic grouping** in function names (e.g., nav_, filter_, extract_ prefixes)
2. **Examples are too simple** - don't show real-world usage patterns
3. **No query builder** or suggestion system beyond basic help
4. **Missing common task templates**

## Recommendations

### 1. Simplify Naming Convention:
```
ast_get_*     -> Simple extraction (returns JSON array)
ast_filter_*  -> Filtering (returns filtered JSON array) 
ast_extract_* -> Type conversion (returns SQL types)
ast_nav_*     -> Tree navigation
ast_analyze_* -> Analysis/metrics
```

### 2. Consolidate Redundant Functions:
- Merge all name extraction into one parameterized function
- Combine depth functions with range support
- Remove "safe" variants (make all functions null-safe)
- Pick either structure-preserving OR extraction pattern as primary

### 3. Add Missing Functionality:
- Language-aware extractors (imports, exports, decorators)
- Regex support for pattern matching
- Source code range extraction
- Query templates for common tasks

### 4. Improve Discoverability:
- Clear getting started guide
- Categorized function reference
- Rich examples showing real queries
- Interactive query builder macro
- Semantic function naming

### 5. Focus the API:
Instead of providing every possible function, focus on:
- **Core**: ast(), find_type(), at_depth(), extract_names()
- **Navigation**: children_of(), parent_of(), descendants_of()
- **Analysis**: summary(), complexity()
- **Source**: get_source(), get_context()
- Make everything else achievable through combinations