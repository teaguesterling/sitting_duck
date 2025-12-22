# Bug: SQL Files with Multiple Statements Not Loading

**Status:** Fixed (Stale)
**Priority:** Critical
**Resolved:** 2025-12-22 - Functions are now loading correctly (90+ functions available)
**Discovered:** 2025-05-26

## Description
SQL macro files containing multiple CREATE statements are not being loaded properly. The DuckDB C++ API's Query() method appears to only execute the first statement when given multiple semicolon-separated statements.

## Evidence
- Total macros defined: 163
- Macros actually loaded: 42
- Clean API functions (ast_get_*, etc.) are not available despite being in embedded header

## Root Cause
The `RegisterASTSQLMacros()` function in ast_sql_macros.cpp executes each SQL file as a single Query() call, but files like clean_api_core.sql contain 14 CREATE statements.

## Solution Options
1. Modify C++ code to split SQL content by semicolons and execute each statement separately
2. Restructure SQL files to have one macro per file
3. Use a different API method that supports multiple statements

## Impact
- Clean API completely unavailable
- Short names feature unavailable
- Only single-statement SQL files are working