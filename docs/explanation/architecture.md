# Architecture

Overview of Sitting Duck's internal architecture.

## System Components

```
┌─────────────────────────────────────────────────────────────┐
│                      DuckDB Interface                       │
│  read_ast() / parse_ast() / ast_supported_languages()       │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   Unified AST Backend                        │
│  File handling, streaming, context extraction               │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                  Language Adapter Registry                   │
│  Language detection, adapter dispatch                        │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Language Adapters                         │
│  Python, JavaScript, Java, Go, Rust, C++, ...               │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Tree-sitter Parsers                       │
│  Grammar-specific parsing                                    │
└─────────────────────────────────────────────────────────────┘
```

## Key Components

### Unified AST Backend

**Location:** `src/unified_ast_backend.cpp`

Handles:
- File reading and glob expansion
- Multi-file processing
- Streaming output chunks
- Context extraction levels
- Parameter validation

### Language Adapter Registry

**Location:** `src/language_adapter_registry_init.cpp`

Handles:
- Language registration
- File extension mapping
- Adapter factory management
- Language detection

### Language Adapters

**Location:** `src/language_adapters/`

Each language has an adapter that provides:
- Tree-sitter parser initialization
- Semantic type mapping
- Name extraction strategies
- Native context extraction

### Type Definitions

**Location:** `src/language_configs/`

`.def` files containing:
- Node type to semantic type mappings
- Name extraction strategies
- Native extraction strategies
- Node flags

## Data Flow

### Parsing Flow

```
File Path
    │
    ▼
File Reader → Content
    │
    ▼
Language Detection → Adapter Selection
    │
    ▼
Tree-sitter Parse → TSTree
    │
    ▼
Tree Traversal → Nodes
    │
    ▼
Context Extraction → Semantic Data
    │
    ▼
Output Projection → DuckDB Table
```

### Multi-File Processing

```
File Patterns
    │
    ▼
Glob Expansion → File List
    │
    ▼
Deduplication → Unique Files
    │
    ▼
Sort by Path → Ordered List
    │
    ▼
For Each File:
    │
    ├─ Parse File
    ├─ Extract AST
    ├─ Add to Output Chunk
    │
    ▼
Stream Chunks → DuckDB
```

## Semantic Type System

### Encoding

8-bit encoding: `[ss kk tt ll]`

- `ss` (6-7): Super Kind
- `kk` (4-5): Kind
- `tt` (2-3): Super Type
- `ll` (0-1): Language-specific

### Type Definitions

```cpp
// src/language_configs/python_types.def
DEF_TYPE(function_definition, DEFINITION_FUNCTION, FIND_IDENTIFIER, FUNCTION_WITH_PARAMS, 0)
DEF_TYPE(class_definition, DEFINITION_CLASS, FIND_IDENTIFIER, CLASS_WITH_METHODS, 0)
```

## Extension Points

### Adding Languages

1. Add Tree-sitter grammar submodule
2. Create language adapter
3. Create type definitions
4. Register in adapter registry

See [Adding Languages](adding-languages.md) for details.

### Custom Extractors

Native extraction strategies can be customized per language:

```cpp
// src/include/<language>_native_extractors.hpp
template<>
struct LanguageNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content);
};
```

## Build System

### CMake Structure

```
CMakeLists.txt
├── Tree-sitter headers installation
├── Grammar generation (generate_parser function)
├── Source compilation
└── Extension linking
```

### Grammar Patching

**Location:** `scripts/apply_grammar_patches.sh`

Patches are applied during build to fix grammar issues.

## Testing

### SQL Logic Tests

**Location:** `test/sql/`

DuckDB's SQLLogicTest format for integration testing.

### Test Data

**Location:** `test/data/`

Sample source files for each supported language.

## Performance Considerations

### Streaming

- Files processed one at a time
- Results streamed in chunks (2048 rows)
- Memory efficient for large codebases

### Context Levels

Performance impact:
- `'none'`: Fastest
- `'node_types_only'`: Fast
- `'normalized'`: Medium
- `'native'`: Detailed but slower

### Caching

- Parser instances cached per language
- Type definitions loaded once at startup

## Source Files

### Core

| File | Purpose |
|------|---------|
| `src/sitting_duck_extension.cpp` | Extension entry point |
| `src/unified_ast_backend.cpp` | Main processing logic |
| `src/ast_file_utils.cpp` | File handling |

### Language Support

| File | Purpose |
|------|---------|
| `src/language_adapter.hpp` | Adapter base class |
| `src/language_adapter_registry_init.cpp` | Registration |
| `src/language_adapters/*.cpp` | Language-specific adapters |

### Type System

| File | Purpose |
|------|---------|
| `src/include/semantic_types.hpp` | Semantic type definitions |
| `src/language_configs/*.def` | Type mappings |
