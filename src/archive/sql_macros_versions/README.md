# Archived SQL Macro Versions

This directory contains historical versions of SQL macro definitions that were used during development.

## Version History

- **ast_sql_macros.sql** - Original basic macros
- **ast_sql_macros_v2.sql** - Added more helper functions
- **ast_sql_macros_v3.sql** - Current production version (with type casting fixes)
- **ast_sql_macros_v4_source.sql** - Initial source reading implementation (with DEFAULT syntax)
- **ast_sql_macros_v4_source_final.sql** - Final source reading implementation (with := syntax)

## Current Status

As of the latest version, SQL macros are loaded from:
1. **ast_sql_macros.cpp** - Hardcoded macros loaded by C++
2. **ast_sql_macros_v3.sql** - Should be loaded for additional macros
3. **ast_sql_macros_v4_source_final.sql** - New source reading macros (to be integrated)

## Future Work

The plan is to:
1. Move all hardcoded macros from C++ to SQL files
2. Load SQL files dynamically during extension initialization
3. Organize macros by category (filtering, extraction, navigation, etc.)