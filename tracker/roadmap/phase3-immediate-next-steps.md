# Phase 3: Immediate Next Steps

**Status:** Mostly Complete (as of 2026-03-28)

## ~~Priority 1: Complete Language Handler Implementations~~ DONE
All 27 languages now have full handler implementations with taxonomy support, name extraction, and node type normalization. Native extractors improved for Java, Swift, PHP in PR #54.

## ~~Priority 2: Test & Validate Core Functionality~~ DONE
87 test files with 4374 assertions covering:
- Cross-language function finding
- Subtree extraction correctness
- Edge cases (empty files, syntax errors)
- Native extraction per-language tests
- Pattern matching

Performance testing exists but is slow (#006).

## ~~Priority 3: Implement "Find Function X" Demo~~ DONE
Implemented via:
- `ast_source_of(source, name)` macro for finding definitions by name
- `ast_definitions(source)` for listing all definitions
- Multi-file glob support in `read_ast('src/**/*.py')`
- `qualified_name` column for scoped paths (e.g., `C:MyClass/F:method`)

## Priority 4: AST Struct Upgrade — DEFERRED
Nested AST struct format (`read_ast_objects`) deferred. Current flat table approach is working well and is more SQL-friendly. May revisit if #023 (unified architecture) resumes.

## ~~Priority 5: Documentation & Examples~~ DONE
- AI_AGENT_GUIDE.md — comprehensive task-oriented skill guide
- KIND taxonomy documented
- SQL macro documentation in source files
- Test files serve as usage examples

## Remaining Future Work
- Global AST index across entire codebase (see roadmap/PARQUET_INDEXING.md)
- Integration with git for temporal analysis (see roadmap/GIT_AWARE_AST_DATABASE.md)
- Query optimization for large-scale analysis (see features/014-filter-at-parse-time.md)
