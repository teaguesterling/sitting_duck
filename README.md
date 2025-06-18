# Sitting Duck 🦆

**Sitting Duck** is a DuckDB extension that makes Abstract Syntax Trees (ASTs) from source code files quack like data - enabling powerful SQL-based analysis across multiple programming languages.

## Why "Sitting Duck"?

The name reflects the project's philosophy and technology stack:
- **Sitting**: A nod to Tree-sitter, our parsing engine - your code sits in trees waiting for analysis
- **Duck**: Everything quacks like data in DuckDB - including your source code!
- **Target**: Your codebase becomes a sitting duck for powerful SQL-based analysis

## What Makes It Special

Traditional code analysis tools force you to learn their APIs and query languages. Sitting Duck lets you use the most powerful data analysis language ever created - **SQL** - to explore your codebase.

Code is data. Data wants to be queried. DuckDB makes querying a joy. Therefore: Analyzing code should be joyful. 🦆

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

## Architecture

Sitting Duck transforms your source code into queriable data structures:

1. **Tree-sitter parsing** - Robust, error-recovering parsers for 12+ languages
2. **Semantic classification** - Universal type system for cross-language analysis  
3. **SQL interface** - Rich table functions and streaming analysis
4. **Streaming design** - Efficient processing of large codebases

## Supported Languages

Currently supports **12 languages** via Tree-sitter parsers with full semantic analysis:

- **Web**: JavaScript, TypeScript, HTML, CSS
- **Systems**: C, C++, Go, Rust (via grammar)
- **Scripting**: Python, Ruby  
- **Enterprise**: Java
- **Other**: SQL, Markdown

*Note: Language support is actively expanding. See our language test coverage for the latest status.*

## Installation

```bash
# Clone with submodules (required for Tree-Sitter and Tree-Sitter grammars)
git clone --recursive https://github.com/your-repo/sitting_duck.git
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
| `language` | VARCHAR | Detected programming language |
| `start_line` | UINTEGER | Starting line number (1-based) |
| `start_column` | UINTEGER | Starting column (1-based) |  
| `end_line` | UINTEGER | Ending line number (1-based) |
| `end_column` | UINTEGER | Ending column (1-based) |
| `parent_id` | BIGINT | Parent node ID (NULL for root) |
| `depth` | UINTEGER | Tree depth (0 for root) |
| `sibling_index` | UINTEGER | Position among siblings (0-based) |
| `children_count` | UINTEGER | Number of direct children |
| `descendant_count` | UINTEGER | Total descendants (useful for complexity) |
| `peek` | VARCHAR | Source code snippet for this node |
| `semantic_type` | UTINYINT | Universal semantic category (0-255) |
| `universal_flags` | UTINYINT | Additional semantic flags |
| `arity_bin` | UTINYINT | Binned arity for analysis |

## Quick Start

```bash
# Install the extension
make

# Use SQL directly
./build/release/duckdb -c "LOAD sitting_duck; SELECT * FROM read_ast('main.py') LIMIT 10;"
# Technically THIS build doesn't need to LOAD sitting_duck.

# Or use the CLI tool (if available)
./tools/ast-cli/ast funcs "**/*.py" "test*"    # Find test functions
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

### Semantic Types for Cross-Language Analysis

The extension includes a universal semantic taxonomy that works across all languages. Use convenience functions for readable queries:

```sql
-- Find all functions regardless of language (readable approach)
SELECT file_path, name, language, semantic_type_to_string(semantic_type) as type_name
FROM read_ast('**/*.*', ignore_errors := true)
WHERE is_definition(semantic_type) AND semantic_type_to_string(semantic_type) = 'DEFINITION_FUNCTION';

-- Analyze semantic categories across languages
SELECT 
    language,
    get_super_kind(semantic_type) as category,
    COUNT(*) as node_count
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language, get_super_kind(semantic_type)
ORDER BY node_count DESC;

-- Find control flow complexity by file
SELECT 
    file_path, 
    language,
    COUNT(*) as control_flow_nodes
FROM read_ast('**/*.*', ignore_errors := true)
WHERE is_control_flow(semantic_type)
GROUP BY file_path, language
ORDER BY control_flow_nodes DESC;

-- Performance-optimized version using raw codes
SELECT file_path, name, language 
FROM read_ast('**/*.*', ignore_errors := true)
WHERE semantic_type = 112; -- DEFINITION_FUNCTION (faster)
```

**Available convenience functions:**
- `semantic_type_to_string()` - Convert codes to readable names
- `is_definition()`, `is_call()`, `is_control_flow()`, `is_identifier()` - Semantic predicates
- `get_super_kind()`, `get_kind()` - Category extraction

> See [API_REFERENCE.md](API_REFERENCE.md) for the complete semantic type hierarchy with cross-language examples.

### Multi-File Processing

```sql
-- Analyze entire codebases with glob patterns
SELECT 
    language, 
    COUNT(DISTINCT file_path) as files, 
    COUNT(CASE WHEN is_definition(semantic_type) AND semantic_type_to_string(semantic_type) = 'DEFINITION_FUNCTION' THEN 1 END) as functions,
    COUNT(CASE WHEN is_definition(semantic_type) AND semantic_type_to_string(semantic_type) = 'DEFINITION_CLASS' THEN 1 END) as classes
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY files DESC;

-- Find architectural patterns across languages
SELECT 
    get_super_kind(semantic_type) as semantic_category,
    language,
    COUNT(*) as usage_count
FROM read_ast('**/*.*', ignore_errors := true)
WHERE get_super_kind(semantic_type) IN ('COMPUTATION', 'CONTROL_EFFECTS')
GROUP BY get_super_kind(semantic_type), language
ORDER BY usage_count DESC;
```

## Limitations

- **Parse-only**: This analyzes syntax, not semantics (no type checking, symbol resolution, etc.)
- **Tree-sitter dependent**: Parsing quality depends on Tree-sitter grammar completeness
- **Single-threaded parsing**: Files are parsed sequentially (though results stream efficiently)
- **File-by-file processing**: Each file is parsed independently (no cross-file analysis)

## Use Cases

- **Code quality analysis** - Find complexity hotspots and code smells
- **Dependency analysis** - Understand import/include relationships
- **Refactoring assistance** - Identify patterns and duplicated code
- **Documentation generation** - Extract API signatures and comments
- **Security auditing** - Find dangerous patterns across languages
- **Learning and exploration** - Understand unfamiliar codebases quickly
- **Cross-language analysis** - Compare patterns across different programming languages

## Contributing

This project uses Tree-sitter grammars as git submodules. To add a new language:

1. Add the grammar: `git submodule add <grammar-repo> grammars/tree-sitter-<lang>`
2. Update `CMakeLists.txt` to build the grammar
3. Add language adapter in `src/language_adapters/`
4. Add type definitions in `src/language_configs/`

See `docs/ADDING_NEW_LANGUAGES.md` for details.

## License

MIT License - see LICENSE file for details.

## Documentation

For AI agents and advanced usage:
- **[AI Agent Guide](AI_AGENT_GUIDE.md)** - Comprehensive guide for AI agents using semantic types
- **[API Reference](API_REFERENCE.md)** - Complete function reference
- **[Adding Languages](docs/ADDING_NEW_LANGUAGES.md)** - How to add new language support

---

*Sitting Duck transforms your source code into a sitting duck for SQL-based analysis. 🦆*

*Previous name: DuckDB AST Extension*
