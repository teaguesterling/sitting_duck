# Hybrid AST Design: Structured + JSON

## Core Insight
Separate the **source metadata** (structured) from the **AST nodes** (flexible JSON), while maintaining strong linkage.

## Design Options

### Option 1: Struct with JSON Array
```sql
CREATE TYPE ASTSource AS STRUCT(
    file_path VARCHAR,
    language VARCHAR,
    parse_time TIMESTAMP,
    file_size BIGINT,
    node_count INTEGER,
    max_depth INTEGER,
    nodes JSON  -- Array of node objects
);

-- Query example
SELECT 
    ast.file_path,
    node
FROM read_ast_objects('*.py', 'python') t,
     json_each(t.ast.nodes) as node
WHERE json_extract_string(node, '$.type') = 'function_definition';
```

### Option 2: Normalized Tables (BEST)
```sql
-- Source metadata table
CREATE TABLE ast_sources AS
SELECT 
    file_path,
    language,
    parse_time,
    file_size,
    node_count,
    max_depth,
    ast_hash  -- Unique identifier
FROM read_ast_sources('*.py', 'python');

-- Nodes as separate table with source link
CREATE TABLE ast_nodes AS
SELECT 
    ast_hash,
    node->>'$.id' as node_id,
    node->>'$.type' as type,
    node->>'$.name' as name,
    (node->>'$.start.line')::INTEGER as start_line,
    (node->>'$.parent_id')::INTEGER as parent_id,
    node as node_data  -- Full JSON for additional fields
FROM read_ast_sources('*.py', 'python') s,
     json_each(s.nodes) as node;

-- Now queries are pure SQL!
SELECT 
    s.file_path,
    n.name,
    n.start_line
FROM ast_sources s
JOIN ast_nodes n ON s.ast_hash = n.ast_hash
WHERE n.type = 'function_definition'
AND s.language = 'python';
```

### Option 3: Single Table with Repeated Metadata
```sql
-- Each row is a node with its source context
CREATE TABLE ast_nodes AS
SELECT * FROM read_ast_flat('*.py', 'python');

-- Returns:
-- file_path | language | node_id | type | name | parent_id | node_json
-- main.py   | python   | 0       | module | null | null     | {...}
-- main.py   | python   | 1       | function | foo | 0       | {...}
```

## Recommendation: Hybrid Approach

```sql
-- 1. read_ast_objects returns structured metadata + JSON nodes
CREATE OR REPLACE FUNCTION read_ast_objects(
    pattern VARCHAR, 
    language VARCHAR
) RETURNS TABLE (
    -- Structured metadata
    file_path VARCHAR,
    language VARCHAR,
    parse_time TIMESTAMP,
    node_count INTEGER,
    max_depth INTEGER,
    -- Flexible node data
    nodes JSON
);

-- 2. Macro to expand nodes while preserving file context
CREATE MACRO ast_nodes(file_path, nodes_json) AS (
    SELECT 
        file_path,
        node->>'$.id' as node_id,
        node->>'$.type' as type,
        node->>'$.name' as name,
        (node->>'$.start.line')::INTEGER as start_line,
        (node->>'$.end.line')::INTEGER as end_line,
        (node->>'$.parent_id')::INTEGER as parent_id,
        node as raw_node
    FROM json_each(nodes_json) as node
);

-- 3. Usage combines both
WITH sources AS (
    SELECT * FROM read_ast_objects('src/**/*.py', 'python')
)
SELECT 
    s.file_path,
    n.name,
    n.start_line
FROM sources s,
     LATERAL ast_nodes(s.file_path, s.nodes) n
WHERE n.type = 'function_definition';
```

## Benefits
1. **Clean separation**: Metadata vs node data
2. **Flexibility**: JSON for extensibility, structured for performance
3. **Queryable**: Standard SQL with JSON helpers
4. **Maintainable**: Each file's context preserved
5. **Scalable**: Can materialize views for performance

## Implementation Path
1. Modify `read_ast_objects` to return this structure
2. Create helper macros for common patterns
3. Document query patterns
4. Add indexes on frequently filtered columns