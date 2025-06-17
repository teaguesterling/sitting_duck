# Adding New Languages to Sitting Duck

This guide explains how to add support for a new programming language to the Sitting Duck DuckDB extension.

## Overview

The modern language addition workflow involves:
1. **Grammar Setup** - Adding tree-sitter grammar as git submodule with build automation
2. **Language Adapter** - Implementing semantic mapping and extraction logic  
3. **Integration** - File extension mapping and registration
4. **Testing** - Validation and test data creation
5. **Patching** - Optional grammar fixes via build-time patches

## Prerequisites

- **Tree-sitter CLI**: Built automatically via Cargo during CMake
- **Grammar availability**: Must have an official tree-sitter grammar repository
- **Basic understanding**: Familiarity with tree-sitter AST structure for the target language

## Phase 1: Grammar Setup

### 1.1 Add Grammar Submodule

```bash
# Add the grammar as a git submodule
git submodule add https://github.com/tree-sitter/tree-sitter-<language> grammars/tree-sitter-<language>

# Initialize and update
git submodule update --init --recursive
```

### 1.2 Configure Build System

Add grammar generation to `CMakeLists.txt` in the appropriate dependency order:

```cmake
# In the grammar generation section, respect dependencies
# Example: TypeScript depends on JavaScript, C++ depends on C

# Generate parsers with dependency tracking
generate_tree_sitter_parser(<language> ${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-<language>)

# Add generated sources to extension
list(APPEND EXTENSION_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/grammars/tree-sitter-<language>/src/parser.c
)

# Add scanner if it exists (check the grammar repository)
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/grammars/tree-sitter-<language>/src/scanner.c)
    list(APPEND EXTENSION_SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/grammars/tree-sitter-<language>/src/scanner.c
    )
endif()
```

### 1.3 Handle Grammar Dependencies

Some grammars depend on others (e.g., TypeScript → JavaScript, C++ → C):

```cmake
# Ensure dependency is generated first
add_dependencies(generate_<language>_parser generate_<dependency>_parser)
```

### 1.4 Create Patches if Needed

If the grammar has build issues, create a patch file:

```bash
# Create patch directory if needed
mkdir -p patches/

# Create patch file (following naming convention)
# Format: fix-<language>-grammar-<issue>.patch
cat > patches/fix-<language>-grammar-require.patch << 'EOF'
--- a/grammar.js
+++ b/grammar.js
@@ -1,5 +1,5 @@
 module.exports = grammar({
   name: '<language>',
-  require: require('./node_modules/tree-sitter-dependency/grammar'),
+  require: require('../tree-sitter-dependency/grammar'),
   // ... rest of grammar
 });
EOF
```

The patch will be automatically applied during build via `scripts/apply_grammar_patches.sh`.

## Phase 2: Language Adapter Implementation

### 2.1 Create Semantic Type Definitions

Create `src/language_configs/<language>_types.def` with semantic mappings:

```cpp
// <Language> language node type definitions
// Format: DEF_TYPE(raw_type, semantic_type, name_strategy, value_strategy, flags)

// Definitions (functions, classes, variables)
DEF_TYPE(function_definition, DEFINITION_FUNCTION, FIND_IDENTIFIER, NONE, 0)
DEF_TYPE(class_definition, DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0) 
DEF_TYPE(method_definition, DEFINITION_FUNCTION, FIND_IDENTIFIER, NONE, 0)
DEF_TYPE(variable_declaration, DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0)

// Names and identifiers
DEF_TYPE(identifier, NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
DEF_TYPE(property_identifier, NAME_PROPERTY, NODE_TEXT, NONE, 0)
DEF_TYPE(type_identifier, TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)

// Literals
DEF_TYPE(string_literal, LITERAL_STRING, NONE, NODE_TEXT, NodeFlags::IS_LITERAL)
DEF_TYPE(number_literal, LITERAL_NUMBER, NONE, NODE_TEXT, NodeFlags::IS_LITERAL)
DEF_TYPE(boolean_literal, LITERAL_ATOMIC, NONE, NODE_TEXT, NodeFlags::IS_LITERAL)

// Function calls and member access
DEF_TYPE(call_expression, COMPUTATION_CALL, FIND_IDENTIFIER, NONE, 0)
DEF_TYPE(member_expression, COMPUTATION_ACCESS, NONE, NONE, 0)

// Control flow
DEF_TYPE(if_statement, FLOW_CONDITIONAL, NONE, NONE, 0)
DEF_TYPE(for_statement, FLOW_LOOP, NONE, NONE, 0)
DEF_TYPE(while_statement, FLOW_LOOP, NONE, NONE, 0)
DEF_TYPE(return_statement, FLOW_JUMP, NONE, NONE, 0)

// External dependencies
DEF_TYPE(import_statement, EXTERNAL_IMPORT, NONE, NONE, 0)
DEF_TYPE(import_declaration, EXTERNAL_IMPORT, NONE, NONE, 0)

// Comments and metadata
DEF_TYPE(comment, METADATA_COMMENT, NONE, NODE_TEXT, 0)

// Parser syntax nodes (filtered by default)
DEF_TYPE(ERROR, PARSER_ERROR, NONE, NODE_TEXT, 0)
DEF_TYPE("(", PARSER_SYNTAX, NONE, NONE, 0)
DEF_TYPE(")", PARSER_SYNTAX, NONE, NONE, 0)
// Add other punctuation...
```

**Semantic Type Hierarchy** (see `src/include/semantic_types.hpp`):

| Category | Examples | Purpose |
|----------|----------|---------|
| **DATA_STRUCTURE** | `LITERAL_STRING`, `NAME_IDENTIFIER`, `TYPE_PRIMITIVE` | Data and type information |
| **COMPUTATION** | `COMPUTATION_CALL`, `DEFINITION_FUNCTION`, `COMPUTATION_ACCESS` | Operations and definitions |
| **CONTROL_EFFECTS** | `FLOW_CONDITIONAL`, `FLOW_LOOP`, `ORGANIZATION_SCOPE` | Control flow and structure |
| **META_EXTERNAL** | `EXTERNAL_IMPORT`, `METADATA_COMMENT`, `PARSER_SYNTAX` | Metadata and external refs |

### 2.2 Add Adapter Class Declaration

In `src/include/language_adapter.hpp`:

```cpp
//==============================================================================
// <Language> Adapter
//==============================================================================
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

Create `src/language_adapters/<language>_adapter.cpp`:

```cpp
#include "include/language_adapter.hpp"
#include "include/semantic_types.hpp"

namespace duckdb {

// External tree-sitter function declaration
extern "C" {
    const TSLanguage *tree_sitter_<language>();
}

//==============================================================================
// <Language> Adapter Implementation
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
    return {"<language>", "<alias1>", "<alias2>"};  // Common names/extensions
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
    
    // Language-specific fallback patterns
    string node_type = string(node_type_str);
    
    // Common patterns for definitions
    if (node_type.find("definition") != string::npos || 
        node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    // Function calls
    if (node_type.find("call") != string::npos) {
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
    string name = ExtractNodeName(node, content);
    if (name.empty()) {
        return false;
    }
    
    // Language-specific public/private detection
    // Examples:
    
    // Go: Uppercase first letter = public
    // return isupper(name[0]);
    
    // Python: Underscore prefix = private
    // return name[0] != '_';
    
    // JavaScript: Check for export or underscore convention
    // string node_type = ts_node_type(node);
    // if (node_type.find("export") != string::npos) {
    //     return true;
    // }
    // return name[0] != '_';
    
    // C++: Complex visibility rules (see existing implementation)
    
    // Default: assume public if named
    return true;
}

uint8_t <Language>Adapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* <Language>Adapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

} // namespace duckdb
```

### 2.4 Custom Extraction Strategies

For languages requiring special handling (like Markdown), implement custom extraction:

```cpp
string <Language>Adapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // Custom strategy example (from Markdown adapter)
    if (node_type == "atx_heading" || node_type == "setext_heading") {
        // Find the inline content, not the raw text with # symbols
        return FindChildByType(node, content, "inline");
    }
    
    // Fall back to standard strategies
    const NodeConfig* config = GetNodeConfig(node_type_str);
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    return "";
}
```

## Phase 3: Integration

### 3.1 Register Language Adapter

Add to `src/language_adapter_registry_init.cpp`:

```cpp
void LanguageAdapterRegistry::InitializeDefaultAdapters() {
    // ... existing registrations ...
    RegisterLanguageFactory("<language>", []() { return make_uniq<<Language>Adapter>(); });
}
```

### 3.2 Add File Extension Mapping

Update `src/ast_file_utils.cpp`:

```cpp
static const std::unordered_map<string, string> EXTENSION_TO_LANGUAGE = {
    // ... existing mappings ...
    {"<ext1>", "<language>"},
    {"<ext2>", "<language>"},
};

static const std::unordered_map<string, vector<string>> LANGUAGE_TO_EXTENSIONS = {
    // ... existing mappings ...
    {"<language>", {"<ext1>", "<ext2>"}},
};
```

## Phase 4: Testing

### 4.1 Create Test Data

Create test files in `test/data/<language>/`:

```bash
mkdir -p test/data/<language>/
```

**Simple test file** (`simple.<ext>`):
```<language>
// Basic constructs for testing
function example() {
    let variable = "hello";
    return variable;
}

class MyClass {
    method() {
        return 42;
    }
}
```

### 4.2 Create Test Suite

Create `test/sql/<language>_language_support.test`:

```sql
# name: test/sql/<language>_language_support.test
# description: Test <Language> language support functionality
# group: [sitting_duck]

require sitting_duck

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

# Test 3: Function detection
query I
SELECT COUNT(*) FROM read_ast('test/data/<language>/simple.<ext>') 
WHERE type LIKE '%function%' AND name IS NOT NULL;
----
<expected_function_count>

# Test 4: Class detection
query I
SELECT COUNT(*) FROM read_ast('test/data/<language>/simple.<ext>') 
WHERE type LIKE '%class%' AND name IS NOT NULL;
----
<expected_class_count>

# Test 5: Semantic type normalization
query I
SELECT DISTINCT semantic_type FROM read_ast('test/data/<language>/simple.<ext>') 
WHERE semantic_type IS NOT NULL 
ORDER BY semantic_type;
----
<expected_semantic_types>

# Test 6: Public node detection
query I
SELECT COUNT(*) FROM read_ast('test/data/<language>/simple.<ext>') 
WHERE name IS NOT NULL AND is_public = true;
----
<expected_public_count>
```

### 4.3 Build and Test

```bash
# Build the extension
make

# Run specific language tests
make test ARGS="--gtest_filter=*<language>*"

# Or run all tests
make test

# Manual testing
./build/release/duckdb -c "
LOAD './build/release/extension/sitting_duck/sitting_duck.duckdb_extension';
SELECT * FROM ast_supported_languages() WHERE language = '<language>';
SELECT COUNT(*) FROM read_ast('test/data/<language>/simple.<ext>');
"
```

## Phase 5: Advanced Features

### 5.1 Language-Specific Optimizations

Implement performance optimizations:

```cpp
// Override for language-specific performance improvements
bool <Language>Adapter::ShouldProcessNode(TSNode node) const {
    const char* node_type = ts_node_type(node);
    
    // Skip certain syntax nodes for performance
    if (strcmp(node_type, "comment") == 0 && skip_comments_) {
        return false;
    }
    
    return LanguageAdapter::ShouldProcessNode(node);
}
```

### 5.2 Complex Public/Private Detection

For languages with sophisticated visibility rules:

```cpp
bool <Language>Adapter::IsPublicNode(TSNode node, const string &content) const {
    string node_type = ts_node_type(node);
    
    // Check for explicit access modifiers
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent)) {
        string parent_type = ts_node_type(parent);
        
        // Look for access specifier siblings
        uint32_t child_count = ts_node_child_count(parent);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode sibling = ts_node_child(parent, i);
            string sibling_type = ts_node_type(sibling);
            
            if (sibling_type == "access_specifier") {
                string access_text = GetNodeText(sibling, content);
                return access_text == "public";
            }
        }
    }
    
    // Fall back to naming conventions
    string name = ExtractNodeName(node, content);
    return !name.empty() && name[0] != '_';
}
```

## Troubleshooting

### Build Issues

**Grammar not found:**
```bash
# Check submodule status
git submodule status grammars/tree-sitter-<language>

# Initialize if needed
git submodule update --init grammars/tree-sitter-<language>
```

**Parser generation fails:**
```bash
# Check for grammar.js issues
cat grammars/tree-sitter-<language>/grammar.js

# Apply patches manually if needed
./scripts/apply_grammar_patches.sh
```

**Missing scanner:**
```bash
# Check if grammar has external scanner
ls grammars/tree-sitter-<language>/src/scanner.*

# Update CMakeLists.txt accordingly
```

### Runtime Issues

**Language not registered:**
```sql
-- Check available languages
SELECT * FROM ast_supported_languages();

-- Verify extension is loaded
LOAD sitting_duck;
```

**Parsing failures:**
```bash
# Test with tree-sitter CLI directly
npx tree-sitter parse path/to/file.<ext> --language grammars/tree-sitter-<language>

# Check for ABI compatibility issues
```

**Semantic type errors:**
```cpp
// Verify semantic type constants exist
grep -r "DEFINITION_FUNCTION" src/include/semantic_types.hpp

// Check for typos in .def file
cat src/language_configs/<language>_types.def
```

### Performance Issues

**Slow parsing:**
- Use more specific semantic types instead of generic ones
- Implement `ShouldProcessNode()` filtering
- Add appropriate NodeFlags for literals

**Memory usage:**
- Review extraction strategies (avoid unnecessary `NODE_TEXT`)
- Consider lazy initialization for large grammars

## Language-Specific Examples

### Go (Uppercase = Public)
```cpp
bool GoAdapter::IsPublicNode(TSNode node, const string &content) const {
    string name = ExtractNodeName(node, content);
    return !name.empty() && isupper(name[0]);
}
```

### Python (Underscore = Private)
```cpp
bool PythonAdapter::IsPublicNode(TSNode node, const string &content) const {
    string name = ExtractNodeName(node, content);
    return !name.empty() && name[0] != '_';
}
```

### JavaScript/TypeScript (Export Detection)
```cpp
bool JavaScriptAdapter::IsPublicNode(TSNode node, const string &content) const {
    // Check for export keywords in parent nodes
    TSNode current = node;
    while (!ts_node_is_null(current)) {
        string node_type = ts_node_type(current);
        if (node_type.find("export") != string::npos) {
            return true;
        }
        current = ts_node_parent(current);
    }
    
    // Fall back to naming convention
    string name = ExtractNodeName(node, content);
    return !name.empty() && name[0] != '_';
}
```

## Semantic Type Reference

### Complete Hierarchy

```cpp
// DATA_STRUCTURE (0x00 - 0x3F)
LITERAL_STRING    = 0x00,  // "hello"
LITERAL_NUMBER    = 0x01,  // 42, 3.14
LITERAL_ATOMIC    = 0x02,  // true, false, null
NAME_IDENTIFIER   = 0x04,  // variable names
NAME_PROPERTY     = 0x05,  // obj.property
NAME_QUALIFIED    = 0x06,  // module::function
TYPE_PRIMITIVE    = 0x08,  // int, string
TYPE_COMPOSITE    = 0x09,  // Array<T>, struct
TYPE_GENERIC      = 0x0A,  // <T>

// COMPUTATION (0x40 - 0x7F)  
COMPUTATION_CALL      = 0x40,  // function()
COMPUTATION_ACCESS    = 0x41,  // obj[key], obj.prop
DEFINITION_FUNCTION   = 0x44,  // function declarations
DEFINITION_CLASS      = 0x45,  // class/struct/interface
DEFINITION_VARIABLE   = 0x46,  // var, let, const

// CONTROL_EFFECTS (0x80 - 0xBF)
FLOW_CONDITIONAL   = 0x80,  // if, switch
FLOW_LOOP         = 0x81,  // for, while
FLOW_JUMP         = 0x82,  // return, break, continue
ORGANIZATION_SCOPE = 0x84,  // blocks, modules

// META_EXTERNAL (0xC0 - 0xFF)
EXTERNAL_IMPORT    = 0xC0,  // import, include
EXTERNAL_EXPORT    = 0xC1,  // export
METADATA_COMMENT   = 0xC4,  // comments
PARSER_SYNTAX      = 0xE0,  // punctuation
PARSER_ERROR       = 0xE1,  // parse errors
```

This comprehensive guide reflects the current sophisticated architecture of Sitting Duck and provides everything needed to add new language support efficiently and correctly.