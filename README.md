# Sitting Duck 🦆

A DuckDB extension that parses source code into Abstract Syntax Trees (ASTs) and makes them queryable with SQL.

## What It Does

Sitting Duck lets you analyze source code using SQL queries. Parse your codebase once, then query it like any other database:

```sql
-- Load the extension
LOAD sitting_duck;

-- Find all functions in a Python file
SELECT name, start_line, end_line 
FROM read_ast('my_script.py') 
WHERE type = 'function_definition';

-- Count different node types
SELECT type, COUNT(*) 
FROM read_ast('my_script.py') 
GROUP BY type 
ORDER BY COUNT(*) DESC;
```

## Supported Languages

Currently supports **13 languages** via Tree-sitter parsers:

- **Web**: JavaScript, TypeScript, HTML, CSS
- **Systems**: C, C++, Rust, Go  
- **Scripting**: Python, Ruby, PHP
- **Enterprise**: Java
- **Documentation**: Markdown

## Installation

```bash
# Clone with submodules (required for Tree-sitter grammars)
git clone --recursive https://github.com/your-repo/sitting-duck.git
cd sitting-duck

# Build the extension
make

# Test it works
./build/release/duckdb -c "LOAD './build/release/extension/sitting_duck/sitting_duck.duckdb_extension'; SELECT COUNT(*) FROM read_ast('README.md');"
```

## Basic Usage

### Parse a Single File

```sql
-- See all nodes in a file
SELECT * FROM read_ast('example.py') LIMIT 10;

-- Find function definitions
SELECT name, start_line, end_line
FROM read_ast('example.py')
WHERE type = 'function_definition';

-- Show tree structure
SELECT 
    repeat('  ', depth) || type as indented_type,
    name,
    start_line
FROM read_ast('example.py')
ORDER BY node_id
LIMIT 20;
```

### Parse Multiple Files

```sql
-- Analyze all Python files in a directory
SELECT file_path, COUNT(*) as node_count
FROM read_ast('src/**/*.py')  
GROUP BY file_path
ORDER BY node_count DESC;

-- Find all imports across your project  
SELECT file_path, peek as import_statement
FROM read_ast('**/*.py')
WHERE type IN ('import_statement', 'import_from_statement');
```

## Table Schema

The `read_ast()` function returns this schema:

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | BIGINT | Unique node identifier |
| `type` | VARCHAR | AST node type (e.g., 'function_definition') |
| `name` | VARCHAR | Node name if applicable |
| `file_path` | VARCHAR | Source file path |
| `start_line` | INT | Starting line number (1-based) |
| `start_column` | INT | Starting column (1-based) |  
| `end_line` | INT | Ending line number (1-based) |
| `end_column` | INT | Ending column (1-based) |
| `parent_id` | BIGINT | Parent node ID (NULL for root) |
| `sibling_index` | INT | Position among siblings (0-based) |
| `depth` | INT | Tree depth (0 for root) |
| `children_count` | INT | Number of direct children |
| `descendant_count` | INT | Total descendants (useful for complexity) |
| `peek` | VARCHAR | Source code snippet for this node |

## CLI Tool

The project includes a unified CLI tool at `./ast` (symlink to `tools/ast-cli/ast`):

```bash
# Index files for fast analysis
./ast index py "**/*.py"
./ast index cpp "src/**/*.cpp" "include/**/*.h"

# Query the indexes
./ast funcs "**/*.py" "parse*"    # Find functions matching "parse*"
./ast complex 50                  # Find functions with >50 nodes
./ast hotspots 100               # Find complexity hotspots
./ast src function_name          # Show source code for a function

# See all commands
./ast help
```

## Real Examples

### Find Complex Functions
```sql
-- Functions with >100 AST nodes (complexity indicator)
SELECT name, file_path, descendant_count as complexity
FROM read_ast('**/*.py')
WHERE type = 'function_definition' 
  AND descendant_count > 100
ORDER BY complexity DESC;
```

### Analyze Import Patterns
```sql
-- Most imported modules
SELECT 
    regexp_extract(peek, 'from (\w+)', 1) as module,
    count(*) as usage_count
FROM read_ast('**/*.py')
WHERE type = 'import_from_statement'
GROUP BY module
ORDER BY usage_count DESC;
```

### Class Hierarchy
```sql
-- Find all classes and their methods
SELECT 
    c.name as class_name,
    c.file_path,
    m.name as method_name,
    m.start_line
FROM read_ast('**/*.py') c
JOIN read_ast('**/*.py') m ON m.parent_id = c.node_id
WHERE c.type = 'class_definition' 
  AND m.type = 'function_definition'
ORDER BY c.name, m.start_line;
```

## Performance Notes

- **Streaming**: Parses files one-by-one, so you see results immediately
- **Memory efficient**: Only one file's AST in memory at a time
- **Glob patterns**: Use `**/*.ext` for recursive directory searches
- **Peek modes**: Control how much source text to extract (affects performance)

```sql
-- Fastest: no source text
SELECT COUNT(*) FROM read_ast('**/*.py', peek_mode := 'none');

-- Balanced: compact source snippets  
SELECT COUNT(*) FROM read_ast('**/*.py', peek_mode := 'compact');

-- Complete: full source text (slower)
SELECT COUNT(*) FROM read_ast('**/*.py', peek_mode := 'smart');
```

## Advanced Features

The extension includes SQL macros for common analysis patterns. Load them with:

```sql
.read ast-navigator.sql

-- Use pre-built analysis functions
SELECT * FROM ast_find_functions('**/*.py');
SELECT * FROM ast_complexity_stats('**/*.py');
```

## Limitations

- **Parse-only**: This analyzes syntax, not semantics (no type checking, symbol resolution, etc.)
- **Tree-sitter dependent**: Parsing quality depends on Tree-sitter grammar completeness
- **Single-threaded parsing**: Files are parsed sequentially (though DuckDB can parallelize queries)
- **In-memory ASTs**: Large files may use significant memory during parsing

## Common Use Cases

- **Code quality analysis**: Find overly complex functions
- **Refactoring assistance**: Understand code structure before changes
- **Documentation**: Extract function signatures and comments
- **Pattern detection**: Find common coding patterns or antipatterns
- **Dependency analysis**: Track imports and includes
- **Learning codebases**: Quickly understand unfamiliar code structure

## Contributing

This project uses Tree-sitter grammars as git submodules. To add a new language:

1. Add the grammar: `git submodule add <grammar-repo> grammars/tree-sitter-<lang>`
2. Update `CMakeLists.txt` to build the grammar
3. Add language adapter in `src/language_adapters/`
4. Add type definitions in `src/language_configs/`

See `docs/ADDING_NEW_LANGUAGES.md` for details.

## License

MIT License - see LICENSE file for details.

---

*Sitting Duck transforms your source code into a sitting duck for SQL-based analysis. 🦆*