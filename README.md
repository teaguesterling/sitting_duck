# DuckDB AST Extension

A powerful DuckDB extension that enables SQL-based analysis of Abstract Syntax Trees (ASTs) from source code files across multiple programming languages.

## Features

### 🌐 Multi-Language Support
- **5 Languages Supported**: Python, JavaScript, TypeScript, C++, SQL
- **Tree-sitter Powered**: Robust parsing with error recovery
- **Auto-Detection**: Automatic language detection from file extensions
- **Unified API**: Same interface works across all languages

### 🔍 Rich AST Analysis
- **Complete AST Access**: Full syntax tree with all node details
- **Semantic Categorization**: 8-bit semantic type system for cross-language analysis
- **Position Tracking**: Precise line/column locations for all nodes
- **Source Text Extraction**: Configurable peek modes (smart, compact, signature, etc.)

### ⚡ High Performance
- **Streaming Architecture**: Process files one-by-one, see results immediately
- **Memory Controlled**: Respects DuckDB memory limits, no memory explosion
- **Enterprise Scale**: 25M nodes across 4M files in ~25 seconds
- **Parallel Ready**: UNION queries automatically use multiple threads
- **Smart Text Extraction**: Configurable `peek_mode` for performance tuning
- **Parquet Integration**: Export 25M rows to 40MB compressed parquet files

### 📊 Advanced Static Analysis
- **Function Complexity**: Identify complex functions by AST node count and structure
- **Dependency Analysis**: Track file-level include/import relationships
- **Code Hotspots**: Find files with high complexity + coupling
- **Call Graph Analysis**: Build function dependency networks
- **Fan-in/Fan-out**: Identify highly coupled functions
- **Dead Code Detection**: Locate potentially unused functions

## Installation

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/yourusername/duckdb_ast.git
cd duckdb_ast

# Build the extension
make
```

## Usage

```sql
-- Load the extension
LOAD duckdb_ast;

-- Parse a Python file and query its AST
SELECT * FROM read_ast('path/to/file.py', 'python');

-- Find all function definitions
SELECT name, start_line, end_line 
FROM read_ast('file.py', 'python')
WHERE type = 'function_definition';

-- Count nodes by type
SELECT type, COUNT(*) as count
FROM read_ast('file.py', 'python')
GROUP BY type
ORDER BY count DESC;

-- Show tree structure
SELECT 
    REPEAT('  ', depth) || type AS tree,
    name,
    start_line
FROM read_ast('file.py', 'python')
WHERE depth <= 3
ORDER BY node_id;
```

## Schema

The `read_ast` function returns a table with the following columns:

| Column | Type | Description |
|--------|------|-------------|
| node_id | BIGINT | Unique identifier for the node |
| type | VARCHAR | Node type (e.g., 'function_definition', 'class_definition') |
| name | VARCHAR | Node name if applicable (e.g., function or class name) |
| file_path | VARCHAR | Path to the source file |
| start_line | INTEGER | Starting line number (1-based) |
| start_column | INTEGER | Starting column (0-based) |
| end_line | INTEGER | Ending line number (1-based) |
| end_column | INTEGER | Ending column (0-based) |
| parent_id | BIGINT | Parent node ID (NULL for root) |
| depth | INTEGER | Depth in the tree (0 for root) |
| sibling_index | INTEGER | Position among siblings (0-based) |
| source_text | VARCHAR | Actual source code for this node |

## Examples

### Basic AST Analysis

#### Find all imports
```sql
SELECT peek
FROM read_ast('script.py', 'python')
WHERE type IN ('import_statement', 'import_from_statement');
```

#### Find all function definitions
```sql
SELECT name, start_line, end_line, descendant_count
FROM read_ast('code.py', 'python')
WHERE type = 'function_definition'
ORDER BY descendant_count DESC;
```

### Advanced Static Analysis

#### Function Complexity Analysis
```sql
-- Find most complex functions across entire codebase
WITH function_info AS (
    SELECT 
        d.file_path,
        d.start_line,
        d.end_line,
        d.descendant_count,
        MAX(CASE WHEN c.type = 'identifier' THEN c.peek END) as function_name
    FROM nodes d
    JOIN nodes c ON c.parent_id = d.node_id AND c.file_path = d.file_path
    WHERE d.type = 'function_declarator' AND d.semantic_type = 112
    GROUP BY d.file_path, d.node_id, d.start_line, d.end_line, d.descendant_count
    HAVING function_name IS NOT NULL
)
SELECT 
    function_name,
    file_path,
    (end_line - start_line + 1) as line_count,
    descendant_count as complexity
FROM function_info
WHERE descendant_count > 100
ORDER BY descendant_count DESC;
```

#### Dependency Analysis
```sql
-- Find files with highest include dependencies
WITH includes AS (
    SELECT 
        file_path as source_file,
        REGEXP_EXTRACT(peek, '#include\s*[<"](.*?)[>"]', 1) as included_file
    FROM nodes
    WHERE type = 'preproc_include' AND peek LIKE '#include%'
)
SELECT 
    source_file,
    COUNT(DISTINCT included_file) as dependency_count
FROM includes
WHERE included_file IS NOT NULL
GROUP BY source_file
ORDER BY dependency_count DESC
LIMIT 10;
```

#### Code Hotspot Detection
```sql
-- Identify files with both complex functions AND high dependencies
WITH function_complexity AS (
    SELECT 
        file_path,
        MAX(descendant_count) as max_complexity,
        COUNT(*) as function_count
    FROM nodes
    WHERE type = 'function_declarator' AND semantic_type = 112
    GROUP BY file_path
),
file_dependencies AS (
    SELECT 
        file_path,
        COUNT(DISTINCT REGEXP_EXTRACT(peek, '#include\s*[<"](.*?)[>"]', 1)) as deps
    FROM nodes
    WHERE type = 'preproc_include' AND peek LIKE '#include%'
    GROUP BY file_path
)
SELECT 
    fc.file_path,
    fc.max_complexity,
    fc.function_count,
    COALESCE(fd.deps, 0) as dependencies,
    CASE 
        WHEN fc.max_complexity > 100 AND COALESCE(fd.deps, 0) > 30 THEN 'CRITICAL'
        WHEN fc.max_complexity > 50 AND COALESCE(fd.deps, 0) > 20 THEN 'HIGH'
        ELSE 'MODERATE'
    END as risk_level
FROM function_complexity fc
LEFT JOIN file_dependencies fd ON fc.file_path = fd.file_path
WHERE fc.max_complexity > 50 OR COALESCE(fd.deps, 0) > 15
ORDER BY fc.max_complexity DESC;
```

### Large-Scale Analysis

#### Performance at Scale
```sql
-- Streaming: Process 25M nodes across 4M files in ~25 seconds
-- Memory-controlled, immediate results, LIMIT-friendly
SELECT 
    COUNT(*) as total_nodes,
    COUNT(DISTINCT file_path) as total_files,
    COUNT(*) FILTER (WHERE type = 'function_declarator' AND semantic_type = 112) as functions,
    COUNT(*) FILTER (WHERE type = 'call_expression') as function_calls
FROM read_ast('duckdb/**/*.{cpp,hpp}');

-- Performance tuning with peek modes
SELECT COUNT(*) FROM read_ast('large_codebase/**/*.cpp', peek_mode := 'none');     -- Fastest
SELECT COUNT(*) FROM read_ast('large_codebase/**/*.cpp', peek_mode := 'compact');  -- Balanced  
SELECT COUNT(*) FROM read_ast('large_codebase/**/*.cpp', peek_mode := 'smart');    -- Full content

-- Export for efficient analysis
COPY (SELECT * FROM read_ast('**/*.cpp', peek_mode := 'none')) TO 'ast_data.parquet';
```

**Streaming Benefits:**
- ✅ **Immediate Results**: See data as soon as first file is parsed
- ✅ **Memory Efficient**: Only one file's AST in memory at a time  
- ✅ **LIMIT-Friendly**: Stop parsing when query limit is reached
- ✅ **Parallel Processing**: Use UNION for multi-threaded analysis

The extension can analyze enterprise codebases with millions of files efficiently.

## Static Analysis Utilities

The project includes comprehensive SQL macros for advanced code analysis in `ast-navigator.sql`:

### Core Analysis Functions
- **`ast_find_functions(file_pattern)`** - Extract all functions with names, locations, and complexity
- **`ast_find_classes(file_pattern)`** - Find class/struct definitions
- **`ast_get_complexity(file_pattern)`** - Calculate file-level complexity metrics
- **`ast_find_references(symbol_name)`** - Track variable/function usage across codebase

### Advanced Analytics
- **Fan-in/Fan-out Analysis** - Identify highly coupled functions
- **Call Graph Construction** - Build function dependency graphs  
- **Code Hotspot Detection** - Find files with high complexity + dependencies
- **Dead Code Detection** - Locate potentially unused functions
- **Dependency Analysis** - Track include/import relationships

### Example: Complete Function Analysis
```sql
-- Load the navigation macros
.read ast-navigator.sql

-- Analyze all functions in a project
SELECT * FROM ast_find_functions('src/**/*.cpp') 
WHERE complexity > 100 
ORDER BY complexity DESC;

-- Find code hotspots
SELECT * FROM ast_code_hotspots() 
WHERE hotspot_level IN ('CRITICAL_HOTSPOT', 'HIGH_HOTSPOT');
```

For complete examples and utilities, see `workspace/static_analysis_utilities.sql`.

## Adding Language Support

To add support for a new language:

1. Add the tree-sitter grammar as a git submodule:
   ```bash
   git submodule add https://github.com/tree-sitter/tree-sitter-<language>.git grammars/tree-sitter-<language>
   ```

2. Update `CMakeLists.txt` to include the grammar files

3. Register the language in `src/grammars.cpp`

## Development

See [workspace/design/architecture.md](workspace/design/architecture.md) for architectural details.

## License

MIT License - see LICENSE file for details.