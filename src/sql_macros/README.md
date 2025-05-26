# SQL Macros Organization

This directory contains the SQL macro definitions for the DuckDB AST extension, organized by category.

## Current Structure

### Active Macros
- **core_macros.sql** - Basic AST operations (from v3)
- **source_macros.sql** - Source code reading and extraction (from v4_final)
- **structure_macros.sql** - Structure-preserving operations (to be implemented)
- **extract_macros.sql** - Type-converting extraction operations (to be implemented)
- **ai_macros.sql** - AI-friendly shortcuts and discovery (to be implemented)

### Loading Strategy

Currently, macros are loaded in two ways:
1. Hardcoded in `ast_sql_macros.cpp` (to be migrated)
2. SQL files loaded during extension initialization (to be implemented)

## Migration Plan

1. Extract hardcoded macros from C++ to `core_macros.sql`
2. Update `ast_sql_macros.cpp` to load SQL files
3. Implement new v2 API macros in separate files
4. Maintain backward compatibility through careful naming

## Naming Conventions

- `ast_*` - Structure-preserving (return AST objects)
- `ast_extract_*` - Type-converting (return SQL types)
- `ast_safe_*` - Safe variants (never return NULL)
- `ast_*_impl` - Internal implementation (not user-facing)