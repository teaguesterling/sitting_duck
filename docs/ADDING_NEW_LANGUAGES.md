# Adding New Languages to DuckDB AST Extension

This guide explains how to add support for a new programming language to the DuckDB AST extension.

## Overview

Adding a new language involves:
1. Setting up the tree-sitter grammar
2. Implementing the language adapter
3. Adding file extension auto-detection
4. Writing tests
5. Building and validating

## Step 1: Setting Up Tree-Sitter Grammar

### 1.1 Clone the Grammar Repository

```bash
cd grammars/
git clone https://github.com/tree-sitter/tree-sitter-<language>
```

### 1.2 Add to CMakeLists.txt

Add the parser and scanner files to `CMakeLists.txt`:

```cmake
set(EXTENSION_SOURCES 
    # ... existing sources ...
    grammars/tree-sitter-<language>/src/parser.c
    grammars/tree-sitter-<language>/src/scanner.c  # if it exists
)
```

### 1.3 Add External Declaration

In `src/language_adapter.cpp`, add the external declaration:

```cpp
extern "C" {
    // ... existing declarations ...
    const TSLanguage *tree_sitter_<language>();
}
```

## Step 2: Implementing the Language Adapter

### 2.1 Create Language Type Definitions

Create `src/language_configs/<language>_types.def`:

```cpp
// <Language> language node type definitions
// Format: DEF_TYPE(raw_type, semantic_type, name_strategy, value_strategy, flags)

// Common patterns:
DEF_TYPE(function_definition, DEFINITION_FUNCTION, FIND_IDENTIFIER, NONE, 0x01)
DEF_TYPE(class_definition, DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0x01)
DEF_TYPE(variable_declaration, DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0x01)
DEF_TYPE(identifier, NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
DEF_TYPE(string_literal, LITERAL_STRING, NONE, NODE_TEXT, 0)
DEF_TYPE(number_literal, LITERAL_NUMBER, NONE, NODE_TEXT, 0)
```

### 2.2 Add Language Adapter Class

In `src/include/language_adapter.hpp`, add the class declaration:

```cpp
class <Language>Adapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    uint8_t GetNodeFlags(const string &node_type) const override;
    const NodeConfig* GetNodeConfig(const string &node_type) const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};
```

### 2.3 Implement Language Adapter

In `src/language_adapter.cpp`, add the implementation:

```cpp
//==============================================================================
// <Language> Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> <Language>Adapter::node_configs = {
    #include "language_configs/<language>_types.def"
};

#undef DEF_TYPE

string <Language>Adapter::GetLanguageName() const {
    return "<language>";
}

vector<string> <Language>Adapter::GetAliases() const {
    return {"<language>", "<alias1>", "<alias2>"};
}

void <Language>Adapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_<language>(), "<Language>");
}

unique_ptr<TSParserWrapper> <Language>Adapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_<language>(), "<Language>");
    return fresh_parser;
}

string <Language>Adapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;
}

string <Language>Adapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Language-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string <Language>Adapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool <Language>Adapter::IsPublicNode(TSNode node, const string &content) const {
    // Implement language-specific public/private detection
    // Examples:
    // - Go: uppercase = public
    // - Python: underscore prefix = private
    // - JavaScript: export detection
    // - C++: access specifiers
    
    string name = ExtractNodeName(node, content);
    return !name.empty(); // Default implementation
}

uint8_t <Language>Adapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* <Language>Adapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}
```

### 2.4 Register Language Adapter

In the `InitializeDefaultAdapters()` function:

```cpp
void LanguageAdapterRegistry::InitializeDefaultAdapters() {
    // ... existing registrations ...
    RegisterLanguageFactory("<language>", []() { return make_uniq<<Language>Adapter>(); });
}
```

## Step 3: Add File Extension Auto-Detection

In `src/ast_file_utils.cpp`, add the extensions:

```cpp
static const std::unordered_map<string, string> EXTENSION_TO_LANGUAGE = {
    // ... existing mappings ...
    {"<ext>", "<language>"}, {"<ext2>", "<language>"},
};

static const std::unordered_map<string, vector<string>> LANGUAGE_TO_EXTENSIONS = {
    // ... existing mappings ...
    {"<language>", {"<ext>", "<ext2>"}},
};
```

## Step 4: Write Tests

### 4.1 Create Test Data

Create test files in `test/data/<language>/`:
- `simple.<ext>` - Basic language constructs
- `complex.<ext>` - More advanced features (optional)

### 4.2 Create Test Suite

Create `test/sql/<language>_language_support.test`:

```sql
# name: test/sql/<language>_language_support.test
# description: Test <Language> language support functionality
# group: [duckdb_ast]

require duckdb_ast

# Test 1: Language is supported
query I
SELECT language FROM ast_supported_languages() WHERE language = '<language>';
----
<language>

# Test 2: File extension auto-detection
query I
SELECT COUNT(*) > 0 FROM read_ast('test/data/<language>/simple.<ext>');
----
true

# Test 3: Basic parsing
query I
SELECT COUNT(*) FROM read_ast('test/data/<language>/simple.<ext>') WHERE name IS NOT NULL;
----
<expected_count>

# Add more language-specific tests...
```

## Step 5: Build and Test

### 5.1 Build the Extension

```bash
make
```

### 5.2 Run Tests

```bash
# Run specific language tests
./build/release/test/unittest "test/sql/<language>_language_support.test"

# Run all tests
./build/release/test/unittest
```

### 5.3 Manual Testing

```bash
./build/release/duckdb -c "
SELECT language FROM ast_supported_languages() WHERE language = '<language>';
SELECT COUNT(*) FROM read_ast('path/to/test.<ext>');
SELECT name, type FROM read_ast('path/to/test.<ext>') WHERE name IS NOT NULL;
"
```

## Common Semantic Type Mappings

Here are common semantic types to use:

### Definitions
- `DEFINITION_FUNCTION` - Functions, methods
- `DEFINITION_CLASS` - Classes, structs, interfaces
- `DEFINITION_VARIABLE` - Variables, constants
- `DEFINITION_MODULE` - Modules, packages, namespaces

### Names and Identifiers
- `NAME_IDENTIFIER` - Simple identifiers
- `NAME_QUALIFIED` - Qualified names (obj.prop)
- `TYPE_REFERENCE` - Type references

### Literals
- `LITERAL_STRING` - String literals
- `LITERAL_NUMBER` - Number literals
- `LITERAL_ATOMIC` - Booleans, null values

### Computation
- `COMPUTATION_CALL` - Function calls
- `COMPUTATION_ACCESS` - Member access, array access

### Control Flow
- `FLOW_CONDITIONAL` - If statements, switches
- `FLOW_LOOP` - For/while loops
- `FLOW_JUMP` - Return, break, continue

### External
- `EXTERNAL_IMPORT` - Import statements
- `EXTERNAL_EXPORT` - Export statements

### Metadata
- `METADATA_COMMENT` - Comments

## Extraction Strategies

- `NONE` - Don't extract anything
- `NODE_TEXT` - Extract the raw text of the node
- `FIND_IDENTIFIER` - Find an identifier child
- `FIND_PROPERTY` - Find a property identifier child
- `FIRST_CHILD` - Use first child's text

## Public/Private Detection Patterns

### Go
```cpp
// Uppercase = public
string name = ExtractNodeName(node, content);
return !name.empty() && isupper(name[0]);
```

### Python  
```cpp
// Underscore prefix = private
string name = ExtractNodeName(node, content);
return !name.empty() && name[0] != '_';
```

### JavaScript/TypeScript
```cpp
// Check for export keywords
string node_type = ts_node_type(node);
if (node_type.find("export") != string::npos) {
    return true;
}
// Check underscore convention
string name = ExtractNodeName(node, content);
return name.empty() || name[0] != '_';
```

### C++
```cpp
// Check access specifiers and naming conventions
// (Complex implementation - see existing C++ adapter)
```

## Troubleshooting

### Build Issues
- Check that grammar files are properly included in CMakeLists.txt
- Verify external function declaration matches grammar name
- Ensure ABI compatibility (tree-sitter version)

### Parsing Issues
- Use `tree-sitter parse <file>` to debug grammar issues
- Check semantic type constants exist in `semantic_types.hpp`
- Verify flag values are numeric (not constants)

### Test Failures
- Check expected node counts in test files
- Verify file paths in tests are correct
- Ensure test data files are valid for the language

### Performance Issues
- Consider using more specific semantic types for better performance
- Optimize `IsPublicNode` implementation for large files
- Use appropriate extraction strategies to minimize text processing