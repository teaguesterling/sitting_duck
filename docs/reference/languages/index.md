# Language Node Type Reference

This reference documents all AST node types supported by Sitting Duck,
organized by programming language. Each page shows:

- **Node Type**: The tree-sitter node type string
- **Semantic Type**: Universal semantic classification
- **Name Extraction**: Strategy for extracting identifiers
- **Description**: What the node represents

!!! warning "Five pages here document languages that are not built in"

    `read_ast` / `parse_ast` accept **27** languages — 26 Tree-sitter grammars plus
    the native `duckdb` adapter. The authoritative list is
    `cmake/BuiltinLanguages.cmake`, and `SELECT language FROM
    ast_supported_languages()` reports what your build has.

    **[YAML](yaml.md), [Scala](scala.md), [F#](fsharp.md), [Haskell](haskell.md)
    and [Julia](julia.md) are not among them.** Each has a semantic-type mapping
    (`src/language_configs/<lang>_types.def`) and a page here, but no adapter is
    built, so they error with "Unsupported language". YAML was deliberately
    disabled — its Tree-sitter grammar is incompatible with the parser-generation
    CLI; the other four were never wired up. Those pages are design references.

    Conversely, two **supported** languages have no page here: `sql` (the
    Tree-sitter SQL grammar) and `duckdb` (DuckDB's own parser, no grammar).

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
