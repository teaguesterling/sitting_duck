#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"

// Tree-sitter language declaration
extern "C" {
    const TSLanguage *tree_sitter_python();
}

namespace duckdb {

//==============================================================================
// Python Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> PythonAdapter::node_configs = {
    #include "../language_configs/python_types.def"
};

#undef DEF_TYPE

string PythonAdapter::GetLanguageName() const {
    return "python";
}

vector<string> PythonAdapter::GetAliases() const {
    return {"python", "py"};
}

void PythonAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_python();
    parser_wrapper_->SetLanguage(ts_language, "Python");
}

unique_ptr<TSParserWrapper> PythonAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_python();
    fresh_parser->SetLanguage(ts_language, "Python");
    return fresh_parser;
}

string PythonAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string PythonAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Fallback: try to find identifier child for common declaration types
    string node_type = string(node_type_str);
    if (node_type.find("definition") != string::npos || 
        node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string PythonAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool PythonAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In Python, names starting with underscore are typically private
    string name = ExtractNodeName(node, content);
    return !name.empty() && name[0] != '_';
}

uint8_t PythonAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* PythonAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

ParsingFunction PythonAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto python_adapter = static_cast<const PythonAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(python_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb