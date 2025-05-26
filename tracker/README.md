# DuckDB AST Extension Tracker

This directory contains our project tracking for bugs, features, and roadmap items.

## Directory Structure

```
tracker/
├── bugs/                      # Bug reports and issues
├── features/                  # Feature tracking
│   ├── in-progress/          # Currently being worked on
│   ├── planned/              # Accepted and planned features
│   └── completed/            # Completed features
├── roadmap/                  # Project roadmap
│   ├── short-term/           # Next 1-2 months
│   └── long-term/            # 6-12 month vision
└── README.md                 # This file
```

## Current Status

### Completed Features
- ✅ Basic AST reading with `read_ast()` function
- ✅ Python language support
- ✅ Name extraction for functions, classes, and identifiers
- ✅ Comprehensive test suite

### Planned Features (Priority Order)
1. 🔴 **High**: Additional language support (JavaScript, C/C++)
2. 🟡 **Medium**: AST type and `read_ast_objects()` function
3. 🟡 **Medium**: AST diff and code evolution analysis
4. 🟢 **Low**: Helper view functions (ast_functions, ast_classes)

### Short-term Goals (Next 1-2 months)
- Support 3+ programming languages
- Performance optimizations for large codebases
- Enhanced name extraction capabilities
- Comprehensive documentation

### Long-term Vision (6-12 months)
- Premier SQL-based code analysis tool
- 10+ language support
- Multi-repository analysis
- Integration with development tools
- Sub-second queries on million-line codebases

## Contributing
When adding new items:
1. Create a markdown file with a descriptive name
2. Include: Status, Priority, Estimated Effort, Description
3. Move items between directories as status changes
4. Update this README when adding major items