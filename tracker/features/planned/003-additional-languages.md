# Support Additional Programming Languages

**Status**: Planned
**Priority**: High
**Estimated Effort**: Low-Medium per language

## Description
Extend the AST extension to support more programming languages beyond Python.

## Proposed Languages (Priority Order)
1. **JavaScript/TypeScript** - Very popular, good tree-sitter support
2. **C/C++** - Common systems languages, complex ASTs
3. **Java** - Enterprise applications
4. **Go** - Growing popularity, simple syntax
5. **Rust** - Systems programming
6. **SQL** - Meta! Parse SQL with DuckDB

## Implementation Plan
For each language:
1. Add tree-sitter-{language} as git submodule
2. Update CMakeLists.txt to include grammar files
3. Add language parser initialization in grammars.cpp
4. Create test files and update test suite
5. Document language-specific node types

## Technical Considerations
- Each language has different AST node types
- Some languages require external scanners
- Need to handle language-specific features (e.g., JSX for JavaScript)
- Consider creating language-specific helper functions