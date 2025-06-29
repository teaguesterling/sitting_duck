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

Add grammar generation to `CMakeLists.txt` using the existing `generate_parser` function. The system respects dependencies automatically:

**Location in CMakeLists.txt**: Lines 89-106 (after existing parsers)

```cmake
# Add your language to the parser generation section
# Format: generate_parser("grammar_dir" "parser_path" "scanner_path")

# For simple grammars without scanners:
generate_parser("tree-sitter-<language>" "tree-sitter-<language>/src/parser.c" "")

# For grammars with scanners:  
generate_parser("tree-sitter-<language>" "tree-sitter-<language>/src/parser.c" "tree-sitter-<language>/src/scanner.c")

# For nested grammar structures (like TypeScript):
generate_parser("tree-sitter-<language>/<language>" "tree-sitter-<language>/<language>/src/parser.c" "tree-sitter-<language>/<language>/src/scanner.c")
```

**Add to EXTENSION_SOURCES** (lines 168-220):

```cmake
set(EXTENSION_SOURCES 
    # ... existing sources ...
    # Add adapter source file:
    src/language_adapters/<language>_adapter.cpp
    # Add generated parser files:
    grammars/tree-sitter-<language>/src/parser.c
    # Add scanner if it exists:
    grammars/tree-sitter-<language>/src/scanner.c
)
```

### 1.3 Handle Grammar Dependencies

The `generate_parser` function automatically handles dependencies:

**C++ dependency on C** (built-in):
```cmake
# Add dependency on C grammar for C++ (since C++ requires C)
if("${GRAMMAR_NAME}" STREQUAL "tree-sitter-cpp")
    list(APPEND GENERATION_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-c/src/parser.c")
    list(APPEND GENERATION_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-c/grammar.js")
endif()
```

**TypeScript dependency on JavaScript** (built-in):
```cmake
# Add dependency on JavaScript grammar for TypeScript
if("${GRAMMAR_NAME}" STREQUAL "typescript")
    list(APPEND GENERATION_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-javascript/src/parser.c")
    list(APPEND GENERATION_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-javascript/grammar.js")
endif()
```

**For new dependencies**, add similar logic to the `generate_parser` function around line 47-57.

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

### 2.1 Native Context Extraction Setup (CRITICAL)

**For languages requiring native context extraction (functions with parameters, classes with methods), you MUST update these four critical files with EXACT class name consistency:**

1. **Forward declaration in `src/include/native_context_extraction.hpp`** (line ~67):
```cpp
class <Language>Adapter;  // EXACT class name match required
```

2. **Template trait specialization in `src/include/native_context_extraction.hpp`** (line ~100+):
```cpp
template<>
struct NativeExtractionTraits<<Language>Adapter> {  // EXACT class name match required
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = <Language>NativeExtractor<Strategy>;
};
```

3. **Include extractor header in `src/include/native_context_extraction.hpp`** (line ~204):
```cpp
#include "<language>_native_extractors.hpp"
```

4. **Create native extractor file `src/include/<language>_native_extractors.hpp`**:
```cpp
#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

// Forward declaration for <Language>Adapter - EXACT class name match required
class <Language>Adapter;

// Base template for <Language> extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct <Language>NativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS
template<>
struct <Language>NativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        try {
            // Extract function signature and parameters
            context.signature_type = Extract<Language>ReturnType(node, content);
            context.parameters = Extract<Language>Parameters(node, content);
            context.modifiers = Extract<Language>Modifiers(node, content);
        } catch (...) {
            context.signature_type = "";  // Empty string becomes NULL in output
            context.parameters.clear();
            context.modifiers.clear();
        }
        return context;
    }

private:
    static string Extract<Language>ReturnType(TSNode node, const string& content) {
        // Language-specific return type extraction logic
        return "";
    }
    
    static vector<ParameterInfo> Extract<Language>Parameters(TSNode node, const string& content) {
        // Language-specific parameter extraction logic
        return {};
    }
    
    static vector<string> Extract<Language>Modifiers(TSNode node, const string& content) {
        // Language-specific modifier extraction logic
        return {};
    }
};

// Add other strategy specializations as needed...

} // namespace duckdb
```

**CRITICAL**: The class name in the forward declaration, template trait specialization, and actual adapter class declaration MUST match EXACTLY (including case). Template resolution will silently fail if there's any mismatch.

**Common Pitfall**: Using `CppAdapter` vs `CPPAdapter` or `JavascriptAdapter` vs `JavaScriptAdapter` will cause template dispatch to fail and native extraction to return NULL.

### 2.2 Create Semantic Type Definitions

Create `src/language_configs/<language>_types.def` with semantic mappings:

```cpp
// <Language> language node type definitions
// Format: DEF_TYPE(raw_type, semantic_type, name_strategy, native_strategy, flags)

// Definitions (functions, classes, variables) - WITH NATIVE EXTRACTION
DEF_TYPE(function_definition, DEFINITION_FUNCTION, FIND_IDENTIFIER, FUNCTION_WITH_PARAMS, 0)
DEF_TYPE(class_definition, DEFINITION_CLASS, FIND_IDENTIFIER, CLASS_WITH_METHODS, 0) 
DEF_TYPE(method_definition, DEFINITION_FUNCTION, FIND_IDENTIFIER, FUNCTION_WITH_PARAMS, 0)
DEF_TYPE(variable_declaration, DEFINITION_VARIABLE, FIND_IDENTIFIER, VARIABLE_WITH_TYPE, 0)

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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;

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
#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"

namespace duckdb {

// External tree-sitter function declaration
extern "C" {
    const TSLanguage *tree_sitter_<language>();
}

//==============================================================================
// <Language> Adapter Implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

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
        // Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
        // For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
        return "";
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

const unordered_map<string, NodeConfig>& <Language>Adapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction <Language>Adapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const <Language>Adapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
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

## Complete Real Example: Adding Kotlin Support

Here's a complete walkthrough of adding Kotlin support to demonstrate the full process:

### Step 1: Add Submodule and CMake Configuration

```bash
# Add the grammar submodule
git submodule add https://github.com/fwcd/tree-sitter-kotlin grammars/tree-sitter-kotlin
git submodule update --init --recursive
```

**CMakeLists.txt changes**:
```cmake
# Add to parser generation section (line ~107):
generate_parser("tree-sitter-kotlin" "tree-sitter-kotlin/src/parser.c" "tree-sitter-kotlin/src/scanner.c")

# Add to EXTENSION_SOURCES (line ~220):
set(EXTENSION_SOURCES 
    # ... existing sources ...
    src/language_adapters/kotlin_adapter.cpp
    grammars/tree-sitter-kotlin/src/parser.c
    grammars/tree-sitter-kotlin/src/scanner.c
)
```

### Step 2: Create Language Configuration

**src/language_configs/kotlin_types.def**:
```cpp
// Kotlin language node type definitions
DEF_TYPE(class_declaration, DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0)
DEF_TYPE(function_declaration, DEFINITION_FUNCTION, FIND_IDENTIFIER, NONE, 0)
DEF_TYPE(property_declaration, DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0)
DEF_TYPE(parameter, DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0)

DEF_TYPE(simple_identifier, NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
DEF_TYPE(type_identifier, TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)

DEF_TYPE(string_literal, LITERAL_STRING, NONE, NODE_TEXT, NodeFlags::IS_LITERAL)
DEF_TYPE(integer_literal, LITERAL_NUMBER, NONE, NODE_TEXT, NodeFlags::IS_LITERAL)
DEF_TYPE(boolean_literal, LITERAL_ATOMIC, NONE, NODE_TEXT, NodeFlags::IS_LITERAL)

DEF_TYPE(call_expression, COMPUTATION_CALL, FIND_IDENTIFIER, NONE, 0)
DEF_TYPE(navigation_expression, COMPUTATION_ACCESS, NONE, NONE, 0)

DEF_TYPE(if_expression, FLOW_CONDITIONAL, NONE, NONE, 0)
DEF_TYPE(for_statement, FLOW_LOOP, NONE, NONE, 0)
DEF_TYPE(while_statement, FLOW_LOOP, NONE, NONE, 0)
DEF_TYPE(return_at, FLOW_JUMP, NONE, NONE, 0)

DEF_TYPE(import_header, EXTERNAL_IMPORT, NONE, NONE, 0)
DEF_TYPE(package_header, EXTERNAL_EXPORT, NONE, NONE, 0)

DEF_TYPE(line_comment, METADATA_COMMENT, NONE, NODE_TEXT, 0)
DEF_TYPE(multiline_comment, METADATA_COMMENT, NONE, NODE_TEXT, 0)
```

### Step 3: Add Adapter Declaration

**src/include/language_adapter.hpp** (add around line ~250):
```cpp
//==============================================================================
// Kotlin Adapter
//==============================================================================
class KotlinAdapter : public LanguageAdapter {
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

### Step 4: Implement Full Adapter

**src/language_adapters/kotlin_adapter.cpp** (complete file):
```cpp
#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"

namespace duckdb {

extern "C" {
    const TSLanguage *tree_sitter_kotlin();
}

//==============================================================================
// Kotlin Adapter Implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> KotlinAdapter::node_configs = {
    #include "language_configs/kotlin_types.def"
};

#undef DEF_TYPE

string KotlinAdapter::GetLanguageName() const {
    return "kotlin";
}

vector<string> KotlinAdapter::GetAliases() const {
    return {"kotlin", "kt", "kts"};
}

void KotlinAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_kotlin(), "Kotlin");
}

unique_ptr<TSParserWrapper> KotlinAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_kotlin(), "Kotlin");
    return fresh_parser;
}

string KotlinAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;
}

string KotlinAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Kotlin-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "simple_identifier");
    }
    
    return "";
}

string KotlinAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool KotlinAdapter::IsPublicNode(TSNode node, const string &content) const {
    // Kotlin: check for 'private' modifier, default is public
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent)) {
        uint32_t child_count = ts_node_child_count(parent);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode sibling = ts_node_child(parent, i);
            string sibling_type = ts_node_type(sibling);
            
            if (sibling_type == "visibility_modifier") {
                string visibility = GetNodeText(sibling, content);
                return visibility != "private" && visibility != "internal";
            }
        }
    }
    
    // Default to public if no explicit visibility modifier
    string name = ExtractNodeName(node, content);
    return !name.empty();
}

const unordered_map<string, NodeConfig>& KotlinAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction KotlinAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const KotlinAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb
```

### Step 5: Register and Map Extensions

**src/language_adapter_registry_init.cpp**:
```cpp
void LanguageAdapterRegistry::InitializeDefaultAdapters() {
    // ... existing registrations ...
    RegisterLanguageFactory("kotlin", []() { return make_uniq<KotlinAdapter>(); });
}
```

**src/ast_file_utils.cpp**:
```cpp
static const std::unordered_map<string, string> EXTENSION_TO_LANGUAGE = {
    // ... existing mappings ...
    {"kt", "kotlin"},
    {"kts", "kotlin"},
};

static const std::unordered_map<string, vector<string>> LANGUAGE_TO_EXTENSIONS = {
    // ... existing mappings ...
    {"kotlin", {"kt", "kts"}},
};
```

### Step 6: Create Tests

**test/data/kotlin/simple.kt**:
```kotlin
package com.example

import kotlin.collections.List

class Person(val name: String, private val age: Int) {
    fun greet(): String {
        return "Hello, I'm $name"
    }
    
    private fun getAge() = age
}

fun main() {
    val person = Person("Alice", 25)
    println(person.greet())
}
```

**test/sql/kotlin_language_support.test**:
```sql
# name: test/sql/kotlin_language_support.test
# description: Test Kotlin language support functionality
# group: [sitting_duck]

require sitting_duck

# Test 1: Language is supported
query I
SELECT language FROM ast_supported_languages() WHERE language = 'kotlin';
----
kotlin

# Test 2: File extension auto-detection
query I
SELECT COUNT(*) > 0 FROM read_ast('test/data/kotlin/simple.kt');
----
true

# Test 3: Function detection
query I
SELECT COUNT(*) FROM read_ast('test/data/kotlin/simple.kt') 
WHERE type LIKE '%function%' AND name IS NOT NULL;
----
2

# Test 4: Class detection
query I
SELECT COUNT(*) FROM read_ast('test/data/kotlin/simple.kt') 
WHERE type LIKE '%class%' AND name IS NOT NULL;
----
1

# Test 5: Public vs private detection
query II
SELECT 
    SUM(CASE WHEN is_public THEN 1 ELSE 0 END) as public_count,
    SUM(CASE WHEN NOT is_public THEN 1 ELSE 0 END) as private_count
FROM read_ast('test/data/kotlin/simple.kt') 
WHERE name IS NOT NULL AND type LIKE '%function%';
----
1	1
```

This example demonstrates the complete complexity and sophistication of the current system, including proper visibility detection for Kotlin's modifier system.

---

This comprehensive guide reflects the current sophisticated architecture of Sitting Duck and provides everything needed to add new language support efficiently and correctly.