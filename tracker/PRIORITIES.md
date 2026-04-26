# DuckDB AST Extension - Development Priorities

**Last Updated:** 2026-04-26

## Priority Levels

- **P0 (Critical)**: Blocking issues that prevent basic functionality
- **P1 (High)**: Important features or bugs affecting user experience
- **P2 (Medium)**: Nice-to-have features or minor bugs
- **P3 (Low)**: Future enhancements or cosmetic issues

## Current Status

### Completed (since last update on 2026-04-26)
- `ast_find_references()` with scope-chain resolution (#016) — finds all definition/reference/call sites for a symbol, resolves through scope chain, tested across Python, JS, Rust, Go (200 assertions)
- `ast_get_calls()` and `ast_call_graph()` (#017) — scope-aware call extraction with call-type classification (function/method/constructor/macro), O(1) caller attribution via scope.function, tested across Python, JS, Rust, Go (392 assertions)
- Async/modifier extraction across all 8 languages (PR #69) — `list_contains(modifiers, 'async')` now works for Python, JS, TS, Rust, Kotlin, Swift, Dart, C#
- `ast_imports`/`ast_exports` correctness for cross-file resolution (PR #68) — IS_EXPORTED flag gated to name-binding nodes, import dedup, 5-language adapter fixes
- IS_EXPORTED flag for file-level visibility (PR #65) — scope-aware export detection
- Custom selector predicates via `ast_selector_predicate_*` macros (PR #67)
- CSS selector bug fixes: `:has(.class)`, `:not(:has())`, combinator+pseudo-class, compound filter leaks, decorrelated join performance (17x speedup)
- Recursive pattern matching (#57) — `<**>` any-depth wildcards, relational operators (`ast_has`, `ast_not_has`, `ast_inside`, `ast_precedes`, `ast_follows`)
- Optional `<?>` and negation `<~>` wildcards (#58, #59) — complete wildcard cardinality system
- `detect_language()` scalar function (#56) — expose language detection for downstream tools
- `qualified_name` column with scoped definition paths (PR #55) — e.g., `MyClass.method`
- `ast_definition_parent` macro (PR #53) — nearest definition ancestor resolution
- Native extractor fixes for Java, Swift, PHP (PR #54) — return types, parameters, modifiers
- Bash semantic refinements and refinement tests for 8 languages (PR #42)
- File-path macro signature migration (PRs #48, #39) — whole-file macros now take source path
- SQL CREATE FUNCTION name extraction (PR #50)
- Pattern matching: variadic capture, same-name constraints, bug fixes (PR #47)
- NULL name extraction fixes across C#, HCL, Dart, C++ (PR #46)
- Bugs #009 (is_syntax_only for delimiters), #010 (punctuation consistency), #011 (comparison types) — all fixed in PR #43

### Previously Completed
- 27 language support (Bash, C, C++, C#, CSS, Dart, Go, GraphQL, HCL, HTML, Java, JavaScript, JSON, Kotlin, Lua, Markdown, PHP, Python, R, Ruby, Rust, SQL, Swift, TOML, TypeScript, YAML, Zig)
- Semantic type system with cross-language type codes
- 50+ SQL macros (tree navigation, pattern matching, analysis)
- SQL-based pattern matching with `ast_match()` / `ast_pattern()` / `ast_capture()`
- Wildcard capture system (named, anonymous, variadic with LIST return, `<*>` `<+>` `<?>` `<~>` `<**>`)
- Relational operators (`ast_has`, `ast_not_has`, `ast_inside`, `ast_precedes`, `ast_follows`)
- Semantic predicates (40+ is_*() macros)
- Source extraction (`ast_get_source()`, `ast_get_source_numbered()`, `ast_source_of()`)
- Children/descendant counts for O(1) subtree operations
- Parser ownership refactor, KIND taxonomy integration
- 87 test files covering all languages and features
- Bugs #001-005, #007-013 all fixed
- IS_SYNTAX_ONLY flags, punctuation consistency, comparison type fixes
- 106 test files, 6049 assertions

### Stalled
- #023 Unified function architecture — no activity since January 2026, deprioritized

### Open Bugs
- #006 Performance tests slow (26s vs 0.1s for basic tests) - Medium priority
- #010 C# name extraction picks up return type instead of method name - Medium priority

## Immediate Priorities

### P1 - Core Analysis Functions
1. **[FEATURE] Parse-Time Filtering** (#014)
   - Add `only_types`, `max_depth` parameters to read_ast
   - Essential for large codebases, 5-50x memory reduction

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
   - Add native extractors for remaining languages (C#, Lua, HCL, Zig, etc.)
   - Java, Swift, PHP extractors fixed in PR #54

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
6. **[FEATURE] Extended Wildcard Rules** (#031) - HTML/XML DSL for constraints (Phase 1 modifiers done: `*`, `+`, `?`, `~`, `**`; Phase 2 constraint attributes remain)
7. **[FEATURE] Smart Pointer Refactor** (#018) - Memory safety improvement
8. **[FEATURE] Grammar Adapter Refactor** (#025) - Consolidate language handling

## Success Metrics

1. **Performance**: <100ms query on 100K node files, 10x improvement in type filtering
2. **Usability**: Chainable API, clear documentation, intuitive macros
3. **Reliability**: All 27 languages working, zero regressions, comprehensive error handling
4. **Extensibility**: User-defined patterns, community language handlers
