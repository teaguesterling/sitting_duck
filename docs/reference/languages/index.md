# Language Node Type Reference

This reference documents all AST node types supported by Sitting Duck,
organized by programming language. Each page shows:

- **Node Type**: The tree-sitter node type string
- **Semantic Type**: Universal semantic classification
- **Name Extraction**: Strategy for extracting identifiers
- **Description**: What the node represents

!!! warning "4 pages here document languages that are not built in"

    `read_ast` / `parse_ast` accept **28** languages. The
    authoritative list is `cmake/BuiltinLanguages.cmake`, which this page
    is generated from, so neither the count nor the list below can drift
    from the build. `SELECT language FROM ast_supported_languages()`
    reports what your build actually has.

    **[Fsharp](fsharp.md), [Haskell](haskell.md), [Julia](julia.md), [Scala](scala.md) are not among them.** Each has a
    semantic-type mapping (`src/language_configs/<lang>_types.def`) and a
    page here, but no adapter is built, so they error with "Unsupported
    language". Those pages are design references.

    Conversely, 2 **supported** languages have no page here: `duckdb`, `sql` (no `_types.def` to generate one from).

## Languages

### Web

- [CSS](css.md)
- [HTML](html.md)
- [JavaScript](javascript.md)
- [TypeScript](typescript.md)

### Systems

- [C](c.md)
- [C++](cpp.md)
- [Go](go.md)
- [Rust](rust.md)
- [Zig](zig.md)

### Scripting

- [Bash](bash.md)
- [Lua](lua.md)
- [PHP](php.md)
- [Python](python.md)
- [R](r.md)
- [Ruby](ruby.md)

### Enterprise & Mobile

- [C#](csharp.md)
- [Dart](dart.md)
- [Java](java.md)
- [Kotlin](kotlin.md)
- [Scala](scala.md)
- [Swift](swift.md)

### Infrastructure

- [GraphQL](graphql.md)
- [HCL (Terraform)](hcl.md)
- [JSON](json.md)
- [TOML](toml.md)
- [YAML](yaml.md)

### Documentation

- [Markdown](markdown.md)

### Functional

- [F#](fsharp.md)
- [Haskell](haskell.md)

### Scientific

- [Julia](julia.md)

---

*This documentation is auto-generated from the `.def` files in `src/language_configs/`*
