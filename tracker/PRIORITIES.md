# DuckDB AST Extension - Development Priorities

**Last Updated:** 2025-05-26

## Critical (P0) - Blocking Issues
1. **[BUG] Clean API Not Loaded** - New SQL macro files not included in build
   - Blocks all clean API functionality
   - Fix: Update CMakeLists.txt

## High Priority (P1) - Core Functionality
1. **[BUG] Test Syntax Errors** - Multiple tests have syntax issues
   - Fix alias/subquery issues
   - Add missing query declarations
   
2. **[FEATURE] Pragma Short Names** - Register unprefixed aliases
   - Improves usability significantly
   - Easy to implement
   
3. **[FEATURE] Error Context Tracking** - Enhanced error messages
   - Critical for debugging chained operations
   - Helps users and AI agents

## Medium Priority (P2) - Quality & Performance
1. **[BUG] ensure_integer_array** - Wrong behavior with [NULL]
   - Inconsistent with ensure_varchar_array
   
2. **[BUG] Performance Test Expectations** - Wrong expected values
   - Simple fix to update expectations
   
3. **[FEATURE] Performance Caching** - Session-level AST cache
   - Major performance improvement
   - Useful for interactive sessions

## Low Priority (P3) - Nice to Have
1. **[BUG] AST v2 Macros Test** - Investigate failure
   - May be obsolete with clean API

## Implementation Order

### Phase 1: Fix Critical Issues (Today)
1. Fix CMakeLists.txt to include clean API files
2. Fix test syntax errors
3. Update test expectations

### Phase 2: Quick Wins (This Week)
1. Implement pragma/macro for short names
2. Fix ensure_integer_array bug

### Phase 3: Enhanced UX (Next Week)
1. Add error context tracking
2. Create performance caching helpers

### Phase 4: Cleanup
1. Remove obsolete tests/functions
2. Update documentation