#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// TypeScript-Specific Native Context Extractors (Stub Implementation)
//==============================================================================

// Base template for TypeScript extractors - returns empty context for now
template<NativeExtractionStrategy Strategy>
struct TypeScriptNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Stub: return empty context
    }
};

// TODO: Implement TypeScript-specific specializations
// For now, all strategies return empty context

} // namespace duckdb