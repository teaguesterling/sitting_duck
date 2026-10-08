# Support Additional Programming Languages

**Status**: In Progress
**Priority**: High
**Estimated Effort**: Low-Medium per language

## Description
Extend the AST extension to support more programming languages.

## Currently Supported (27 languages, as of 2026-10-08)
- bash, c, cpp, csharp, css, dart, duckdb, go, graphql, hcl, html
- java, javascript, json, kotlin, lua, markdown, php, python
- r, ruby, rust, sql, swift, toml, typescript, zig

26 tree-sitter grammars plus native `duckdb`. Authoritative list:
`cmake/BuiltinLanguages.cmake`. Note that `yaml`, `scala`, `fsharp`, `haskell` and
`julia` have `src/language_configs/*_types.def` files and docs pages but are **not**
built — do not read those as support.

## Planned Languages (Priority Order)

### Tier 1 - ~~High Priority~~ — all three DONE
1. ~~**TOML**~~ - Ubiquitous config format (Cargo.toml, pyproject.toml, Hugo). **Shipped.**
2. ~~**Zig**~~ - Fastest growing systems language. Modern C replacement. **Shipped.**
3. ~~**Dart**~~ - Flutter ecosystem for mobile/cross-platform. **Shipped.**

### Tier 2 - Medium Priority
4. **Scala** - Big data ecosystem (Spark, Kafka). Enterprise presence. *Partially
   started: `scala_types.def` and a docs page exist, but there is no adapter class and
   no `cmake/BuiltinLanguages.cmake` declaration, so it does not work.*
5. **XML** - Maven, Android manifests, SOAP, config files. Unglamorous but practical.
6. **Elixir** - Distributed systems, passionate community.

### Tier 3 - Lower Priority
7. **Julia** - Scientific computing (Python + R cover this well). *Partially started:
   `.def` + docs page, no adapter — does not work.*
8. **OCaml** - Niche but influential (compiler work, Rust origins)
9. **Haskell** - Academic/functional niche. *Partially started: `.def` + docs page, no
   adapter — does not work.*
10. **Perl** - Legacy maintenance, declining usage
11. **F#** - *Partially started: `.def` + docs page, no adapter — does not work. Not
    previously listed here despite the half-done state.*

**Note on the four "partially started" entries** (scala, julia, haskell, fsharp): each
has a `src/language_configs/<lang>_types.def`, a `src/language_configs/unparse/<lang>_unparse.def`
and a `docs/reference/languages/<lang>.md`, but no adapter class in
`src/include/language_adapter.hpp` and no `sitting_duck_language(...)` declaration. The
semantic mapping work is done; the grammar submodule, adapter and build wiring are not.
Finishing one is therefore cheaper than starting from scratch — see
`docs/development/adding-languages.md`.

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
