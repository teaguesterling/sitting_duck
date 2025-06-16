# AST Tool Improvements Summary

## Overview
The refactored AST tool provides three major improvements:
1. **Semantic Type Integration** - Uses the new semantic type functions
2. **Reusable SQL Functions** - Extracts common patterns as DuckDB macros
3. **Unified Database Storage** - Single DuckDB file instead of per-language parquet files

## 1. Semantic Type Integration

### Before (Hard-coded values)
```sql
WHERE i.type = 'function_declarator' AND i.semantic_type = 112
WHERE semantic_type = -20  -- What does this mean?
```

### After (Semantic functions)
```sql
WHERE semantic_type = semantic_type_code('DEFINITION_FUNCTION')
WHERE is_definition(semantic_type)
WHERE is_call(semantic_type)
WHERE semantic_type = ANY(get_searchable_types())
```

### Benefits
- Self-documenting code
- No magic numbers
- Consistent with the rest of the extension
- Easier to maintain and extend

## 2. Reusable SQL Functions

### Extracted Macros

#### ast_get_function_name(table, file_path, node_id)
Gets the name of a function from its declarator node.

#### ast_find_functions(pattern)
Finds all functions matching a pattern.

#### ast_find_searchable(pattern)
Finds all searchable items (functions, classes, etc.) using `get_searchable_types()`.

#### ast_get_dependencies(file_path)
Extracts all imports/includes from a file.

#### ast_find_hotspots(threshold)
Identifies complex code hotspots.

### Benefits
- DRY principle - no repeated SQL code
- Can be used in custom queries
- Consistent results across commands
- Easy to test and maintain

## 3. Unified Database Storage

### Before (Parquet files)
```
.index-cpp.parquet
.index-python.parquet
.index-javascript.parquet
```
- No incremental updates
- Redundant storage
- No metadata tracking

### After (DuckDB database)
```
ast_index.duckdb
```
- Single file for all languages
- Incremental updates: `ast update file.cpp`
- Git commit tracking
- Rich metadata and indexes

### Database Schema
```sql
CREATE TABLE ast_index (
    -- All columns from read_ast()
    node_id, file_path, type, name, semantic_type, ...
    
    -- Additional metadata
    indexed_at TIMESTAMP,
    git_commit VARCHAR
);
```

### New Capabilities
1. **Incremental Updates** - Update single files without full reindex
2. **Git Integration** - Track which commit each file was indexed at
3. **Cross-Language Queries** - Search across all languages in one query
4. **Performance** - DuckDB indexes for fast queries
5. **Persistence** - Maintain history and metadata

## Usage Examples

### Initialize and Index
```bash
# One-time setup
ast init

# Index entire codebase
ast index "src/**/*.cpp" "**/*.py" "**/*.js"

# Update specific files
ast update src/main.cpp src/utils.cpp
```

### Search with Semantic Types
```bash
# Find all functions
ast find ParseToAST

# Search all searchable items
ast search "semantic_type"

# Find unused functions
ast unused

# Find hotspots
ast hotspots 300
```

### Git Integration
```bash
# See which files need reindexing
ast git-changed

# Update only changed files
ast update $(git diff --name-only HEAD~1)
```

## Migration Path

Since indexes are fast to recreate, migration is simple:

1. Run `ast init` to create the database
2. Run `ast index` with your file patterns
3. Delete old `.index-*.parquet` files
4. Update scripts to use new commands

## Performance Comparison

### Old System
- Full reindex for any change: ~30s for large codebase
- No way to update single file
- Multiple file reads for cross-language search

### New System
- Initial index: ~30s (same)
- Update single file: <1s
- Cross-language search: Single query
- Rich filtering with indexes

## Future Enhancements

1. **Auto-update on save** - File watcher integration
2. **Diff analysis** - Show what changed between commits
3. **Team sharing** - Shared database with merge capabilities
4. **IDE integration** - Real-time code intelligence
5. **Historical analysis** - Track code evolution over time