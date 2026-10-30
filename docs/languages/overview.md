# Languages Overview

Sitting Duck supports **28 programming languages** with full semantic analysis.

## Supported Languages

| Category | Languages |
|----------|-----------|
| **Web** | JavaScript, TypeScript, HTML, CSS |
| **Systems** | C, C++, Go, Rust, Zig |
| **Scripting** | Python, Ruby, PHP, Lua, R, Bash |
| **Enterprise** | Java, C#, Kotlin, Swift |
| **Mobile** | Dart |
| **Infrastructure** | HCL (Terraform), JSON, TOML, YAML, GraphQL |
| **Documentation** | SQL, DuckDB SQL, Markdown |

## Language Detection

Languages are automatically detected from file extensions:

| Extension(s) | Language |
|--------------|----------|
| `.py`, `.pyi`, `.pyw` | Python |
| `.js`, `.jsx`, `.mjs` | JavaScript |
| `.ts`, `.tsx` | TypeScript |
| `.java` | Java |
| `.go` | Go |
| `.rs`, `.rlib` | Rust |
| `.c`, `.h` | C |
| `.cpp`, `.cc`, `.cxx`, `.c++`, `.hpp`, `.hh`, `.hxx`, `.h++` | C++ |
| `.cs` | C# |
| `.rb`, `.ruby` | Ruby |
| `.php`, `.php3`, `.php4`, `.php5`, `.phtml` | PHP |
| `.swift` | Swift |
| `.kt`, `.kts` | Kotlin |
| `.lua` | Lua |
| `.r`, `.R` | R |
| `.dart` | Dart |
| `.zig` | Zig |
| `.sql` | SQL |
| `.md`, `.markdown` | Markdown |
| `.html`, `.htm` | HTML |
| `.css` | CSS |
| `.json` | JSON |
| `.yaml`, `.yml` | YAML |
| `.toml` | TOML |
| `.sh`, `.bash`, `.zsh` | Bash |
| `.hcl`, `.tf`, `.tfvars` | HCL |
| `.graphql`, `.gql` | GraphQL |

The 28th language, `duckdb`, has no file-extension mapping: it is the DuckDB SQL
dialect, reachable through `parse_ast(code, 'duckdb')` rather than through
`read_ast()` detection.

## Query Available Languages

```sql
SELECT language, extensions
FROM ast_supported_languages()
ORDER BY language;
```

## Language Features

All languages include:

- **Tree-sitter parsing** - Fast, accurate parsing with error recovery
- **Semantic type mapping** - Universal semantic categories
- **Name extraction** - Identifier and symbol extraction
- **Native context** - Language-specific semantic analysis

### Semantic Refinements

Most languages include semantic refinements:

| Refinement | Description |
|------------|-------------|
| Function types | Regular, lambda, constructor, getter/setter, async |
| Variable types | Mutable, immutable, parameter, field |
| Class types | Regular, abstract, enum, struct |
| Control flow | Binary, multiway, ternary, iterator, conditional |

## Override Language Detection

```sql
-- Force a specific language
SELECT * FROM read_ast('script.txt', 'python');

-- Parse as different language
SELECT * FROM read_ast('Makefile', 'bash');
```

## Language-Specific Details

Each category doc includes:
- **Extraction Quality Ratings** - Star ratings for functions, classes, calls, variables, body detection
- **Implementation Notes** - How extraction works for each language
- **Known Limitations** - Current gaps and workarounds
- **Examples** - SQL queries for common tasks

| Category | Doc | Languages |
|----------|-----|-----------|
| Web | [web.md](web.md) | JavaScript, TypeScript, HTML, CSS |
| Systems | [systems.md](systems.md) | C, C++, Go, Rust, Zig |
| Scripting | [scripting.md](scripting.md) | Python, Ruby, PHP, Lua, R, Bash |
| Enterprise & Mobile | [enterprise.md](enterprise.md) | Java, C#, Kotlin, Swift, Dart |
| Infrastructure | [infrastructure.md](infrastructure.md) | HCL, JSON, TOML, GraphQL |

See also: [Native Extraction Semantics](../explanation/native-extraction-semantics.md) for detailed field semantics across languages.
