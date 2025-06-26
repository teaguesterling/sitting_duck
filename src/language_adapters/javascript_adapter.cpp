#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "node_config.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"

// Tree-sitter language declaration
extern "C" {
    const TSLanguage *tree_sitter_javascript();
}

namespace duckdb {

//==============================================================================
// JavaScript Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> JavaScriptAdapter::node_configs = {
    #include "../language_configs/javascript_types.def"
};

#undef DEF_TYPE

string JavaScriptAdapter::GetLanguageName() const {
    return "javascript";
}

vector<string> JavaScriptAdapter::GetAliases() const {
    return {"javascript", "js"};
}

void JavaScriptAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_javascript();
    parser_wrapper_->SetLanguage(ts_language, "JavaScript");
}

unique_ptr<TSParserWrapper> JavaScriptAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_javascript();
    fresh_parser->SetLanguage(ts_language, "JavaScript");
    return fresh_parser;
}

string JavaScriptAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string JavaScriptAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // JavaScript-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string JavaScriptAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
        // For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
        return "";
    }
    
    return "";
}

bool JavaScriptAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In JavaScript, check for export statements or naming conventions
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // Check if it's an export declaration
    if (node_type.find("export") != string::npos) {
        return true;
    }
    
    // Check parent nodes for export context
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent)) {
        const char* parent_type = ts_node_type(parent);
        if (string(parent_type).find("export") != string::npos) {
            return true;
        }
    }
    
    // Check naming conventions - underscore prefix typically indicates private
    string name = ExtractNodeName(node, content);
    if (!name.empty() && name[0] == '_') {
        return false;
    }
    
    // Default to public for JavaScript (no explicit access modifiers)
    return true;
}

const unordered_map<string, NodeConfig>& JavaScriptAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction JavaScriptAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const JavaScriptAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}




} // namespace duckdb