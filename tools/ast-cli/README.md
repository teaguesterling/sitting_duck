# AST CLI - Unified AST Analysis Tool

This is the unified AST analysis tool that combines the best features from the previous three tools:
- Parquet-based indexing system (from `./ast`)
- Clean semantic queries (from `./ast-nav`) 
- Database storage capabilities (from `./ast-refactored`)

## Quick Start

```bash
# Create an index for Python files
./ast index py "**/*.py"

# Find all functions in a directory
./ast funcs "src/**/*.py"

# Find a specific function
./ast find parse_ast

# Show function source code
./ast src parse_ast

# Find complex functions
./ast complex 50

# Find code hotspots
./ast hotspots 100
```

## Features

### Indexing
- **Multiple patterns**: Support for multiple glob patterns in one command
- **Parquet storage**: Fast querying with DuckDB's parquet engine
- **Language detection**: Automatic language detection from file extensions

### Analysis
- **Complexity analysis**: Find functions with high node counts
- **Hotspot detection**: Identify code complexity and coupling issues
- **Duplicate detection**: Find similar code patterns
- **Dependency analysis**: Understand code relationships

### Navigation
- **Function search**: Find functions by name or pattern
- **Source extraction**: Get full source code for functions
- **Context viewing**: See code context around functions
- **Cross-references**: Find callers and callees

### Storage Options
- **Parquet mode**: Fast read-only analysis (default)
- **Database mode**: Persistent storage with updates (`ast init`)

## Command Reference

### Indexing Commands
- `ast index <lang> <pattern...>` - Create parquet index
- `ast init` - Initialize database storage
- `ast update <files...>` - Update database
- `ast list` - List available indexes
- `ast stats` - Show index statistics

### Search Commands
- `ast funcs <pattern> [name]` - Find functions
- `ast find <function> [lang]` - Find function across indexes
- `ast search <term> [lang]` - Search names containing term
- `ast classes <pattern> [name]` - Find classes/structs

### Analysis Commands
- `ast complex [threshold]` - Find complex functions
- `ast hotspots [threshold]` - Find code hotspots
- `ast unused [lang]` - Find unused functions
- `ast duplicates [similarity]` - Find duplicates
- `ast metrics [type]` - Advanced metrics

### Navigation Commands
- `ast src <function>` - Show function source
- `ast context <function> [lines]` - Show context
- `ast definition <symbol> [lang]` - Show definition
- `ast references <symbol> [lang]` - Find references
- `ast tree <file>` - Show file structure

### File Analysis
- `ast file <path>` - Analyze specific file
- `ast deps <path>` - Show dependencies
- `ast callers <function>` - Find callers
- `ast called-by <function>` - Find callees

## Migration from Old Tools

### From `./ast`
The new tool maintains all indexing and analysis features:
```bash
# Old: ./ast index cpp "src/**/*.cpp" "include/**/*.h"
# New: ./ast index cpp "src/**/*.cpp" "include/**/*.h"  # Same syntax!
```

### From `./ast-nav`
Semantic queries work the same way:
```bash
# Old: ./ast-nav functions "src/**/*.py" "parse*"
# New: ./ast funcs "src/**/*.py" "parse*"  # Slightly shorter command
```

### From `./ast-refactored`
Database functionality is available:
```bash
# Old: ./ast-refactored init
# New: ./ast init  # Same functionality
```

## Performance

The unified tool optimizes for different use cases:
- **Quick analysis**: Use parquet mode for read-only operations
- **Persistent storage**: Use database mode for ongoing projects
- **Large codebases**: Automatic syntax node filtering for performance

## Dependencies

- DuckDB built with AST extension
- AST extension loaded and available
- SQL library files for parquet operations

## Backward Compatibility

For transition period, the old tools remain available in `tools/legacy/`:
- `tools/legacy/ast-original`
- `tools/legacy/ast-nav-original` 
- `tools/legacy/ast-refactored-original`