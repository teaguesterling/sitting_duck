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
- **Glob Pattern Support**: Analyze entire codebases with `'src/**/*.py'`
- **Multi-File Processing**: Efficient batch processing of multiple files
- **Smart Caching**: Optimized for large codebases
- **Thread-Safe**: Concurrent parsing support

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

### Find all imports
```sql
SELECT source_text
FROM read_ast('script.py', 'python')
WHERE type IN ('import_statement', 'import_from_statement');
```

### Analyze function complexity
```sql
WITH function_nodes AS (
    SELECT node_id, name, start_line, end_line
    FROM read_ast('code.py', 'python')
    WHERE type = 'function_definition'
)
SELECT 
    f.name,
    f.end_line - f.start_line + 1 as lines,
    COUNT(DISTINCT a.node_id) as total_nodes
FROM function_nodes f
JOIN read_ast('code.py', 'python') a 
    ON a.start_line >= f.start_line 
    AND a.end_line <= f.end_line
GROUP BY f.name, f.start_line, f.end_line
ORDER BY total_nodes DESC;
```

### Find deeply nested code
```sql
SELECT 
    type,
    name,
    depth,
    start_line,
    source_text
FROM read_ast('complex.py', 'python')
WHERE depth > 10
ORDER BY depth DESC, start_line;
```

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