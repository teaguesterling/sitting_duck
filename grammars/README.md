# Tree-sitter Grammar Submodules

This directory contains git submodules for tree-sitter language grammars used by the duckdb_ast extension.

## Currently Included Grammars

- **tree-sitter-python**: Python language grammar

## Adding New Grammars

To add support for a new language, add its tree-sitter grammar as a submodule:

```bash
cd <project root>
git submodule add https://github.com/tree-sitter/tree-sitter-<language>.git grammars/tree-sitter-<language>
```

Then update the CMakeLists.txt to include the grammar's parser.c (and scanner.c/cc if present) in the EXTENSION_SOURCES.

## Updating Grammars

To update all grammar submodules to their latest versions:

```bash
git submodule update --remote --merge
```

To update a specific grammar:

```bash
cd grammars/tree-sitter-<language>
git pull origin main
cd ../..
git add grammars/tree-sitter-<language>
git commit -m "Update tree-sitter-<language> to latest version"
```
