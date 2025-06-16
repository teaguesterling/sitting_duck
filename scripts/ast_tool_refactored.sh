#!/bin/bash
# AST Tool - Refactored version with semantic types and DuckDB storage
# Key improvements:
# 1. Uses semantic type functions instead of hard-coded values
# 2. Extracts reusable SQL functions as macros
# 3. Single DuckDB database instead of per-language parquet files

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DB_PATH="${AST_DB:-$PROJECT_ROOT/ast_index.duckdb}"
# Handle different mount paths
if [ -f "$PROJECT_ROOT/build/release/duckdb" ]; then
    DUCKDB="$PROJECT_ROOT/build/release/duckdb"
elif [ -f "/mnt/aux-data/teague/Projects/duckdb_ast/build/release/duckdb" ]; then
    DUCKDB="/mnt/aux-data/teague/Projects/duckdb_ast/build/release/duckdb"
else
    echo "Error: Could not find DuckDB binary"
    exit 1
fi
EXTENSION="$(dirname $DUCKDB)/extension/duckdb_ast/duckdb_ast.duckdb_extension"

# Helper to run SQL with the AST extension loaded
sql() {
    $DUCKDB "$DB_PATH" -noheader -list -s "LOAD '$EXTENSION'; $1"
}

sql_out() {
    $DUCKDB "$DB_PATH" -column -s "LOAD '$EXTENSION'; $1"
}

# Initialize database with schema and reusable functions
init() {
    echo "Initializing AST database at $DB_PATH..."
    
    $DUCKDB "$DB_PATH" << 'EOF'
-- Load extension
LOAD duckdb_ast;

-- Main AST table
CREATE TABLE IF NOT EXISTS ast_index (
    node_id BIGINT,
    type VARCHAR,
    name VARCHAR,
    file_path VARCHAR,
    language VARCHAR,
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
    semantic_type UTINYINT,
    universal_flags UTINYINT,
    arity_bin UTINYINT,
    indexed_at TIMESTAMP DEFAULT current_timestamp,
    git_commit VARCHAR
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_semantic_type ON ast_index(semantic_type);
CREATE INDEX IF NOT EXISTS idx_name ON ast_index(name);
CREATE INDEX IF NOT EXISTS idx_file_path ON ast_index(file_path);
CREATE INDEX IF NOT EXISTS idx_start_line ON ast_index(file_path, start_line);

-- Reusable function: Get function name from a function node
CREATE OR REPLACE MACRO ast_get_function_name(file, node) AS (
    SELECT MAX(CASE WHEN type IN ('identifier','qualified_identifier') THEN name END)
    FROM ast_index
    WHERE parent_id = node AND file_path = file
);

-- Reusable function: Find functions
CREATE OR REPLACE MACRO ast_find_functions(pattern) AS TABLE (
    WITH funcs AS (
        SELECT * FROM ast_index 
        WHERE semantic_type = semantic_type_code('DEFINITION_FUNCTION')
    )
    SELECT DISTINCT
        f.file_path,
        ast_get_function_name(f.file_path, f.node_id) as function_name,
        f.start_line,
        f.end_line,
        f.descendant_count as complexity
    FROM funcs f
    WHERE pattern IS NULL 
       OR ast_get_function_name(f.file_path, f.node_id) LIKE pattern
);

-- Reusable function: Find all searchable definitions
CREATE OR REPLACE MACRO ast_find_searchable(pattern) AS TABLE (
    SELECT 
        file_path,
        name,
        semantic_type_to_string(semantic_type) as type,
        start_line,
        descendant_count as complexity
    FROM ast_index
    WHERE semantic_type = ANY(get_searchable_types())
      AND name IS NOT NULL
      AND (pattern IS NULL OR name LIKE pattern)
);

-- Reusable function: Get file dependencies
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
    FROM ast_index
    WHERE file_path = file
      AND semantic_type IN (
          semantic_type_code('EXTERNAL_IMPORT'),
          semantic_type_code('EXTERNAL_INCLUDE')
      )
);

-- Reusable function: Find code hotspots
CREATE OR REPLACE MACRO ast_find_hotspots(threshold) AS TABLE (
    WITH file_metrics AS (
        SELECT 
            file_path,
            COUNT(*) FILTER (WHERE is_definition(semantic_type)) as definitions,
            MAX(descendant_count) as max_complexity,
            ROUND(AVG(descendant_count) FILTER (WHERE is_definition(semantic_type)), 1) as avg_complexity,
            COUNT(*) FILTER (WHERE semantic_type IN (
                semantic_type_code('EXTERNAL_IMPORT'),
                semantic_type_code('EXTERNAL_INCLUDE')
            )) as dependencies
        FROM ast_index
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
);

.exit
EOF
    echo "Database initialized successfully!"
}

# Index files - now stores in database instead of separate parquet files
index() {
    if [[ $# -eq 0 ]]; then
        echo "Usage: ast index <pattern1> [pattern2] ..."
        exit 1
    fi
    
    echo "Indexing files..."
    
    # Get current git commit if available
    GIT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "")
    
    # Build UNION query for multiple patterns
    UNION_QUERY=""
    for pattern in "$@"; do
        if [[ -z "$UNION_QUERY" ]]; then
            UNION_QUERY="SELECT * FROM read_ast('$pattern')"
        else
            UNION_QUERY="$UNION_QUERY UNION ALL SELECT * FROM read_ast('$pattern')"
        fi
    done
    
    # Clear existing entries for these files and insert new ones
    sql "
        DELETE FROM ast_index WHERE file_path IN (
            SELECT DISTINCT file_path FROM ($UNION_QUERY)
        );
        
        INSERT INTO ast_index
        SELECT *, current_timestamp as indexed_at, '$GIT_COMMIT' as git_commit
        FROM ($UNION_QUERY);
        
        SELECT COUNT(DISTINCT file_path) || ' files, ' || COUNT(*) || ' nodes indexed' as result;
    "
}

# Update specific files
update() {
    if [[ $# -eq 0 ]]; then
        echo "Usage: ast update <file1> [file2] ..."
        exit 1
    fi
    
    GIT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "")
    
    for file in "$@"; do
        echo "Updating $file..."
        sql "
            DELETE FROM ast_index WHERE file_path = '$file';
            INSERT INTO ast_index
            SELECT *, current_timestamp as indexed_at, '$GIT_COMMIT' as git_commit
            FROM read_ast('$file');
        "
    done
}

# Find functions using semantic types
find() {
    local pattern="${1:-%}"
    sql_out "
        SELECT * FROM ast_find_functions('%$pattern%')
        ORDER BY complexity DESC
        LIMIT 20
    "
}

# Search all searchable items
search() {
    local pattern="${1:-%}"
    sql_out "
        SELECT * FROM ast_find_searchable('%$pattern%')
        ORDER BY complexity DESC
        LIMIT 30
    "
}

# Find hotspots
hotspots() {
    local threshold="${1:-200}"
    sql_out "
        SELECT * FROM ast_find_hotspots($threshold)
        ORDER BY hotspot_score DESC
        LIMIT 20
    "
}

# Show dependencies
deps() {
    local file="$1"
    if [[ -z "$file" ]]; then
        echo "Usage: ast deps <file>"
        exit 1
    fi
    
    sql_out "SELECT * FROM ast_get_dependencies('$file')"
}

# Find unused functions
unused() {
    sql_out "
        WITH all_functions AS (
            SELECT DISTINCT 
                f.file_path,
                ast_get_function_name(f.file_path, f.node_id) as func_name
            FROM ast_index f
            WHERE f.semantic_type = semantic_type_code('DEFINITION_FUNCTION')
        ),
        called_functions AS (
            SELECT DISTINCT name as func_name
            FROM ast_index
            WHERE is_call(semantic_type) AND name IS NOT NULL
        )
        SELECT af.file_path, af.func_name
        FROM all_functions af
        LEFT JOIN called_functions cf ON af.func_name = cf.func_name
        WHERE cf.func_name IS NULL
          AND af.func_name IS NOT NULL
          AND af.func_name NOT IN ('main', 'init', 'cleanup')
          AND af.func_name NOT LIKE 'test_%'
        ORDER BY af.file_path, af.func_name
        LIMIT 50
    "
}

# Show statistics
stats() {
    sql_out "
        WITH stats AS (
            SELECT 
                COUNT(DISTINCT file_path) as files,
                COUNT(*) as nodes,
                COUNT(*) FILTER (WHERE is_definition(semantic_type)) as definitions,
                COUNT(*) FILTER (WHERE is_call(semantic_type)) as calls,
                COUNT(DISTINCT get_super_kind(semantic_type)) as super_kinds
            FROM ast_index
        ),
        type_breakdown AS (
            SELECT 
                get_super_kind(semantic_type) as category,
                COUNT(*) as count
            FROM ast_index
            WHERE semantic_type IS NOT NULL
            GROUP BY category
        )
        SELECT 'Files' as metric, files::VARCHAR as value FROM stats
        UNION ALL
        SELECT 'Total Nodes', nodes::VARCHAR FROM stats
        UNION ALL
        SELECT 'Definitions', definitions::VARCHAR FROM stats
        UNION ALL
        SELECT 'Function Calls', calls::VARCHAR FROM stats
        UNION ALL
        SELECT '', ''
        UNION ALL
        SELECT 'Category: ' || category, count::VARCHAR FROM type_breakdown
        ORDER BY 1
    "
}

# Git integration - show files changed since last index
git-changed() {
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        echo "Not in a git repository"
        exit 1
    fi
    
    sql_out "
        WITH indexed_files AS (
            SELECT DISTINCT 
                file_path, 
                MAX(git_commit) as last_commit,
                MAX(indexed_at) as last_indexed
            FROM ast_index
            GROUP BY file_path
        ),
        current_commit AS (
            SELECT '$(git rev-parse HEAD)' as sha
        )
        SELECT 
            f.file_path,
            f.last_commit,
            CASE 
                WHEN f.last_commit = c.sha THEN 'Up to date'
                ELSE 'Needs update'
            END as status
        FROM indexed_files f, current_commit c
        WHERE f.last_commit != c.sha
        ORDER BY f.file_path
    "
}

# Main command dispatcher
case "$1" in
    init)
        init
        ;;
    index)
        shift
        index "$@"
        ;;
    update)
        shift
        update "$@"
        ;;
    find)
        shift
        find "$@"
        ;;
    search)
        shift
        search "$@"
        ;;
    hotspots)
        shift
        hotspots "$@"
        ;;
    deps)
        shift
        deps "$@"
        ;;
    unused)
        unused
        ;;
    stats)
        stats
        ;;
    git-changed)
        git-changed
        ;;
    *)
        cat << 'EOF'
AST Tool - Semantic-aware code analysis

USAGE:
  # Database Management
  ast init                    Initialize the AST database
  ast stats                   Show database statistics
  
  # Indexing (now in database, not parquet files)
  ast index <pattern...>      Index files matching patterns
  ast update <file...>        Update specific files
  
  # Search (using semantic types)
  ast find <pattern>          Find functions by name
  ast search <pattern>        Search all definitions
  
  # Analysis
  ast hotspots [threshold]    Find code hotspots
  ast deps <file>            Show file dependencies
  ast unused                 Find potentially unused functions
  
  # Git Integration
  ast git-changed            Show files changed since last index

KEY IMPROVEMENTS:
  - Uses semantic type functions (is_definition, is_call, etc.)
  - Single DuckDB database instead of multiple parquet files
  - Reusable SQL macros for common queries
  - Git integration for tracking changes
  - Incremental updates for individual files

EXAMPLES:
  ast init                              # Initialize database
  ast index "src/**/*.cpp" "**/*.h"     # Index C++ files
  ast find ParseToAST                   # Find function
  ast search "semantic_type"            # Search all definitions
  ast hotspots 300                      # Find complex code
  ast update src/main.cpp               # Update single file
EOF
        ;;
esac