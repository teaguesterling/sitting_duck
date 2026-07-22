# Contributing

Guidelines for contributing to Sitting Duck.

## Getting Started

### Prerequisites

- Git
- CMake 3.20+
- C++ compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- Node.js (for tree-sitter-cli)

### Clone and Build

```bash
# Clone with submodules
git clone --recursive https://github.com/teaguesterling/sitting_duck.git
cd sitting_duck

# Build. A bare `make` is single-threaded (it runs `cmake --build` without -j).
# Use all cores by setting the cmake parallel level:
CMAKE_BUILD_PARALLEL_LEVEL=$(nproc) make
# (or `GEN=ninja make` if ninja is installed). vcpkg is not required — vcpkg.json has no deps.

# Run tests
make test
```

### Building a Language Subset

Every built-in language (tree-sitter grammar + adapter + semantic-type config)
is a build-time module. The default build compiles all of them; two CMake cache
variables select a subset:

```bash
# Only these languages (comma or semicolon separated; semicolons need quoting)
EXT_FLAGS='-DSITTING_DUCK_LANGUAGES=python,cpp' make release

# Or subtract from the full set
EXT_FLAGS='-DSITTING_DUCK_EXCLUDE_LANGUAGES=dart,swift' make release
```

Unknown language names fail the configure step with the list of valid names.
The per-language wiring lives in one place, `cmake/BuiltinLanguages.cmake`,
which generates the registration table consumed by
`src/language_adapter_registry_init.cpp`.

A compiled-out language behaves exactly like a language sitting_duck never
supported: `detect_language()` misses its extensions, glob expansion skips its
files, and naming it explicitly raises the usual `Unsupported language` error.
`ast_supported_languages()` reflects only what was compiled in. (Once runtime
grammar registration lands, a compiled-out language's name is deliberately free
for runtime registration — built-ins only shadow names they actually contain.)

Subset builds exist as groundwork for grammar packs and the slim
"sitting_duckling" distribution — see
[issue #87](https://github.com/teaguesterling/sitting_duck/issues/87).
`scripts/test_language_subset.sh` smoke-tests a `python;cpp` build (run in CI by
the `language-subset-build` job). Note that the SQL test suite assumes the full
set; run it against a default build.

## Development Workflow

### Making Changes

1. Create a feature branch
2. Make changes
3. Run tests
4. Submit pull request

### Running Tests

```bash
# All tests
make test

# Specific test file
./build/release/unittest "test/sql/your_test.test"
```

### Code Style

- Follow existing code patterns
- Use descriptive variable names
- Add comments for complex logic
- Keep functions focused

## Adding Language Support

See [Adding Languages](adding-languages.md) for the complete guide.

Quick steps:
1. Add grammar submodule
2. Update CMakeLists.txt
3. Create adapter in `src/language_adapters/`
4. Create type definitions in `src/language_configs/`
5. Register in `src/language_adapter_registry_init.cpp`
6. Add tests

## Test Guidelines

### SQL Logic Tests

Create tests in `test/sql/` using DuckDB's SQLLogicTest format:

```sql
# name: test/sql/my_feature.test
# description: Test my new feature
# group: [sitting_duck]

require sitting_duck

statement ok
LOAD sitting_duck;

query I
SELECT COUNT(*) > 0 FROM read_ast('test/data/example.py');
----
true
```

### Test Data

Add sample files in `test/data/<language>/`:
- `simple.<ext>` - Basic constructs
- Additional files as needed

## Documentation

### Updating Docs

- User documentation: `docs/`
- API reference: `API_REFERENCE.md`
- AI agent guide: `AI_AGENT_GUIDE.md`

### Building Docs Locally

```bash
# Install dependencies
pip install -r docs/requirements.txt

# Build docs
mkdocs serve

# View at http://localhost:8000
```

## Pull Request Guidelines

### Before Submitting

- [ ] Tests pass (`make test`)
- [ ] Code follows style guidelines
- [ ] Documentation updated if needed
- [ ] Commit messages are clear

### PR Description

Include:
- What the change does
- Why it's needed
- How to test it

### Review Process

1. Automated tests run
2. Code review by maintainers
3. Address feedback
4. Merge when approved

## Reporting Issues

### Bug Reports

Include:
- DuckDB version
- Sitting Duck version
- Steps to reproduce
- Expected vs actual behavior
- Error messages

### Feature Requests

Include:
- Use case description
- Proposed solution
- Alternative approaches considered

## Community

- GitHub Issues: Bug reports and feature requests
- Pull Requests: Code contributions
- Discussions: Questions and ideas

## License

Contributions are licensed under the MIT License.
