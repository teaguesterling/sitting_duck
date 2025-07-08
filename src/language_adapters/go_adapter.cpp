#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_go();
}

namespace duckdb {

//==============================================================================
// Go Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> GoAdapter::node_configs = {
    #include "../language_configs/go_types.def"
};

#undef DEF_TYPE

string GoAdapter::GetLanguageName() const {
    return "go";
}

vector<string> GoAdapter::GetAliases() const {
    return {"go", "golang"};
}

void GoAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_go(), "Go");
}

unique_ptr<TSParserWrapper> GoAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_go(), "Go");
    return fresh_parser;
}

string GoAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}


string GoAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Go-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    } else if (node_type.find("_spec") != string::npos) {
        return FindChildByType(node, content, "identifier");
    } else if (node_type == "package_clause") {
        return FindChildByType(node, content, "package_identifier");
    }
    
    return "";
}

string GoAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
        // For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
        return "";
    }
    
    return "";
}

bool GoAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In Go, names starting with uppercase are public (exported)
    string name = ExtractNodeName(node, content);
    return !name.empty() && isupper(name[0]);
}

const unordered_map<string, NodeConfig>& GoAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction GoAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const GoAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}




} // namespace duckdb
