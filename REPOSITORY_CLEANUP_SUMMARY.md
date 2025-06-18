# Repository Cleanup Summary

## Changes Made

### CLI Tools Consolidation
- **Removed**: `ast-nav-indexed` (duplicate CLI tool)
- **Kept**: `tools/ast-cli/ast` as the unified CLI tool
- **Updated**: CLI tool now uses `queries/ast_queries.sql` as primary library

### SQL Library Organization
- **Created**: `queries/` directory structure
- **Moved**: `ast-navigator.sql` → `queries/legacy/ast-navigator.sql`
- **Moved**: `ast-nav-parquet.sql` → `queries/legacy/ast-nav-parquet.sql`
- **Primary**: `queries/ast_queries.sql` - Master query library for new work
- **Added**: `queries/README.md` - Documentation for SQL libraries

### File Organization
- **Moved**: `demo_parquet_index.sql` → `examples/demo_parquet_index.sql`
- **Archived**: All design documents from `workspace/design/` → `workspace/archive/design-archive/`

## Current Structure

```
queries/
├── ast_queries.sql          # Master library (use this)
├── README.md               # Library documentation
└── legacy/
    ├── ast-navigator.sql   # Complex C++ function extraction
    └── ast-nav-parquet.sql # Parquet indexing system

tools/
├── ast-cli/
│   ├── ast                 # Unified CLI tool
│   └── ...
└── legacy/                 # Archived original tools

workspace/
├── sandbox/                # Active experimentation
├── archive/                # Archived materials
└── ...                     # Current working documents
```

## Migration Guide

### For SQL Users
- **New projects**: Load `queries/ast_queries.sql`
- **Complex C++**: Also load `queries/legacy/ast-navigator.sql`  
- **Large scale**: Use `queries/legacy/ast-nav-parquet.sql`

### For CLI Users
- Use `./ast` (symlink to `tools/ast-cli/ast`)
- All commands now use the master query library

## Benefits
- **Clearer organization**: SQL libraries properly categorized
- **Reduced duplication**: Single source of truth for CLI tool
- **Better navigation**: Logical directory structure
- **Preserved functionality**: All valuable code retained in legacy/