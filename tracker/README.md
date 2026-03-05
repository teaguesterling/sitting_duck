# DuckDB AST Extension Tracker

This directory contains project tracking for bugs, features, and roadmap items.

## Directory Structure

```
tracker/
├── bugs/                      # Bug reports and issues
├── features/                  # Feature tracking
│   ├── planned/              # Accepted and planned features
│   └── completed/            # Completed features
├── roadmap/                  # Project roadmap
│   ├── short-term/           # Near-term goals
│   └── long-term/            # Long-term vision
├── PRIORITIES.md             # Priority breakdown
└── README.md                 # This file
```

## Current Status (Updated 2026-03-04)

### Language Support: 27 languages
Bash, C, C++, C#, CSS, Dart, Go, GraphQL, HCL, HTML, Java, JavaScript, JSON,
Kotlin, Lua, Markdown, PHP, Python, R, Ruby, Rust, SQL, Swift, TOML, TypeScript,
YAML, Zig

### Core Capabilities
- `read_ast()` - Parse files/globs into flattened AST tables (18 columns)
- `parse_ast()` - Parse source strings into AST tables
- `read_ast_objects()` - Parse files into AST struct format
- Semantic type system with cross-language type codes
- 50+ SQL macros for tree navigation, pattern matching, and analysis
- SQL-based pattern matching with wildcard captures (`ast_match()`)
- Source code extraction (`ast_get_source()`)
- Comprehensive semantic predicate functions (`is_function_definition()`, etc.)

### Test Suite: 75 test files
Covers all languages, pattern matching, semantic types, edge cases, and regressions.

## Bug Summary

| # | Title | Status |
|---|-------|--------|
| 001 | Clean API Not Loaded | Fixed |
| 002 | ensure_integer_array Bug | Fixed |
| 003 | Test Syntax Errors | Fixed |
| 004 | Performance Test Expectations | Fixed |
| 005 | AST v2 Macros Failing | Fixed |
| 006 | Performance Tests Slow | **Open** |
| 007 | Multiple SQL Statements | Fixed |
| 008 | Segfault Class Type Name Filter | Fixed |
| 009 | is_syntax_only Not Set for Delimiters | Fixed |
| 010 | Inconsistent Punctuation Types | Fixed |
| 011 | Comparison Semantic Type Mismatch | Fixed |
| 012 | Import Name Extraction | Fixed |
| 013 | FIND_IDENTIFIER Missing Types | Fixed |

**12 of 13 bugs fixed.** Only #006 (performance tests slow) remains open.

## Feature Summary

### Completed/Implemented
- #001 Basic AST reading
- #006 Pragma short names
- #012 Children/descendant counts (O(1) subtree ops)
- #019 Parser refactor (ownership, KIND taxonomy, SQL macros)
- Structural analysis macros (13 macros: children, descendants, ancestors, siblings, etc.)
- Analysis helpers v2 (call arguments, definitions, source extraction)
- SQL-based pattern matching (ast_match, ast_pattern, wildcard captures)
- Semantic predicate functions (40+ type-checking macros)
- File utilities (ast_get_source, ast_get_source_numbered)

### In Progress
- #023 Unified function architecture (Phase 1)

### Open / Planned (by priority)
- **P1**: ast_get_source with context (#013), parse-time filtering (#014), parent chain (#015), find references (#016), get calls (#017)
- **P2**: Native tree-sitter query API (#028), pattern by example (#029), pattern matching C++ (#030), columnar AST (#012-columnar)
- **P2**: Taxonomy exposure audit (#020), concise CLI syntax (#022), standardized extraction API (#026)
- **P3**: Native AST type (#011), AI agent UX (#010), performance caching, AST diff analysis

## Contributing
When adding new items:
1. Create a markdown file with a descriptive name
2. Include: Status, Priority, Estimated Effort, Description
3. Update status in the file as work progresses
4. Update this README when adding major items
