# Installation

This guide covers how to install Sitting Duck for use with DuckDB.

## Prerequisites

- **DuckDB**: Version 1.0.0 or later
- **Git**: For cloning the repository
- **CMake**: Version 3.20 or later (for building from source)
- **C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+

## Building from Source

Sitting Duck must currently be built from source. We plan to publish to the DuckDB extension repository in the future.

### Clone the Repository

```bash
# Clone with submodules (required for Tree-Sitter grammars)
git clone --recursive https://github.com/teaguesterling/sitting_duck.git
cd sitting_duck
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

### Build the Extension

```bash
# Build with make (recommended)
make

# Or use CMake directly
mkdir -p build/release
cd build/release
cmake ../..
make -j$(nproc)
```

### Verify Installation

```bash
# Test the extension loads correctly
./build/release/duckdb -c "
LOAD './build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
SELECT COUNT(*) FROM read_ast('README.md');
"
```

## Loading the Extension

### From Build Directory

```sql
-- Load from build directory
LOAD './build/release/extension/sitting_duck/sitting_duck.duckdb_extension';

-- Verify it loaded
SELECT * FROM ast_supported_languages() LIMIT 5;
```

### Installing to DuckDB Extensions Directory

```bash
# Copy to DuckDB extensions directory
cp build/release/extension/sitting_duck/sitting_duck.duckdb_extension ~/.duckdb/extensions/

# Now you can load without the full path
duckdb -c "LOAD sitting_duck; SELECT COUNT(*) FROM read_ast('README.md');"
```

## Build Options

### Debug Build

```bash
make debug
# Or
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make
```

### Release with Debug Info

```bash
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ../..
make
```

## Troubleshooting

### Submodule Issues

If grammar submodules show errors:

```bash
# Reset and reinitialize submodules
git submodule deinit -f .
git submodule update --init --recursive
```

### Build Errors

**Missing Tree-sitter headers:**
```bash
# Headers are auto-installed during CMake configuration
rm -rf build && make
```

**Grammar generation failures:**
```bash
# Ensure Node.js is available for tree-sitter-cli
npm install -g tree-sitter-cli
make clean && make
```

### Runtime Errors

**Extension not found:**
```sql
-- Use the full path to the extension
LOAD '/full/path/to/sitting_duck.duckdb_extension';
```

**Language not recognized:**
```sql
-- Check available languages
SELECT * FROM ast_supported_languages();

-- Force a specific language
SELECT * FROM read_ast('file.txt', 'python');
```

## Next Steps

- [Quick Start Guide](quickstart.md) - Your first Sitting Duck queries
- [Basic Usage](basic-usage.md) - Common usage patterns
