#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"

// Tree-sitter language declaration
extern "C" {
    const TSLanguage *tree_sitter_c_sharp();
}

namespace duckdb {

//==============================================================================
// C# Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> CSharpAdapter::node_configs = {
    #include "../language_configs/csharp_types.def"
};

#undef DEF_TYPE

string CSharpAdapter::GetLanguageName() const {
    return "csharp";
}

vector<string> CSharpAdapter::GetAliases() const {
    return {"csharp", "cs", "c#"};
}

void CSharpAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_c_sharp();
    parser_wrapper_->SetLanguage(ts_language, "C#");
}

unique_ptr<TSParserWrapper> CSharpAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_c_sharp();
    fresh_parser->SetLanguage(ts_language, "C#");
    return fresh_parser;
}

string CSharpAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string CSharpAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);

    if (config) {
        if (config->name_strategy == ExtractionStrategy::CUSTOM) {
            // C#-specific custom extraction
            string node_type = string(node_type_str);
            if (node_type.find("declaration") != string::npos ||
                node_type.find("definition") != string::npos) {
                // Try to find identifier child
                return FindChildByType(node, content, "identifier");
            }
        } else {
            return ExtractByStrategy(node, content, config->name_strategy);
        }
    }

    // Fallback: try to find identifier child for common declaration types
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos ||
        node_type.find("definition") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }

    return "";
}

string CSharpAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);

    if (config) {
        // Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
        // For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
        return "";
    }

    // Default fallback
    return "";
}

bool CSharpAdapter::IsPublicNode(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);

    if (config) {
        return false; // IS_PUBLIC not yet implemented
    }

    // Check for C#-specific public visibility
    string node_type = string(node_type_str);

    // In C#, declarations need explicit public modifier (private by default for class members)
    if (node_type.find("declaration") != string::npos) {
        // Look for visibility modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "modifier") == 0) {
                // Check the modifier text
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.size() && end <= content.size()) {
                    string modifier_text = content.substr(start, end - start);
                    if (modifier_text == "public") {
                        return true;
                    }
                }
            }
        }
        return false;  // Private by default in C#
    }

    return false;
}

const unordered_map<string, NodeConfig>& CSharpAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction CSharpAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language,
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto csharp_adapter = static_cast<const CSharpAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(csharp_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb
