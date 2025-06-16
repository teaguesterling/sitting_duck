#!/bin/bash
# AST Tool v2 - Refactored with semantic types and DuckDB storage
# This is a proof-of-concept showing how the refactored tool would work

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DB_PATH="${AST_DB:-$SCRIPT_DIR/../ast_index.duckdb}"
DUCKDB="$SCRIPT_DIR/../build/release/duckdb"
EXTENSION="$SCRIPT_DIR/../build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension"

# Initialize database with schema and helper functions
init_db() {
    $DUCKDB "$DB_PATH" << 'EOF'
INSTALL duckdb_ast FROM '${EXTENSION}';
LOAD duckdb_ast;

-- Main AST nodes table
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
    git_commit_sha VARCHAR,
    PRIMARY KEY (file_path, node_id)
);

-- File metadata
CREATE TABLE IF NOT EXISTS ast_files (
    file_path VARCHAR PRIMARY KEY,
    language VARCHAR,
    last_modified TIMESTAMP,
    git_commit_sha VARCHAR,
    total_nodes INTEGER,
    max_depth INTEGER,
    indexed_at TIMESTAMP DEFAULT current_timestamp
);

-- Create indexes
CREATE INDEX IF NOT EXISTS idx_semantic_type ON ast_nodes(semantic_type);
CREATE INDEX IF NOT EXISTS idx_name ON ast_nodes(name);
CREATE INDEX IF NOT EXISTS idx_file_type ON ast_nodes(file_path, semantic_type);

-- Helper function to get function name
CREATE OR REPLACE MACRO ast_get_function_name(file_path, node_id) AS (
    SELECT MAX(CASE WHEN type IN ('identifier','qualified_identifier') THEN name END)
    FROM ast_nodes
    WHERE parent_id = node_id AND file_path = file_path
);

-- Find functions by pattern
CREATE OR REPLACE MACRO ast_find_functions(pattern, lang) AS TABLE (
    SELECT DISTINCT
        f.file_path,
        ast_get_function_name(f.file_path, f.node_id) as function_name,
        f.start_line,
        f.end_line,
        f.descendant_count as complexity,
        semantic_type_to_string(f.semantic_type) as semantic_type
    FROM ast_nodes f
    WHERE f.semantic_type = semantic_type_code('DEFINITION_FUNCTION')
      AND (lang IS NULL OR f.language = lang)
      AND (pattern IS NULL OR ast_get_function_name(f.file_path, f.node_id) LIKE pattern)
);

-- Find all definitions
CREATE OR REPLACE MACRO ast_find_definitions(pattern, lang) AS TABLE (
    SELECT DISTINCT
        f.file_path,
        COALESCE(f.name, ast_get_function_name(f.file_path, f.node_id)) as name,
        f.start_line,
        f.end_line,
        f.descendant_count as complexity,
        semantic_type_to_string(f.semantic_type) as definition_type
    FROM ast_nodes f
    WHERE is_definition(f.semantic_type)
      AND (lang IS NULL OR f.language = lang)
      AND (pattern IS NULL OR COALESCE(f.name, ast_get_function_name(f.file_path, f.node_id)) LIKE pattern)
);

-- Find hotspots
CREATE OR REPLACE MACRO ast_find_hotspots(threshold) AS TABLE (
    WITH file_metrics AS (
        SELECT 
            file_path,
            COUNT(*) FILTER (WHERE is_definition(semantic_type)) as definitions,
            MAX(descendant_count) as max_complexity,
            ROUND(AVG(descendant_count), 1) as avg_complexity,
            COUNT(*) FILTER (WHERE semantic_type IN (
                semantic_type_code('EXTERNAL_IMPORT'),
                semantic_type_code('EXTERNAL_INCLUDE')
            )) as dependencies
        FROM ast_nodes
        GROUP BY file_path
    )
    SELECT 
        file_path,
        definitions,
        max_complexity,
        avg_complexity,
        dependencies,
        (max_complexity + dependencies * 10) as hotspot_score,
        CASE 
            WHEN (max_complexity + dependencies * 10) > 500 THEN 'CRITICAL'
            WHEN (max_complexity + dependencies * 10) > 300 THEN 'HIGH'
            WHEN (max_complexity + dependencies * 10) > 150 THEN 'MEDIUM'
            ELSE 'LOW'
        END as risk_level
    FROM file_metrics
    WHERE max_complexity >= threshold OR dependencies > 20
    ORDER BY hotspot_score DESC
);

-- Get file dependencies
CREATE OR REPLACE MACRO ast_get_dependencies(file) AS TABLE (
    SELECT DISTINCT
        CASE
            WHEN semantic_type = semantic_type_code('EXTERNAL_IMPORT') THEN
                COALESCE(
                    REGEXP_EXTRACT(peek, 'import\s+(\S+)', 1),
                    REGEXP_EXTRACT(peek, 'from\s+(\S+)', 1),
                    name
                )
            WHEN semantic_type = semantic_type_code('EXTERNAL_INCLUDE') THEN
                REGEXP_EXTRACT(peek, '#include\s*[<"]([^>"]+)[>"]', 1)
            ELSE name
        END as dependency,
        semantic_type_to_string(semantic_type) as import_type,
        start_line
    FROM ast_nodes
    WHERE file_path = file
      AND semantic_type IN (
          semantic_type_code('EXTERNAL_IMPORT'),
          semantic_type_code('EXTERNAL_INCLUDE')
      )
      AND peek IS NOT NULL
);

.exit
EOF
    echo "Database initialized at: $DB_PATH"
}

# Index files into database
index_files() {
    local patterns=("$@")
    
    if [[ ${#patterns[@]} -eq 0 ]]; then
        echo "Usage: ast index <pattern1> [pattern2] ..."
        exit 1
    fi
    
    echo "Indexing files..."
    
    # Build query to index files
    local union_query=""
    for pattern in "${patterns[@]}"; do
        if [[ -z "$union_query" ]]; then
            union_query="SELECT *, '$pattern' as source_pattern FROM read_ast_objects('$pattern')"
        else
            union_query="$union_query UNION ALL SELECT *, '$pattern' as source_pattern FROM read_ast_objects('$pattern')"
        fi
    done
    
    $DUCKDB "$DB_PATH" << EOF
LOAD duckdb_ast;

-- Get current git commit (if in git repo)
CREATE TEMPORARY MACRO get_git_commit() AS (
    CASE 
        WHEN current_setting('git_commit') IS NOT NULL THEN current_setting('git_commit')
        ELSE 'unknown'
    END
);

-- Clear existing data for these files
DELETE FROM ast_nodes WHERE file_path IN (
    SELECT DISTINCT (ast).source.file_path
    FROM ($union_query) t
);

-- Insert new nodes
INSERT INTO ast_nodes
SELECT 
    node.node_id,
    (ast).source.file_path,
    (ast).source.language,
    node.type,
    node.name,
    node.semantic_type,
    node.universal_flags,
    node.arity_bin,
    node.start_line,
    node.start_column,
    node.end_line,
    node.end_column,
    node.parent_id,
    node.depth,
    node.sibling_index,
    node.children_count,
    node.descendant_count,
    node.peek,
    current_timestamp,
    get_git_commit()
FROM (
    SELECT ast FROM ($union_query) t
) ast_data,
LATERAL (
    SELECT unnest((ast).nodes) as node
) nodes;

-- Update file metadata
INSERT OR REPLACE INTO ast_files
SELECT 
    file_path,
    language,
    current_timestamp,
    get_git_commit(),
    COUNT(*) as total_nodes,
    MAX(depth) as max_depth,
    current_timestamp
FROM ast_nodes
GROUP BY file_path, language;

SELECT COUNT(DISTINCT file_path) as files_indexed,
       COUNT(*) as total_nodes
FROM ast_nodes;
EOF
}

# Update specific files
update_files() {
    local files=("$@")
    
    if [[ ${#files[@]} -eq 0 ]]; then
        echo "Usage: ast update <file1> [file2] ..."
        exit 1
    fi
    
    echo "Updating ${#files[@]} file(s)..."
    
    for file in "${files[@]}"; do
        $DUCKDB "$DB_PATH" << EOF
LOAD duckdb_ast;

-- Delete existing nodes for this file
DELETE FROM ast_nodes WHERE file_path = '$file';

-- Re-index the file
INSERT INTO ast_nodes
SELECT 
    node.node_id,
    ast.source.file_path,
    ast.source.language,
    node.type,
    node.name,
    node.semantic_type,
    node.universal_flags,
    node.arity_bin,
    node.start_line,
    node.start_column,
    node.end_line,
    node.end_column,
    node.parent_id,
    node.depth,
    node.sibling_index,
    node.children_count,
    node.descendant_count,
    node.peek,
    current_timestamp,
    'update'
FROM (
    SELECT read_ast_objects('$file') as ast
) ast_data,
LATERAL (
    SELECT unnest(ast.nodes) as node
) nodes;

-- Update file metadata
UPDATE ast_files
SET last_modified = current_timestamp,
    total_nodes = (SELECT COUNT(*) FROM ast_nodes WHERE file_path = '$file'),
    max_depth = (SELECT MAX(depth) FROM ast_nodes WHERE file_path = '$file')
WHERE file_path = '$file';
EOF
        echo "Updated: $file"
    done
}

# Find functions using semantic types
find_functions() {
    local pattern="${1:-%}"
    local lang="$2"
    
    $DUCKDB "$DB_PATH" -column << EOF
LOAD duckdb_ast;
SELECT * FROM ast_find_functions('$pattern', $([ -z "$lang" ] && echo "NULL" || echo "'$lang'"))
ORDER BY complexity DESC
LIMIT 20;
EOF
}

# Find all definitions (functions, classes, etc.)
find_definitions() {
    local pattern="${1:-%}"
    local lang="$2"
    
    $DUCKDB "$DB_PATH" -column << EOF
LOAD duckdb_ast;
SELECT * FROM ast_find_definitions('$pattern', $([ -z "$lang" ] && echo "NULL" || echo "'$lang'"))
ORDER BY definition_type, complexity DESC
LIMIT 30;
EOF
}

# Find hotspots
find_hotspots() {
    local threshold="${1:-200}"
    
    $DUCKDB "$DB_PATH" -column << EOF
LOAD duckdb_ast;
SELECT * FROM ast_find_hotspots($threshold)
LIMIT 20;
EOF
}

# Show file dependencies
show_dependencies() {
    local file="$1"
    
    if [[ -z "$file" ]]; then
        echo "Usage: ast deps <file>"
        exit 1
    fi
    
    $DUCKDB "$DB_PATH" -column << EOF
LOAD duckdb_ast;
SELECT * FROM ast_get_dependencies('$file')
ORDER BY import_type, dependency;
EOF
}

# Show database statistics
show_stats() {
    $DUCKDB "$DB_PATH" -column << EOF
LOAD duckdb_ast;

WITH stats AS (
    SELECT 
        COUNT(DISTINCT file_path) as total_files,
        COUNT(DISTINCT language) as languages,
        COUNT(*) as total_nodes,
        COUNT(*) FILTER (WHERE is_definition(semantic_type)) as definitions,
        COUNT(*) FILTER (WHERE is_call(semantic_type)) as calls,
        COUNT(*) FILTER (WHERE semantic_type = semantic_type_code('EXTERNAL_IMPORT') 
                            OR semantic_type = semantic_type_code('EXTERNAL_INCLUDE')) as imports
    FROM ast_nodes
),
language_breakdown AS (
    SELECT 
        language,
        COUNT(DISTINCT file_path) as files,
        COUNT(*) as nodes
    FROM ast_nodes
    GROUP BY language
)
SELECT 'Total Files' as metric, total_files::VARCHAR as value FROM stats
UNION ALL
SELECT 'Languages', languages::VARCHAR FROM stats
UNION ALL
SELECT 'Total Nodes', total_nodes::VARCHAR FROM stats
UNION ALL
SELECT 'Definitions', definitions::VARCHAR FROM stats
UNION ALL
SELECT 'Function Calls', calls::VARCHAR FROM stats
UNION ALL
SELECT 'Imports/Includes', imports::VARCHAR FROM stats
UNION ALL
SELECT '', '' -- separator
UNION ALL
SELECT 'Language: ' || language, files || ' files, ' || nodes || ' nodes'
FROM language_breakdown
ORDER BY 1;
EOF
}

# Quick search with semantic awareness
quick_search() {
    local term="$1"
    
    if [[ -z "$term" ]]; then
        echo "Usage: ast search <term>"
        exit 1
    fi
    
    $DUCKDB "$DB_PATH" -column << EOF
LOAD duckdb_ast;

WITH ranked_results AS (
    SELECT 
        file_path,
        name,
        semantic_type_to_string(semantic_type) as semantic_type,
        type,
        start_line,
        descendant_count,
        CASE 
            WHEN name = '$term' THEN 100
            WHEN name LIKE '$term%' THEN 90
            WHEN name LIKE '%$term' THEN 80
            WHEN name LIKE '%$term%' THEN 70
            ELSE 50
        END as relevance_score
    FROM ast_nodes
    WHERE name LIKE '%$term%'
      AND semantic_type = ANY(get_searchable_types())
)
SELECT 
    file_path,
    name,
    semantic_type,
    start_line,
    relevance_score
FROM ranked_results
ORDER BY relevance_score DESC, descendant_count DESC
LIMIT 20;
EOF
}

# Main command dispatcher
case "$1" in
    init)
        init_db
        ;;
    index)
        shift
        index_files "$@"
        ;;
    update)
        shift
        update_files "$@"
        ;;
    find)
        shift
        find_functions "$@"
        ;;
    defs|definitions)
        shift
        find_definitions "$@"
        ;;
    hotspots)
        shift
        find_hotspots "$@"
        ;;
    deps|dependencies)
        shift
        show_dependencies "$@"
        ;;
    stats)
        show_stats
        ;;
    search)
        shift
        quick_search "$@"
        ;;
    *)
        cat << 'EOF'
AST Tool v2 - Semantic-aware code analysis with DuckDB storage

USAGE:
  # Database Management
  ast init                              Initialize the AST database
  ast stats                             Show database statistics
  
  # Indexing
  ast index <pattern...>                Index files matching patterns
  ast update <file...>                  Update specific files
  
  # Search (using semantic types)
  ast find <pattern> [lang]             Find functions by name pattern
  ast defs <pattern> [lang]             Find all definitions (functions, classes, etc.)
  ast search <term>                     Quick search with relevance ranking
  
  # Analysis
  ast hotspots [threshold]              Find code hotspots
  ast deps <file>                       Show file dependencies
  
EXAMPLES:
  ast init                              # Initialize database
  ast index "src/**/*.cpp" "**/*.h"     # Index C++ files
  ast find ParseToAST cpp               # Find function in C++ files
  ast defs "^Test" python               # Find all test definitions in Python
  ast hotspots 300                      # Find complex hotspots
  ast search "semantic_type"            # Quick search for term

This version uses:
- Semantic type functions (is_definition, is_call, etc.)
- DuckDB storage with incremental updates
- SQL macros for reusable queries
EOF
        ;;
esac