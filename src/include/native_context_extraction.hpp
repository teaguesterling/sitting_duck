#pragma once

#include "duckdb.hpp"
#include "node_config.hpp"
#include "ast_type.hpp"  // For ParameterInfo and NativeContext definitions
#include <tree_sitter/api.h>
#include <vector>

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

// Language adapter traits - each adapter defines its extractor type
template<typename AdapterType>
struct NativeExtractionTraits {
    // Default: use generic extractor
    template<NativeExtractionStrategy Strategy>
    using ExtractorType = NativeExtractor<Strategy>;
};

// Specializations for each language adapter (to be defined in adapter files)
// Example:
// template<>
// struct NativeExtractionTraits<PythonAdapter> {
//     template<NativeExtractionStrategy Strategy>
//     using ExtractorType = PythonNativeExtractor<Strategy>;
// };

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

} // namespace duckdb