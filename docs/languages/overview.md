# Languages Overview

Sitting Duck supports **27 programming languages** with full semantic analysis.

## Supported Languages

| Category | Languages |
|----------|-----------|
| **Web** | JavaScript, TypeScript, HTML, CSS |
| **Systems** | C, C++, Go, Rust, Zig |
| **Scripting** | Python, Ruby, PHP, Lua, R, Bash |
| **Enterprise** | Java, C#, Kotlin, Swift |
| **Mobile** | Dart |
| **Infrastructure** | HCL (Terraform), JSON, TOML, GraphQL |
| **Documentation** | SQL, Markdown |

## Language Detection

Languages are automatically detected from file extensions:

| Extension(s) | Language |
|--------------|----------|
| `.py` | Python |
| `.js`, `.jsx` | JavaScript |
| `.ts`, `.tsx` | TypeScript |
| `.java` | Java |
| `.go` | Go |
| `.rs` | Rust |
| `.c`, `.h` | C |
| `.cpp`, `.hpp`, `.cc`, `.cxx` | C++ |
| `.cs` | C# |
| `.rb` | Ruby |
| `.php` | PHP |
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
| `.sh`, `.bash` | Bash |
| `.tf`, `.hcl`, `.tfvars` | HCL |
| `.graphql`, `.gql` | GraphQL |
| `.toml` | TOML |

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

- [Web Languages](web.md) - JavaScript, TypeScript, HTML, CSS
- [Systems Languages](systems.md) - C, C++, Go, Rust, Zig
- [Scripting Languages](scripting.md) - Python, Ruby, PHP, Lua, R, Bash
- [Enterprise Languages](enterprise.md) - Java, C#, Kotlin, Swift
- [Infrastructure Languages](infrastructure.md) - HCL, JSON, TOML, GraphQL
