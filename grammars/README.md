# Tree-sitter Grammar Submodules

This directory contains git submodules for tree-sitter language grammars used by the duckdb_ast extension.

## Pre-generated Parsers

For CI builds, we use pre-generated parser files stored in `generated_parsers/`. This eliminates the need for tree-sitter CLI, Node.js, and Cargo in CI environments.

### Building with Pre-generated Parsers (Default)

When `generated_parsers/` exists, CMake automatically uses the pre-generated files:

```bash
make release  # Uses pre-generated parsers if available
```

### Regenerating Parsers Locally

If you modify grammars or patches, regenerate the parsers:

```bash
make generate-parsers   # Generate all parsers
make regenerate-parsers # Clean and regenerate
make clean-parsers      # Remove generated_parsers/
```

Requirements for regeneration:
- tree-sitter CLI (built automatically from `third_party/tree-sitter/` or install via `cargo install tree-sitter-cli`)
- Node.js (for grammar dependencies)

### Forcing Live Generation

To bypass pre-generated parsers and generate at build time:

```bash
cmake -DUSE_PREGENERATED_PARSERS=OFF ...
```

## Currently Included Grammars

| Language | Grammar | Has Scanner |
|----------|---------|-------------|
| Bash | tree-sitter-bash | Yes |
| C | tree-sitter-c | No |
| C++ | tree-sitter-cpp | Yes |
| C# | tree-sitter-c-sharp | Yes |
| CSS | tree-sitter-css | Yes |
| Go | tree-sitter-go | No |
| HTML | tree-sitter-html | Yes |
| Java | tree-sitter-java | No |
| JavaScript | tree-sitter-javascript | Yes |
| JSON | tree-sitter-json | No |
| Kotlin | tree-sitter-kotlin | Yes |
| Markdown | tree-sitter-markdown | Yes |
| PHP | tree-sitter-php/php | Yes |
| Python | tree-sitter-python | Yes |
| R | tree-sitter-r | Yes |
| Ruby | tree-sitter-ruby | Yes |
| Rust | tree-sitter-rust | Yes |
| SQL | tree-sitter-sql | Yes |
| Swift | tree-sitter-swift | Yes |
| TypeScript | tree-sitter-typescript/typescript | Yes |

## Adding New Grammars

1. Add the grammar as a submodule:

```bash
cd <project root>
git submodule add https://github.com/tree-sitter/tree-sitter-<language>.git grammars/tree-sitter-<language>
```

2. Update `CMakeLists.txt` to include the grammar's parser.c (and scanner.c if present) in EXTENSION_SOURCES.

3. Update `scripts/generate_all_parsers.sh` to include the new grammar in the GRAMMARS array.

4. Regenerate parsers and commit:

```bash
make regenerate-parsers
git add generated_parsers/tree-sitter-<language>/
git commit -m "Add <language> grammar support"
```

## Updating Grammars

To update all grammar submodules to their latest versions:

```bash
git submodule update --remote --merge
make regenerate-parsers  # Regenerate with updated grammars
```

## Patches

Some grammars require patches for compatibility. These are stored in `patches/` and applied automatically during parser generation. See `scripts/apply_grammar_patches.sh` for details.
