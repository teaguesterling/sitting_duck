# DuckDB AST Extension Architecture

## Overview

The duckdb_ast extension enables SQL-based querying of Abstract Syntax Trees (ASTs) from source code files. It uses Tree-sitter for parsing and exposes the AST data through DuckDB's table function interface.

## Directory Structure

```
duckdb_ast/
├── src/
│   ├── include/
│   │   ├── duckdb_ast_extension.hpp    # Main extension header
│   │   ├── ast_parser.hpp              # AST parser interface
│   │   └── grammars.hpp                # Language grammar registry
│   ├── duckdb_ast_extension.cpp        # Extension entry point
│   ├── ast_parser.cpp                  # Tree-sitter wrapper
│   ├── read_ast_function.cpp           # read_ast table function
│   └── grammars.cpp                    # Language registration
├── grammars/                           # Git submodules for languages
│   └── tree-sitter-python/             # Python grammar
└── test/
    └── sql/
        └── duckdb_ast.test             # SQL tests
```

## Key Components

### 1. Extension Entry Point (`duckdb_ast_extension.cpp`)
- Registers the extension with DuckDB
- Calls registration functions for table functions

### 2. AST Parser (`ast_parser.cpp`)
- Wraps Tree-sitter C API
- Converts Tree-sitter nodes to DuckDB-friendly ASTNode structs
- Handles file I/O and string parsing
- Generates deterministic node IDs

### 3. Grammar Registry (`grammars.cpp`)
- Maps language names to Tree-sitter language objects
- Provides list of supported languages
- Easily extensible for new languages

### 4. Table Functions (`read_ast_function.cpp`)
- `read_ast(file_path, language)`: Main function that returns AST nodes
- Implements DuckDB's TableFunction interface
- Handles binding and execution phases

## Data Model

The AST is represented as a table with the following schema:

| Column | Type | Description |
|--------|------|-------------|
| node_id | BIGINT | Unique identifier for the node |
| type | VARCHAR | Node type (e.g., 'function_definition') |
| name | VARCHAR | Node name if applicable |
| file_path | VARCHAR | Source file path |
| start_line | INTEGER | Starting line (1-based) |
| start_column | INTEGER | Starting column (0-based) |
| end_line | INTEGER | Ending line (1-based) |
| end_column | INTEGER | Ending column (0-based) |
| parent_id | BIGINT | Parent node ID (NULL for root) |
| depth | INTEGER | Tree depth (0 for root) |
| sibling_index | INTEGER | Position among siblings |
| source_text | VARCHAR | Actual source code |

## Adding New Languages

1. Add the grammar as a git submodule:
   ```bash
   git submodule add https://github.com/tree-sitter/tree-sitter-<language>.git grammars/tree-sitter-<language>
   ```

2. Update `CMakeLists.txt` to include the grammar files:
   ```cmake
   set(EXTENSION_SOURCES 
       ...
       grammars/tree-sitter-<language>/src/parser.c
       grammars/tree-sitter-<language>/src/scanner.c  # if exists
   )
   ```

3. Update `grammars.cpp` to register the language:
   ```cpp
   extern "C" {
       TSLanguage *tree_sitter_<language>();
   }
   
   const TSLanguage* GetLanguage(const std::string &language) {
       if (language == "<language>") {
           return tree_sitter_<language>();
       }
       ...
   }
   ```

## Build System

- Uses DuckDB's extension build system
- Depends on tree-sitter via vcpkg
- Grammar files included as git submodules
- CMake handles compilation of C grammar files

## Future Enhancements

1. **Filtered Views**: Functions like `ast_functions()`, `ast_classes()`
2. **Source Extraction**: `get_node_source()` with context
3. **Method Chaining**: Custom types for fluent API
4. **Performance**: Lazy evaluation, caching, parallel parsing
5. **More Languages**: JavaScript, TypeScript, Java, C++, etc.