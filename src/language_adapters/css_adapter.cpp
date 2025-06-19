#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_css();
}

namespace duckdb {

//==============================================================================
// CSS Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> CSSAdapter::node_configs = {
    #include "../language_configs/css_types.def"
};

#undef DEF_TYPE

string CSSAdapter::GetLanguageName() const {
    return "css";
}

vector<string> CSSAdapter::GetAliases() const {
    return {"css"};
}

void CSSAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_css(), "CSS");
}

unique_ptr<TSParserWrapper> CSSAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_css(), "CSS");
    return fresh_parser;
}

string CSSAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string CSSAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Handle custom CSS strategies
        if (config->name_strategy == ExtractionStrategy::CUSTOM) {
            string node_type = string(node_type_str);
            if (node_type == "declaration") {
                return FindChildByType(node, content, "property_name");
            } else if (node_type == "call_expression") {
                return FindChildByType(node, content, "function_name");
            } else if (node_type == "at_rule") {
                // Extract the @ keyword (e.g., @media, @import)
                return FindChildByType(node, content, "at_keyword");
            } else if (node_type == "import_statement" || node_type == "charset_statement") {
                // Extract the string value
                return FindChildByType(node, content, "string_value");
            } else if (node_type == "keyframe_block") {
                // Extract percentage or from/to keyword
                string percentage = FindChildByType(node, content, "integer_value");
                if (percentage.empty()) {
                    percentage = FindChildByType(node, content, "from");
                }
                if (percentage.empty()) {
                    percentage = FindChildByType(node, content, "to");
                }
                return percentage;
            }
        }
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    return "";
}

string CSSAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Handle custom CSS strategies
        if (config->value_strategy == ExtractionStrategy::CUSTOM) {
            string node_type = string(node_type_str);
            if (node_type == "declaration") {
                // Extract all values after the property name
                // This is simplified - in reality we'd need to collect all value nodes
                return ExtractNodeText(node, content);  // Fallback to full text for now
            }
        }
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool CSSAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In CSS, all rules are "public"
    return true;
}

uint8_t CSSAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* CSSAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

ParsingFunction CSSAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const CSSAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}




} // namespace duckdb
