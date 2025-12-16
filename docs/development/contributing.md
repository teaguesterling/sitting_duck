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

# Build
make

# Run tests
make test
```

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
