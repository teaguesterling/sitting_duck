-- Migration script from Parquet indexes to DuckDB database
-- Run with: duckdb ast_index.duckdb < migrate_ast_indexes.sql

LOAD duckdb_ast;

-- Create schema if not exists
CREATE TABLE IF NOT EXISTS ast_nodes (
    node_id BIGINT,
    file_path VARCHAR,
    language VARCHAR,
    type VARCHAR,
    name VARCHAR,
    semantic_type UTINYINT,
    universal_flags UTINYINT,
    arity_bin UTINYINT,
    start_line INTEGER,
    start_column INTEGER,
    end_line INTEGER,
    end_column INTEGER,
    parent_id BIGINT,
    depth INTEGER,
    sibling_index INTEGER,
    children_count INTEGER,
    descendant_count INTEGER,
    peek VARCHAR,
    indexed_at TIMESTAMP DEFAULT current_timestamp,
    git_commit_sha VARCHAR DEFAULT 'legacy',
    PRIMARY KEY (file_path, node_id)
);

CREATE TABLE IF NOT EXISTS ast_files (
    file_path VARCHAR PRIMARY KEY,
    language VARCHAR,
    last_modified TIMESTAMP DEFAULT current_timestamp,
    git_commit_sha VARCHAR DEFAULT 'legacy',
    total_nodes INTEGER,
    max_depth INTEGER,
    indexed_at TIMESTAMP DEFAULT current_timestamp
);

-- Migrate from each parquet index
CREATE TEMPORARY TABLE migration_status (
    index_file VARCHAR,
    language VARCHAR,
    files_migrated INTEGER,
    nodes_migrated INTEGER,
    status VARCHAR
);

-- Function to migrate a single parquet index
CREATE OR REPLACE MACRO migrate_parquet_index(index_file VARCHAR, language VARCHAR) AS (
    INSERT INTO migration_status
    SELECT 
        index_file,
        language,
        COUNT(DISTINCT file_path) as files_migrated,
        COUNT(*) as nodes_migrated,
        'completed' as status
    FROM (
        INSERT INTO ast_nodes
        SELECT 
            node_id,
            file_path,
            language,
            type,
            name,
            -- Handle signed semantic_type from old indexes
            CASE 
                WHEN semantic_type < 0 THEN (256 + semantic_type)::UTINYINT
                ELSE semantic_type::UTINYINT
            END as semantic_type,
            0 as universal_flags,  -- Not available in old format
            0 as arity_bin,        -- Not available in old format
            start_line,
            start_column,
            end_line,
            end_column,
            parent_id,
            depth,
            sibling_index,
            children_count,
            descendant_count,
            peek,
            current_timestamp,
            'legacy'
        FROM read_parquet(index_file)
        WHERE NOT EXISTS (
            SELECT 1 FROM ast_nodes existing 
            WHERE existing.file_path = read_parquet.file_path
        )
        RETURNING *
    ) migrated
);

-- Migrate all existing indexes
INSERT INTO migration_status
SELECT 
    filename,
    REGEXP_EXTRACT(filename, '\.index-(.+)\.parquet', 1) as language,
    0, 0, 'pending'
FROM (
    SELECT DISTINCT filename 
    FROM read_parquet('.index-*.parquet', filename=true)
);

-- Process each index
UPDATE migration_status
SET (files_migrated, nodes_migrated, status) = (
    SELECT COUNT(DISTINCT file_path), COUNT(*), 'completed'
    FROM (
        INSERT INTO ast_nodes
        SELECT 
            p.node_id,
            p.file_path,
            ms.language,
            p.type,
            p.name,
            CASE 
                WHEN p.semantic_type < 0 THEN (256 + p.semantic_type)::UTINYINT
                ELSE p.semantic_type::UTINYINT
            END as semantic_type,
            0 as universal_flags,
            0 as arity_bin,
            p.start_line,
            p.start_column,
            p.end_line,
            p.end_column,
            p.parent_id,
            p.depth,
            p.sibling_index,
            p.children_count,
            p.descendant_count,
            p.peek,
            current_timestamp,
            'legacy'
        FROM migration_status ms
        JOIN read_parquet(ms.index_file) p ON 1=1
        WHERE ms.status = 'pending'
        ON CONFLICT DO NOTHING
        RETURNING *
    ) migrated
)
WHERE status = 'pending';

-- Update file metadata
INSERT OR REPLACE INTO ast_files
SELECT 
    file_path,
    language,
    current_timestamp,
    'legacy',
    COUNT(*) as total_nodes,
    MAX(depth) as max_depth,
    current_timestamp
FROM ast_nodes
GROUP BY file_path, language;

-- Create indexes
CREATE INDEX IF NOT EXISTS idx_semantic_type ON ast_nodes(semantic_type);
CREATE INDEX IF NOT EXISTS idx_name ON ast_nodes(name);
CREATE INDEX IF NOT EXISTS idx_file_type ON ast_nodes(file_path, semantic_type);
CREATE INDEX IF NOT EXISTS idx_type ON ast_nodes(type);

-- Report migration results
SELECT 
    '=== Migration Summary ===' as report
UNION ALL
SELECT 
    'Language: ' || language || 
    ' | Files: ' || SUM(files_migrated) || 
    ' | Nodes: ' || SUM(nodes_migrated) ||
    ' | Status: ' || MAX(status)
FROM migration_status
GROUP BY language
UNION ALL
SELECT '=== Total ===' 
UNION ALL
SELECT 
    'Files: ' || COUNT(DISTINCT file_path) || 
    ' | Nodes: ' || COUNT(*) ||
    ' | Languages: ' || COUNT(DISTINCT language)
FROM ast_nodes;

-- Analyze and optimize
ANALYZE ast_nodes;
ANALYZE ast_files;