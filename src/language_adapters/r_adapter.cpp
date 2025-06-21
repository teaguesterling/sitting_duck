#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_r();
}

namespace duckdb {

//==============================================================================
// R Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> RAdapter::node_configs = {
    #include "../language_configs/r_types.def"
};

#undef DEF_TYPE

string RAdapter::GetLanguageName() const {
    return "r";
}

vector<string> RAdapter::GetAliases() const {
    return {"r", "R"};
}

void RAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_r(), "R");
}

unique_ptr<TSParserWrapper> RAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_r(), "R");
    return fresh_parser;
}

string RAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string RAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // R-specific fallback patterns
    string node_type = string(node_type_str);
    
    // Function definitions: look for identifier in parent assignment
    if (node_type == "function_definition") {
        // In R, function definitions typically have the pattern: name <- function(...)
        // The function_definition node is a child of a binary_operator (assignment)
        // We need to find the identifier on the left side of the assignment
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            if (parent_type == "binary_operator") {
                // Look for the left operand which should be an identifier
                TSNode left_operand = ts_node_child(parent, 0);
                if (!ts_node_is_null(left_operand) && 
                    string(ts_node_type(left_operand)) == "identifier") {
                    return ExtractNodeText(left_operand, content);
                }
            }
        }
        return "";
    }
    
    // Parameters and arguments
    if (node_type == "parameter" || node_type == "argument") {
        return FindChildByType(node, content, "identifier");
    }
    
    // Function calls: get the function name
    if (node_type == "call") {
        // First child should be the function being called
        TSNode function_name = ts_node_child(node, 0);
        if (!ts_node_is_null(function_name)) {
            return ExtractNodeText(function_name, content);
        }
    }
    
    return "";
}

string RAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool RAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In R, most objects are public by default unless they start with a dot
    string name = ExtractNodeName(node, content);
    if (name.empty()) {
        return false;
    }
    
    // Objects starting with '.' are considered private/internal in R
    if (name[0] == '.') {
        return false;
    }
    
    // Check for common private naming patterns
    if (name.find("..") == 0) {  // Names starting with '..' are typically internal
        return false;
    }
    
    // Everything else is considered public in R
    return true;
}

uint8_t RAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* RAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

ParsingFunction RAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const RAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb