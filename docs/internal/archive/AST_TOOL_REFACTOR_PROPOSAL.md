# AST Tool Refactoring Proposal

## Overview
This document proposes a comprehensive refactoring of the `./ast` tool to:
1. Update commands to use new semantic type functions
2. Extract reusable functions as DuckDB SQL functions/macros
3. Replace per-language Parquet indexes with a unified DuckDB database

## 1. Update Commands to Use New Semantic Type Functions

### Current Issues
- Hard-coded semantic type values (e.g., `semantic_type = 112`)
- Complex filtering logic repeated throughout
- Inconsistent use of semantic type checks

### Proposed Changes

#### Replace Hard-coded Values
```sql
-- OLD:
WHERE i.type = 'function_declarator' AND i.semantic_type = 112

-- NEW:
WHERE i.semantic_type = semantic_type_code('DEFINITION_FUNCTION')
-- OR using predicate:
WHERE is_definition(i.semantic_type) AND is_function_type(i.type)
```

#### Use Semantic Type Predicates
```sql
-- Finding definitions:
WHERE is_definition(semantic_type)

-- Finding calls:
WHERE is_call(semantic_type)

-- Finding control flow:
WHERE is_control_flow(semantic_type)

-- Finding identifiers:
WHERE semantic_type = semantic_type_code('NAME_IDENTIFIER')
```

#### Leverage get_searchable_types()
```sql
-- For building indexes of searchable items:
WHERE semantic_type = ANY(get_searchable_types())
```

## 2. Extract Reusable Functions

### Proposed SQL Macros/Functions

#### ast_get_function_name(file_path, node_id) → VARCHAR
Extracts function name from a function node.
```sql
CREATE MACRO ast_get_function_name(file_path, node_id) AS (
    SELECT MAX(CASE WHEN c.type IN ('identifier','qualified_identifier') THEN c.name END)
    FROM ast_nodes c
    WHERE c.parent_id = node_id AND c.file_path = file_path
);
```

#### ast_find_functions(pattern, language) → TABLE
Returns all functions matching a pattern.
```sql
CREATE MACRO ast_find_functions(pattern, language) AS TABLE (
    SELECT f.file_path, 
           ast_get_function_name(f.file_path, f.node_id) as name,
           f.start_line, 
           f.end_line,
           f.descendant_count as complexity
    FROM ast_nodes f
    WHERE is_definition(f.semantic_type)
      AND f.semantic_type = semantic_type_code('DEFINITION_FUNCTION')
      AND (language IS NULL OR f.language = language)
      AND ast_get_function_name(f.file_path, f.node_id) LIKE pattern
);
```

#### ast_get_calls(file_path, start_line, end_line) → TABLE
Returns all function calls within a range.
```sql
CREATE MACRO ast_get_calls(file_path, start_line, end_line) AS TABLE (
    SELECT name as function_name, 
           start_line as call_line,
           semantic_type_to_string(semantic_type) as call_type
    FROM ast_nodes
    WHERE file_path = file_path
      AND start_line >= start_line
      AND end_line <= end_line
      AND is_call(semantic_type)
);
```

#### ast_get_dependencies(file_path) → TABLE
Extracts all dependencies from a file.
```sql
CREATE MACRO ast_get_dependencies(file_path) AS TABLE (
    SELECT CASE
        WHEN semantic_type = semantic_type_code('EXTERNAL_IMPORT') THEN
            REGEXP_EXTRACT(peek, 'import\\s+(\\S+)', 1)
        WHEN semantic_type = semantic_type_code('EXTERNAL_INCLUDE') THEN
            REGEXP_EXTRACT(peek, '#include\\s*[<"]([^>"]+)[>"]', 1)
        ELSE name
    END as dependency,
    semantic_type_to_string(semantic_type) as import_type,
    start_line
    FROM ast_nodes
    WHERE file_path = file_path
      AND semantic_type IN (
          semantic_type_code('EXTERNAL_IMPORT'),
          semantic_type_code('EXTERNAL_INCLUDE')
      )
);
```

#### ast_complexity_score(node) → INTEGER
Calculates complexity score for a node.
```sql
CREATE MACRO ast_complexity_score(descendant_count, depth) AS (
    descendant_count + (depth * 10)
);
```

#### ast_find_hotspots(threshold) → TABLE
Identifies code hotspots.
```sql
CREATE MACRO ast_find_hotspots(threshold) AS TABLE (
    WITH file_metrics AS (
        SELECT file_path,
               COUNT(*) FILTER (WHERE is_definition(semantic_type)) as definitions,
               MAX(descendant_count) as max_complexity,
               AVG(descendant_count) as avg_complexity,
               COUNT(*) FILTER (WHERE semantic_type IN (
                   semantic_type_code('EXTERNAL_IMPORT'),
                   semantic_type_code('EXTERNAL_INCLUDE')
               )) as dependencies
        FROM ast_nodes
        GROUP BY file_path
    )
    SELECT file_path,
           definitions,
           max_complexity,
           dependencies,
           (max_complexity + dependencies * 10) as hotspot_score
    FROM file_metrics
    WHERE max_complexity >= threshold OR dependencies > 20
    ORDER BY hotspot_score DESC
);
```

## 3. Replace Parquet Indexes with DuckDB Database

### Current Architecture Problems
- Multiple `.index-*.parquet` files per language
- No ability to update individual files
- No git integration or version tracking
- Redundant storage of the same file in multiple indexes

### Proposed Database Schema

```sql
-- Main AST nodes table
CREATE TABLE ast_nodes (
    -- Identity
    node_id BIGINT,
    file_path VARCHAR,
    language VARCHAR,
    
    -- Node properties
    type VARCHAR,
    name VARCHAR,
    semantic_type UTINYINT,
    universal_flags UTINYINT,
    arity_bin UTINYINT,
    
    -- Position
    start_line INTEGER,
    start_column INTEGER,
    end_line INTEGER,
    end_column INTEGER,
    
    -- Tree structure
    parent_id BIGINT,
    depth INTEGER,
    sibling_index INTEGER,
    children_count INTEGER,
    descendant_count INTEGER,
    
    -- Content
    peek VARCHAR,
    
    -- Metadata
    indexed_at TIMESTAMP DEFAULT current_timestamp,
    git_commit_sha VARCHAR,
    
    PRIMARY KEY (file_path, node_id)
);

-- File metadata table
CREATE TABLE ast_files (
    file_path VARCHAR PRIMARY KEY,
    language VARCHAR,
    last_modified TIMESTAMP,
    git_commit_sha VARCHAR,
    total_nodes INTEGER,
    max_depth INTEGER,
    indexed_at TIMESTAMP DEFAULT current_timestamp
);

-- Git tracking table (optional)
CREATE TABLE ast_git_history (
    file_path VARCHAR,
    git_commit_sha VARCHAR,
    commit_date TIMESTAMP,
    author VARCHAR,
    message VARCHAR,
    nodes_added INTEGER,
    nodes_removed INTEGER,
    nodes_modified INTEGER,
    PRIMARY KEY (file_path, git_commit_sha)
);

-- Indexes for performance
CREATE INDEX idx_ast_nodes_semantic_type ON ast_nodes(semantic_type);
CREATE INDEX idx_ast_nodes_name ON ast_nodes(name);
CREATE INDEX idx_ast_nodes_type ON ast_nodes(type);
CREATE INDEX idx_ast_nodes_file_language ON ast_nodes(file_path, language);
```

### New Commands Structure

#### Initialize Database
```bash
ast init [db_path]  # Creates ast_index.duckdb
```

#### Index Management
```bash
# Index files (replaces current index command)
ast index <pattern...> [--language=auto] [--update]

# Update specific files
ast update <file_path...>

# Remove files from index
ast remove <file_path...>

# Show index status
ast status [file_pattern]
```

#### Git Integration
```bash
# Track current git state
ast git-sync

# Show changes since last index
ast git-diff

# Index only changed files
ast git-update
```

### Implementation Benefits

1. **Incremental Updates**: Can update individual files without rebuilding entire index
2. **Version Tracking**: Track changes across git commits
3. **Unified Storage**: Single database instead of multiple parquet files
4. **Better Performance**: DuckDB indexes and query optimization
5. **Richer Queries**: Can join across languages and track history

### Migration Path

1. Create new database schema
2. Import existing parquet indexes into database
3. Update all commands to use database instead of parquet files
4. Add new incremental update functionality
5. Add git integration features

## Implementation Priority

### Phase 1: Update to New Semantic Functions
- Replace all hard-coded semantic type values
- Use new predicate functions
- Test all existing functionality

### Phase 2: Extract Reusable Functions
- Create SQL macros for common patterns
- Refactor commands to use macros
- Add to extension as built-in functions

### Phase 3: Database Migration
- Implement database schema
- Create migration tool from parquet
- Update commands one by one
- Add new features (git integration, incremental updates)

## Example Refactored Commands

### Find Function (Updated)
```sql
-- Using new semantic types and extracted macro
SELECT * FROM ast_find_functions('%${function_name}%', '${language}')
ORDER BY complexity DESC
LIMIT 20;
```

### Hotspots (Updated)
```sql
-- Using extracted macro
SELECT * FROM ast_find_hotspots(${threshold})
WHERE risk_level IN ('CRITICAL', 'HIGH');
```

### Git-aware Search
```sql
-- Find functions changed in last commit
WITH changed_files AS (
    SELECT DISTINCT file_path 
    FROM ast_git_history 
    WHERE git_commit_sha = (SELECT MAX(git_commit_sha) FROM ast_git_history)
)
SELECT n.file_path, 
       ast_get_function_name(n.file_path, n.node_id) as function_name,
       'MODIFIED' as change_type
FROM ast_nodes n
JOIN changed_files c ON n.file_path = c.file_path
WHERE is_definition(n.semantic_type);
```

## Conclusion

This refactoring will:
1. Make the tool more maintainable by using semantic types consistently
2. Reduce code duplication through reusable SQL functions
3. Enable powerful new features through database storage
4. Improve performance for large codebases
5. Add version control awareness

The phased approach allows incremental implementation while maintaining backward compatibility.