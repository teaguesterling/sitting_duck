# Test File Analysis for Minimal API

## Tests to KEEP (Worth Fixing)
- **edge_cases_and_errors.test** - Tests null safety, boundary conditions - ESSENTIAL
- **clean_api.test** - Tests our new minimal API - ESSENTIAL 
- **performance_tests.test** - Tests performance with large data - USEFUL (but slow)

## Tests to DISABLE (Test Removed Features)
- **comprehensive_chain_tests.test** - Tests chain methods we removed
- **entrypoint_api.test** - Tests ast() chaining we removed  
- **method_syntax_comprehensive.test** - Tests method syntax with removed functions
- **method_syntax.test** - Tests method syntax with removed functions
- **syntax_equivalence.test** - Tests equivalence between removed functions
- **dot_notation_macros.test** - Tests method syntax we don't fully support
- **dot_notation.test** - Tests method syntax we don't fully support

## Tests to UPDATE (Test Valid Concepts with Wrong Functions)
- **comprehensive_macros.test** - Tests basic functionality, just needs function name updates
- **comprehensive_macro_tests.test** - Tests basic functionality, just needs function name updates  
- **improved_macros.test** - Tests functionality we still have
- **sql_macros.test** - Tests basic macro functionality
- **ast_v2_macros.test** - Tests functionality we still support
- **ai_agent_scenarios.test** - Tests real-world usage patterns

## Next Steps:
1. Fix the 3 ESSENTIAL tests
2. Update the 6 tests that need function name fixes
3. Disable the 7 tests for removed features  
4. Consider if performance_tests.test should be moved to a separate slow test suite