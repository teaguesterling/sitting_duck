# Basic AST Reading with read_ast()

**Status**: Completed
**Completed**: 2024-01-23

## Description
Implement the core `read_ast(file_path, language)` table function that returns AST nodes as rows.

## Features Implemented
- Tree-sitter integration for parsing
- Python language support via tree-sitter-python
- Full AST node information with 12 columns:
  - node_id, type, name, file_path
  - start_line, start_column, end_line, end_column
  - parent_id, depth, sibling_index, source_text
- Name extraction for functions, classes, and identifiers
- Error handling for missing files and unsupported languages
- Comprehensive test suite with 18 test cases

## Technical Details
- Uses vcpkg for tree-sitter dependency
- Git submodule for tree-sitter-python grammar
- Follows DuckDB extension patterns