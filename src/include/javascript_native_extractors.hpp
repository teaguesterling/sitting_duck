#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// JavaScript-Specific Native Context Extractors (Stub Implementation)
//==============================================================================

// Base template for JavaScript extractors - returns empty context for now
template<NativeExtractionStrategy Strategy>
struct JavaScriptNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Stub: return empty context
    }
};

// TODO: Implement JavaScript-specific specializations
// For now, all strategies return empty context

} // namespace duckdb