#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_php();
}

namespace duckdb {

//==============================================================================
// PHP Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> PHPAdapter::node_configs = {
#include "../language_configs/php_types.def"
};

#undef DEF_TYPE

string PHPAdapter::GetLanguageName() const {
    return "php";
}

vector<string> PHPAdapter::GetAliases() const {
    return {"php"};
}

void PHPAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_php();
    parser_wrapper_->SetLanguage(ts_language, "PHP");
}

unique_ptr<TSParserWrapper> PHPAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_php();
    fresh_parser->SetLanguage(ts_language, "PHP");
    return fresh_parser;
}

string PHPAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string PHPAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config && config->name_strategy != ExtractionStrategy::CUSTOM) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // PHP-specific custom logic for CUSTOM strategy and fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("function") != string::npos || node_type.find("method") != string::npos || node_type == "method_declaration") {
        return FindChildByType(node, content, "name");
    }
    if (node_type.find("class") != string::npos || node_type.find("interface") != string::npos) {
        return FindChildByType(node, content, "name");
    }
    if (node_type == "namespace_definition") {
        return FindChildByType(node, content, "namespace_name");
    }
    
    return "";
}

string PHPAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
        // For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
        return "";
    }
    
    return "";
}

bool PHPAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In PHP, check for public visibility modifier
    string node_text = ExtractNodeText(node, content);
    return node_text.find("public") != string::npos;
}

const unordered_map<string, NodeConfig>& PHPAdapter::GetNodeConfigs() const {
    return node_configs;
}


ParsingFunction PHPAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const PHPAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb