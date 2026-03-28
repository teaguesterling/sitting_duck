# Bug: Clean API Functions Not Loaded

**Status:** Fixed
**Priority:** Critical
**Discovered:** 2025-05-26

## Description
The clean API functions (ast_get_*, ast_filter_*, ast_nav_*, ast_analyze_*) are not being loaded because the new SQL files are not included in CMakeLists.txt.

## Root Cause
The following files need to be added to CMakeLists.txt:
- clean_api_core.sql
- clean_api_analysis.sql  
- clean_api_chains.sql
- entrypoint_macros.sql (also missing)

## Fix
Update src/sql_macros/CMakeLists.txt to include all SQL macro files.

## Impact
- All clean API tests fail
- New API is completely unavailable