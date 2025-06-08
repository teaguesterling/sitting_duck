#!/bin/bash

# Quick function extraction tool using tree-based approach
# Usage: ./extract_functions.sh <file_path> [options]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DUCKDB="$PROJECT_DIR/build/release/duckdb"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <file_path> [--with-return-types] [--verbose] [--limit N]"
    echo ""
    echo "Examples:"
    echo "  $0 src/unified_ast_backend.cpp"
    echo "  $0 src/unified_ast_backend.cpp --with-return-types --limit 5"
    echo "  $0 src/unified_ast_backend.cpp --verbose"
    exit 1
fi

FILE_PATH="$1"
shift

# Parse options
WITH_RETURN_TYPES=false
VERBOSE=false
LIMIT=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --with-return-types)
            WITH_RETURN_TYPES=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --limit)
            LIMIT="LIMIT $2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check if file exists
if [ ! -f "$FILE_PATH" ]; then
    echo "Error: File '$FILE_PATH' not found"
    exit 1
fi

# Check if DuckDB binary exists
if [ ! -f "$DUCKDB" ]; then
    echo "Error: DuckDB binary not found at $DUCKDB"
    echo "Please run 'make' first to build the extension"
    exit 1
fi

if [ "$VERBOSE" = true ]; then
    echo "Extracting functions from: $FILE_PATH"
    echo "With return types: $WITH_RETURN_TYPES"
    echo "Using DuckDB: $DUCKDB"
    echo ""
fi

# Build the SQL query
if [ "$WITH_RETURN_TYPES" = true ]; then
    SQL="
    -- Extract functions with return types
    WITH func_defs AS (
        SELECT node_id, start_line, end_line
        FROM read_ast('$FILE_PATH')
        WHERE type = 'function_definition' AND semantic_type = 112
    ),
    -- Get return types (sibling_index = 0)
    return_types AS (
        SELECT 
            fd.node_id,
            fd.start_line,
            fd.end_line,
            MAX(CASE WHEN c.sibling_index = 0 THEN c.name END) as return_type,
            MAX(CASE WHEN c.sibling_index = 0 THEN c.type END) as return_type_kind
        FROM func_defs fd
        JOIN read_ast('$FILE_PATH') c ON c.parent_id = fd.node_id
        GROUP BY fd.node_id, fd.start_line, fd.end_line
    ),
    -- Get function names (from declarator children)
    func_names AS (
        SELECT 
            fd.node_id,
            MAX(CASE 
                WHEN d.type = 'function_declarator' AND gc.type IN ('qualified_identifier', 'identifier')
                THEN gc.name 
            END) as full_name,
            MAX(CASE 
                WHEN d.type = 'function_declarator' AND pc.type = 'parameter_list'
                THEN pc.children_count 
                ELSE 0
            END) as param_count
        FROM func_defs fd
        JOIN read_ast('$FILE_PATH') d ON d.parent_id = fd.node_id
        LEFT JOIN read_ast('$FILE_PATH') gc ON gc.parent_id = d.node_id
        LEFT JOIN read_ast('$FILE_PATH') pc ON pc.parent_id = d.node_id
        GROUP BY fd.node_id
    )
    SELECT 
        COALESCE(rt.return_type, 'unknown') as return_type,
        CASE 
            WHEN fn.full_name LIKE '%::%' 
            THEN split_part(fn.full_name, '::', 2)
            ELSE fn.full_name
        END as function_name,
        CASE 
            WHEN fn.full_name LIKE '%::%' 
            THEN split_part(fn.full_name, '::', 1)
            ELSE NULL
        END as class_name,
        rt.start_line,
        rt.end_line - rt.start_line + 1 as line_count,
        fn.param_count,
        fn.full_name as qualified_name
    FROM return_types rt
    LEFT JOIN func_names fn ON fn.node_id = rt.node_id
    WHERE fn.full_name IS NOT NULL
    ORDER BY rt.start_line
    $LIMIT;
    "
else
    SQL="
    -- Extract functions (fast version without return types)
    WITH func_declarators AS (
        SELECT node_id, parent_id, start_line, end_line, children_count
        FROM read_ast('$FILE_PATH')
        WHERE type = 'function_declarator' AND semantic_type = 112
    ),
    function_info AS (
        SELECT 
            fd.node_id,
            fd.start_line,
            fd.end_line,
            MAX(CASE 
                WHEN c.type IN ('identifier', 'qualified_identifier') 
                THEN c.name
            END) as full_name,
            MAX(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count
        FROM func_declarators fd
        JOIN read_ast('$FILE_PATH') c ON c.parent_id = fd.node_id
        GROUP BY fd.node_id, fd.start_line, fd.end_line
    )
    SELECT 
        CASE 
            WHEN full_name LIKE '%::%' 
            THEN split_part(full_name, '::', 2)
            ELSE full_name
        END as function_name,
        CASE 
            WHEN full_name LIKE '%::%' 
            THEN split_part(full_name, '::', 1)
            ELSE NULL
        END as class_name,
        CASE 
            WHEN full_name LIKE '%::%' THEN 'method'
            ELSE 'function'
        END as function_type,
        start_line,
        end_line,
        end_line - start_line + 1 as line_count,
        param_count,
        full_name as qualified_name
    FROM function_info
    WHERE full_name IS NOT NULL
    ORDER BY start_line
    $LIMIT;
    "
fi

# Execute the query
if [ "$VERBOSE" = true ]; then
    echo "Executing query..."
    echo ""
fi

"$DUCKDB" -s "$SQL"