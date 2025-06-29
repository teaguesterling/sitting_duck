#pragma once

#include "duckdb.hpp"
#include "node_config.hpp"
#include "ast_type.hpp"  // For ParameterInfo and NativeContext definitions
#include <tree_sitter/api.h>
#include <vector>
#include <type_traits>

// Language-specific extractors will be included at the END to avoid circular dependencies

namespace duckdb {

// Note: ParameterInfo and NativeContext are now defined in ast_type.hpp

//==============================================================================
// Template-Based Native Context Extraction Framework
//==============================================================================

// Base template for native context extraction - default returns empty context
template<NativeExtractionStrategy Strategy>
struct NativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for NONE strategy - explicit no-op
template<>
struct NativeExtractor<NativeExtractionStrategy::NONE> {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Explicitly no extraction
    }
};

//==============================================================================
// Language-Specific Template Specialization Framework
//==============================================================================

// Forward declarations for language-specific extractors
template<NativeExtractionStrategy Strategy> struct PythonNativeExtractor;
template<NativeExtractionStrategy Strategy> struct JavaScriptNativeExtractor;
template<NativeExtractionStrategy Strategy> struct JavaNativeExtractor;
template<NativeExtractionStrategy Strategy> struct TypeScriptNativeExtractor;
template<NativeExtractionStrategy Strategy> struct RustNativeExtractor;
template<NativeExtractionStrategy Strategy> struct CppNativeExtractor;
template<NativeExtractionStrategy Strategy> struct GoNativeExtractor;
template<NativeExtractionStrategy Strategy> struct CNativeExtractor;
template<NativeExtractionStrategy Strategy> struct PHPNativeExtractor;
template<NativeExtractionStrategy Strategy> struct RubyNativeExtractor;
template<NativeExtractionStrategy Strategy> struct SwiftNativeExtractor;
template<NativeExtractionStrategy Strategy> struct KotlinNativeExtractor;

// Language adapter traits - each adapter defines its extractor type
template<typename AdapterType>
struct NativeExtractionTraits {
    // Default: use generic extractor
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = NativeExtractor<Strategy>;
};

// Forward declare language adapters for traits
class PythonAdapter;
class JavaScriptAdapter;
class TypeScriptAdapter;
class JavaAdapter;
class CppAdapter;
class RustAdapter;
class GoAdapter;
class CAdapter;
class PHPAdapter;
class RubyAdapter;
class SwiftAdapter;
class KotlinAdapter;

// Specializations for each language adapter
template<>
struct NativeExtractionTraits<PythonAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = PythonNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<JavaScriptAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = JavaScriptNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<TypeScriptAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = TypeScriptNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<JavaAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = JavaNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<CppAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = CppNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<RustAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = RustNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<GoAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = GoNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<CAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = CNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<PHPAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = PHPNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<RubyAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = RubyNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<SwiftAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = SwiftNativeExtractor<Strategy>;
};

template<>
struct NativeExtractionTraits<KotlinAdapter> {
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = KotlinNativeExtractor<Strategy>;
};

//==============================================================================
// Main Template Dispatch Function (Zero Virtual Calls)
//==============================================================================

// Dynamic strategy dispatch function - called from hot loop
template<typename AdapterType>
NativeContext ExtractNativeContextTemplated(TSNode node, const string& content, NativeExtractionStrategy strategy) {
    // Runtime dispatch to language-specific extractors
    // Uses template traits to get the correct extractor type for each language
    switch (strategy) {
        case NativeExtractionStrategy::FUNCTION_WITH_PARAMS:
            return NativeExtractionTraits<AdapterType>::template ExtractorType<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
        case NativeExtractionStrategy::ASYNC_FUNCTION:
            return NativeExtractionTraits<AdapterType>::template ExtractorType<NativeExtractionStrategy::ASYNC_FUNCTION>::Extract(node, content);
        case NativeExtractionStrategy::CLASS_WITH_METHODS:
            return NativeExtractionTraits<AdapterType>::template ExtractorType<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
        case NativeExtractionStrategy::VARIABLE_WITH_TYPE:
            return NativeExtractionTraits<AdapterType>::template ExtractorType<NativeExtractionStrategy::VARIABLE_WITH_TYPE>::Extract(node, content);
        case NativeExtractionStrategy::ARROW_FUNCTION:
            return NativeExtractionTraits<AdapterType>::template ExtractorType<NativeExtractionStrategy::ARROW_FUNCTION>::Extract(node, content);
        case NativeExtractionStrategy::CLASS_WITH_INHERITANCE:
            return NativeExtractionTraits<AdapterType>::template ExtractorType<NativeExtractionStrategy::CLASS_WITH_INHERITANCE>::Extract(node, content);
        case NativeExtractionStrategy::FUNCTION_WITH_DECORATORS:
            return NativeExtractionTraits<AdapterType>::template ExtractorType<NativeExtractionStrategy::FUNCTION_WITH_DECORATORS>::Extract(node, content);
        case NativeExtractionStrategy::NONE:
        default:
            return NativeContext(); // Return empty context for NONE or unknown strategies
    }
}

//==============================================================================
// Helper Functions for Common Extraction Patterns
//==============================================================================

// Helper function to extract text from a specific child by type
string ExtractChildTextByType(TSNode node, const string& content, const string& child_type);

// Helper function to extract all children of a specific type
vector<TSNode> FindChildrenByType(TSNode node, const string& child_type);

// Helper function to extract parameter list from common patterns
vector<ParameterInfo> ExtractParameterList(TSNode params_node, const string& content);

// Helper function to extract modifiers from various patterns
vector<string> ExtractModifiersFromNode(TSNode node, const string& content);

// Helper function to build qualified name from context
string BuildQualifiedName(TSNode node, const string& content, const string& base_name);

// Helper function to extract node text content
string ExtractNodeText(TSNode node, const string& content);

} // namespace duckdb

// Include language-specific extractors AFTER all declarations to prevent circular dependencies
#include "python_native_extractors.hpp"
#include "javascript_native_extractors.hpp"
#include "typescript_native_extractors.hpp"
#include "java_native_extractors.hpp"
#include "cpp_native_extractors.hpp"
#include "rust_native_extractors.hpp"
#include "go_native_extractors.hpp"
#include "c_native_extractors.hpp"
#include "php_native_extractors.hpp"
#include "ruby_native_extractors.hpp"
#include "swift_native_extractors.hpp"
#include "kotlin_native_extractors.hpp"