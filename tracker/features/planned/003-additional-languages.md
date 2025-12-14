# Support Additional Programming Languages

**Status**: In Progress
**Priority**: High
**Estimated Effort**: Low-Medium per language

## Description
Extend the AST extension to support more programming languages.

## Currently Supported (23 languages)
- bash, c, cpp, csharp, css, duckdb, go, graphql, hcl, html
- java, javascript, json, kotlin, lua, markdown, php, python
- r, ruby, rust, sql, swift, typescript

## Planned Languages (Priority Order)

### Tier 1 - High Priority
1. **TOML** - Ubiquitous config format (Cargo.toml, pyproject.toml, Hugo). Low effort, high utility.
2. **Zig** - Fastest growing systems language. Modern C replacement, strong community.
3. **Dart** - Flutter ecosystem for mobile/cross-platform. Large developer base.

### Tier 2 - Medium Priority
4. **Scala** - Big data ecosystem (Spark, Kafka). Enterprise presence.
5. **XML** - Maven, Android manifests, SOAP, config files. Unglamorous but practical.
6. **Elixir** - Distributed systems, passionate community.

### Tier 3 - Lower Priority
7. **Julia** - Scientific computing (Python + R cover this well)
8. **OCaml** - Niche but influential (compiler work, Rust origins)
9. **Haskell** - Academic/functional niche
10. **Perl** - Legacy maintenance, declining usage

## Known Issues
- **YAML** - Grammar exists but disabled due to complex self-modifying structure incompatible with tree-sitter CLI

## Implementation Plan
For each language:
1. Add tree-sitter-{language} as git submodule
2. Update generate_all_parsers.sh
3. Update CMakeLists.txt to include grammar files
4. Create {language}_types.def with semantic type mappings
5. Create {language}_adapter.cpp if custom extraction needed
6. Update language_adapter.hpp and language_adapter_registry_init.cpp
7. Create test files and update test suite

## Technical Considerations
- Each language has different AST node types
- Some languages require external scanners (may need patching)
- Languages using non-standard identifier nodes (like GraphQL's "name") need CUSTOM extraction strategy
- Consider creating language-specific helper functions
