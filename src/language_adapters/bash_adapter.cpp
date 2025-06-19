#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_bash();
}

namespace duckdb {

//==============================================================================
// Bash Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> BashAdapter::node_configs = {
#include "../language_configs/bash_types.def"
};

#undef DEF_TYPE

string BashAdapter::GetLanguageName() const {
    return "bash";
}

vector<string> BashAdapter::GetAliases() const {
    return {"bash", "shell", "sh"};
}

void BashAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_bash();
    parser_wrapper_->SetLanguage(ts_language, "Bash");
}

unique_ptr<TSParserWrapper> BashAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_bash();
    fresh_parser->SetLanguage(ts_language, "Bash");
    return fresh_parser;
}

string BashAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string BashAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Bash-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type == "function_definition") {
        // Extract function name
        return FindChildByType(node, content, "word");
    }
    if (node_type == "variable_assignment") {
        // Extract variable name from assignment
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            if (child_type == "variable_name") {
                return ExtractNodeText(child, content);
            }
        }
    }
    if (node_type == "simple_command" || node_type == "command") {
        // Extract command name (first word)
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            if (child_type == "word") {
                return ExtractNodeText(child, content);
            }
        }
    }
    
    return "";
}

string BashAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool BashAdapter::IsPublicNode(TSNode node, const string &content) const {
    // Bash doesn't have visibility concepts - all nodes are "public"
    return true;
}

uint8_t BashAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* BashAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

ParsingFunction BashAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const BashAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb