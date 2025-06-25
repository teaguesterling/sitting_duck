#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_swift();
}

namespace duckdb {

//==============================================================================
// Swift Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> SwiftAdapter::node_configs = {
#include "../language_configs/swift_types.def"
};

#undef DEF_TYPE

string SwiftAdapter::GetLanguageName() const {
    return "swift";
}

vector<string> SwiftAdapter::GetAliases() const {
    return {"swift"};
}

void SwiftAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_swift();
    parser_wrapper_->SetLanguage(ts_language, "Swift");
}

unique_ptr<TSParserWrapper> SwiftAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_swift();
    fresh_parser->SetLanguage(ts_language, "Swift");
    return fresh_parser;
}

string SwiftAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string SwiftAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Swift-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type == "function_declaration" || node_type == "init_declaration") {
        return FindChildByType(node, content, "simple_identifier");
    }
    if (node_type == "class_declaration" || node_type == "struct_declaration" || 
        node_type == "enum_declaration" || node_type == "protocol_declaration") {
        return FindChildByType(node, content, "type_identifier");
    }
    if (node_type == "property_declaration" || node_type == "variable_declaration") {
        return FindChildByType(node, content, "pattern");
    }
    if (node_type == "call_expression") {
        // Extract function name from call expression
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            if (child_type == "simple_identifier" || child_type == "navigation_expression") {
                return ExtractNodeText(child, content);
            }
        }
    }
    
    return "";
}

string SwiftAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
        // For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
        return "";
    }
    
    return "";
}

bool SwiftAdapter::IsPublicNode(TSNode node, const string &content) const {
    // Check for Swift access modifiers
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        string child_type = ts_node_type(child);
        
        if (child_type == "modifiers") {
            string modifier_text = ExtractNodeText(child, content);
            if (modifier_text.find("public") != string::npos || 
                modifier_text.find("open") != string::npos) {
                return true;
            }
            if (modifier_text.find("private") != string::npos || 
                modifier_text.find("fileprivate") != string::npos) {
                return false;
            }
        }
    }
    
    // Default to internal (package-visible) in Swift
    return true;
}

const unordered_map<string, NodeConfig>& SwiftAdapter::GetNodeConfigs() const {
    return node_configs;
}


ParsingFunction SwiftAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const SwiftAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb