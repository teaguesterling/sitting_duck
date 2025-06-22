#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_rust();
}

namespace duckdb {

//==============================================================================
// Rust Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> RustAdapter::node_configs = {
#include "../language_configs/rust_types.def"
};

#undef DEF_TYPE

string RustAdapter::GetLanguageName() const {
    return "rust";
}

vector<string> RustAdapter::GetAliases() const {
    return {"rust", "rs"};
}

void RustAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_rust();
    parser_wrapper_->SetLanguage(ts_language, "Rust");
}

unique_ptr<TSParserWrapper> RustAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_rust();
    fresh_parser->SetLanguage(ts_language, "Rust");
    return fresh_parser;
}

string RustAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string RustAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config && config->name_strategy != ExtractionStrategy::CUSTOM) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Rust-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("function") != string::npos || node_type == "function_item") {
        return FindChildByType(node, content, "identifier");
    }
    if (node_type.find("struct") != string::npos || node_type == "struct_item") {
        return FindChildByType(node, content, "type_identifier");
    }
    if (node_type.find("enum") != string::npos || node_type == "enum_item") {
        return FindChildByType(node, content, "type_identifier");
    }
    if (node_type.find("trait") != string::npos || node_type == "trait_item") {
        return FindChildByType(node, content, "type_identifier");
    }
    if (node_type.find("impl") != string::npos || node_type == "impl_item") {
        return FindChildByType(node, content, "type_identifier");
    }
    if (node_type.find("mod") != string::npos || node_type == "mod_item") {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string RustAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool RustAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In Rust, check for pub visibility modifier
    string node_text = ExtractNodeText(node, content);
    return node_text.find("pub") != string::npos;
}

const unordered_map<string, NodeConfig>& RustAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction RustAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const RustAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb