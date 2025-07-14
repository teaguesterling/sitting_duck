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

Sitting Duck lets you analyze source code using SQL queries with language-specific semantic understanding. Parse your codebase once, then query it like any other database:

```sql
-- Load the extension
LOAD sitting_duck;

-- Find all functions with their signatures (native context)
SELECT name, semantic_type as signature, start_line, end_line 
FROM read_ast('my_script.py', 'python', context := 'native') 
WHERE type = 'function_definition';

-- Analyze Java method return types
SELECT name, semantic_type as return_type, start_line
FROM read_ast('MyClass.java', 'java', context := 'native')
WHERE type = 'method_declaration';

-- Count different node types
SELECT type, COUNT(*) 
FROM read_ast('my_script.py') 
GROUP BY type 
ORDER BY COUNT(*) DESC;
```

## Architecture

Sitting Duck transforms your source code into queriable data structures:

1. **Tree-sitter parsing** - Robust, error-recovering parsers for 12+ languages
2. **Native semantic extraction** - Language-specific semantic analysis with type information
3. **Multiple context levels** - From basic parsing to full semantic understanding
4. **SQL interface** - Rich table functions with DuckDB-consistent design
5. **Memory-safe processing** - Production-ready with comprehensive error handling
6. **Streaming design** - Efficient processing of large codebases

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
-- Single glob pattern
SELECT file_path, COUNT(*) as node_count
FROM read_ast('src/**/*.py')  
GROUP BY file_path
ORDER BY node_count DESC;

-- Array of specific files (Note: currently processes first file only)
SELECT file_path, language, COUNT(*) as nodes
FROM read_ast([
    'main.py',
    'utils.py', 
    'config.js'
])
GROUP BY file_path, language
ORDER BY nodes DESC;

-- Process files with batch processing
SELECT file_path, COUNT(*) as nodes
FROM read_ast([
    'src/module1.py',
    'src/module2.py'
], batch_size := 2)
GROUP BY file_path;

-- Find functions in specific files
SELECT file_path, name, start_line
FROM read_ast(['main.py', 'utils.py'])
WHERE type = 'function_definition'
ORDER BY file_path, start_line;
```

## DuckDB-Consistent Interface

Sitting Duck follows DuckDB conventions, making it feel native to DuckDB users:

### Function Overloads (like `read_csv`, `read_parquet`)

```sql
-- Single file (VARCHAR)
SELECT * FROM read_ast('main.py');

-- Array of files (LIST(VARCHAR))
SELECT * FROM read_ast(['main.py', 'utils.py']);

-- With explicit language
SELECT * FROM read_ast('script.py', 'python');
SELECT * FROM read_ast(['file1.py', 'file2.py'], 'python');

-- With native context extraction
SELECT * FROM read_ast('main.py', 'python', context := 'native');
```

### Named Parameters

```sql
-- Error handling (like other DuckDB file functions)
SELECT * FROM read_ast(['file1.py', 'file2.py'], ignore_errors := true);

-- Context extraction levels
SELECT * FROM read_ast('main.py', context := 'native');        -- Full semantic analysis
SELECT * FROM read_ast('main.py', context := 'normalized');    -- Cross-language normalization
SELECT * FROM read_ast('main.py', context := 'node_types_only'); -- Basic types only

-- Source and structure control
SELECT * FROM read_ast('main.py', source := 'full', structure := 'full');

-- Performance tuning
SELECT * FROM read_ast('main.py', peek := 50);                 -- Limit peek size
SELECT * FROM read_ast('main.py', peek := 'smart');            -- Smart peek mode

-- Batch processing
SELECT * FROM read_ast(['file1.py', 'file2.py'], batch_size := 2);
```

### Consistent Behavior

- **Parameter validation**: Comprehensive validation with clear error messages
- **Error handling**: Graceful handling with `ignore_errors` parameter  
- **Memory safety**: Robust multi-file processing with corruption prevention
- **Native extraction**: Language-specific semantic analysis (Go, Java, C++, Python, JavaScript, etc.)
- **Batch processing**: Configurable batch sizes for performance optimization

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
| `semantic_type` | VARCHAR | Language-specific semantic type (native context) or universal type |

### Context Extraction Levels

The extension supports multiple context extraction levels via the `context` parameter:

- **`'none'`**: Minimal processing, fastest performance
- **`'node_types_only'`**: Basic AST node types only
- **`'normalized'`**: Cross-language semantic normalization  
- **`'native'`**: Full language-specific semantic analysis (recommended)

In native context mode, `semantic_type` contains language-specific semantic information:
- **Go**: Function types, method receivers, return types, parameter types
- **Java**: Method signatures, class types, return types, access modifiers
- **C++**: Function signatures, class/struct types, template information
- **Python**: Function/class definitions, decorators, type hints
- **JavaScript**: Function types, arrow functions, class methods

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
-- Find all function definitions across languages with native context
SELECT file_path, name, language, semantic_type
FROM read_ast('main.py', 'python', context := 'native')
WHERE type = 'function_definition';

-- Compare Java and C++ method signatures
UNION ALL
(
  SELECT 'java' as lang, name, semantic_type as signature
  FROM read_ast('MyClass.java', 'java', context := 'native')
  WHERE type = 'method_declaration'
)
UNION ALL  
(
  SELECT 'cpp' as lang, name, semantic_type as signature
  FROM read_ast('MyClass.cpp', 'cpp', context := 'native')
  WHERE type = 'function_definition'
);

-- Analyze function complexity in native context
SELECT 
    name,
    semantic_type,
    descendant_count as complexity
FROM read_ast('complex_file.py', 'python', context := 'native')
WHERE type = 'function_definition'
ORDER BY complexity DESC;
```

**Native Context Features:**
- **Language-specific semantic types**: Method signatures, return types, parameter types
- **Cross-language compatibility**: Compare similar constructs across languages
- **Enhanced analysis**: More detailed semantic information than normalized context
- **Production ready**: Thoroughly tested with memory safety guarantees

> Use `context := 'native'` for the most detailed analysis. See language-specific tests for examples.

### Native Context Extraction

The native context mode provides language-specific semantic analysis:

```sql
-- Get Java method signatures with return types
SELECT 
    name as method_name,
    semantic_type as return_type,
    start_line
FROM read_ast('MyClass.java', 'java', context := 'native')
WHERE type = 'method_declaration';

-- Find Go function signatures
SELECT 
    name as function_name,
    semantic_type as signature,
    file_path
FROM read_ast('*.go', 'go', context := 'native')
WHERE type = 'function_declaration';

-- Analyze C++ class hierarchies
SELECT 
    name as class_name,
    semantic_type as class_info,
    descendant_count as complexity
FROM read_ast('*.cpp', 'cpp', context := 'native')
WHERE type = 'class_specifier'
ORDER BY complexity DESC;
```

## Limitations

- **Parse-only**: This analyzes syntax, not semantics (no type checking, symbol resolution, etc.)
- **Tree-sitter dependent**: Parsing quality depends on Tree-sitter grammar completeness
- **Multi-file arrays**: Currently processes first file/pattern only (limitation under active development)
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
