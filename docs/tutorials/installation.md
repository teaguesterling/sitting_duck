# Installation

## From the DuckDB Community Registry

The simplest way to install:

```sql
INSTALL sitting_duck FROM community;
LOAD sitting_duck;

-- Verify it works
SELECT * FROM ast_supported_languages() LIMIT 5;
```

This works on Linux (x64, ARM64), macOS (Intel, Apple Silicon), Windows, and DuckDB-Wasm.

## Building from Source

For development or if you need the latest unreleased features.

### Prerequisites

- **DuckDB**: v1.5.1 or later (the extension is compiled against a specific DuckDB API version)
- **Git**: for cloning the repository
- **CMake**: 3.20 or later
- **C++ compiler**: GCC 9+, Clang 10+, or MSVC 2019+
- **Node.js**: required for tree-sitter grammar generation

### Build

```bash
git clone --recursive https://github.com/teaguesterling/sitting_duck.git
cd sitting_duck
make
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

### Verify

```bash
./build/release/duckdb -c "
LOAD 'build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
SELECT COUNT(*) FROM read_ast('README.md');
"
```

### Load from build directory

```sql
LOAD 'build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
```

## Troubleshooting

**Submodule issues:**
```bash
git submodule deinit -f .
git submodule update --init --recursive
```

**Missing tree-sitter headers:**
```bash
rm -rf build && make
```

**Grammar generation failures** (Node.js needed):
```bash
npm install -g tree-sitter-cli
make clean && make
```

**Extension not found at runtime:**
```sql
LOAD '/full/path/to/sitting_duck.duckdb_extension';
```

## Next Steps

- [Your First Query](quickstart.md) — start querying code
