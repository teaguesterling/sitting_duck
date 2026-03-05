# DuckDB AST Extension - Development Priorities

**Last Updated:** 2026-03-04

## Priority Levels

- **P0 (Critical)**: Blocking issues that prevent basic functionality
- **P1 (High)**: Important features or bugs affecting user experience
- **P2 (Medium)**: Nice-to-have features or minor bugs
- **P3 (Low)**: Future enhancements or cosmetic issues

## Current Status

### Completed (since last update)
- 27 language support (Bash, C, C++, C#, CSS, Dart, Go, GraphQL, HCL, HTML, Java, JavaScript, JSON, Kotlin, Lua, Markdown, PHP, Python, R, Ruby, Rust, SQL, Swift, TOML, TypeScript, YAML, Zig)
- Semantic type system with cross-language type codes
- 50+ SQL macros (tree navigation, pattern matching, analysis)
- SQL-based pattern matching with `ast_match()` / `ast_pattern()` / `ast_capture()`
- Wildcard capture system (named, anonymous, variadic with LIST return)
- Semantic predicates (40+ is_*() macros)
- Source extraction (`ast_get_source()`, `ast_get_source_numbered()`)
- Children/descendant counts for O(1) subtree operations
- Parser ownership refactor, KIND taxonomy integration
- 75 test files covering all languages and features
- Bugs #001-005, #007-013 all fixed (12 of 13 total)
- IS_SYNTAX_ONLY flags, punctuation consistency, comparison type fixes

### In Progress
- #023 Unified function architecture (Phase 1: unified backend)

### Only Open Bug
- #006 Performance tests slow (26s vs 0.1s for basic tests) - Medium priority

## Immediate Priorities

### P1 - Core Analysis Functions
1. **[FEATURE] Parse-Time Filtering** (#014)
   - Add `only_types`, `max_depth` parameters to read_ast
   - Essential for large codebases, 5-50x memory reduction

2. **[FEATURE] ast_get_parent_chain()** (#015)
   - Get all ancestors of a node up to root
   - Critical for scope understanding and qualified names

3. **[FEATURE] ast_find_references()** (#016)
   - Find all uses of a variable/function within scope
   - Core navigation feature

4. **[FEATURE] ast_get_calls()** (#017)
   - Extract function/method calls within a node
   - Essential for call graph analysis

### P1 - Architecture
1. **[FEATURE] Unified Function Architecture** (#023) - IN PROGRESS
   - Single parsing backend for all 5 AST functions
   - Enables method chaining and consistent behavior

### P2 - Pattern Matching Evolution
1. **[FEATURE] Native Tree-sitter Query API** (#028)
   - Expose tree-sitter S-expression queries via C++ (current impl is SQL macros)
   - Would be significantly faster for complex patterns

2. **[FEATURE] Pattern by Example** (#029)
   - Write patterns as actual code with `__WILDCARD__` placeholders

3. **[FEATURE] Pattern Matching C++ Implementation** (#030)
   - Move pattern engine from SQL to C++ for performance

### P2 - Language & Extraction
1. **[FEATURE] Missing Native Extractors** (#027)
   - Add native extractors for 9 languages (C#, Lua, HCL, Zig, etc.)

2. **[FEATURE] Standardized Semantic Extraction API** (#026)
   - Universal SQL functions: extract_functions, extract_classes, etc.

3. **[FEATURE] Taxonomy Exposure Audit** (#020)
   - Audit which functions expose kind/universal_flags/semantic_id fields

## Medium-term Priorities

### P2 - Performance & Architecture
1. **[FEATURE] Columnar AST Implementation** (#012-columnar)
   - Parallel array representation for 10-100x faster type filtering

2. **[FEATURE] Performance Optimization Roadmap** (#009)
   - Three-tier strategy: C++ hot-path, structural, architectural

3. **[BUG] Performance Tests Slow** (#006)
   - 26 seconds is too long; optimize or split test suite

### P2 - User Experience
1. **[FEATURE] Concise CLI Syntax** (#022)
   - Streamlined CLI for AST queries with method chaining

2. **[FEATURE] Self-Analysis Dogfooding** (#021)
   - Use extension to analyze its own C++ codebase

3. **[FEATURE] Native DuckDB Parser Integration** (#024)
   - Use DuckDB's own parser for true SQL semantic analysis

## Long-term Priorities

### P3 - Future Enhancements
1. **[FEATURE] Native AST Type** (#011) - Custom DuckDB type
2. **[FEATURE] AI Agent UX Optimization** (#010) - Simplified entry points
3. **[FEATURE] AST Diff Analysis** (planned/005) - Code evolution
4. **[FEATURE] Performance Caching** (planned/008) - Session-level AST cache
5. **[FEATURE] Error Context Tracking** (planned/007) - Enhanced error messages
6. **[FEATURE] Extended Wildcard Rules** (#031) - HTML/XML DSL for constraints
7. **[FEATURE] Smart Pointer Refactor** (#018) - Memory safety improvement
8. **[FEATURE] Grammar Adapter Refactor** (#025) - Consolidate language handling

## Success Metrics

1. **Performance**: <100ms query on 100K node files, 10x improvement in type filtering
2. **Usability**: Chainable API, clear documentation, intuitive macros
3. **Reliability**: All 27 languages working, zero regressions, comprehensive error handling
4. **Extensibility**: User-defined patterns, community language handlers
