#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"

// Tree-sitter language declaration
extern "C" {
    const TSLanguage *tree_sitter_kotlin();
}

namespace duckdb {

//==============================================================================
// Kotlin Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> KotlinAdapter::node_configs = {
    #include "../language_configs/kotlin_types.def"
};

#undef DEF_TYPE

string KotlinAdapter::GetLanguageName() const {
    return "kotlin";
}

vector<string> KotlinAdapter::GetAliases() const {
    return {"kotlin", "kt"};
}

void KotlinAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_kotlin();
    parser_wrapper_->SetLanguage(ts_language, "Kotlin");
}

unique_ptr<TSParserWrapper> KotlinAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_kotlin();
    fresh_parser->SetLanguage(ts_language, "Kotlin");
    return fresh_parser;
}

string KotlinAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string KotlinAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        if (config->name_strategy == ExtractionStrategy::CUSTOM) {
            // Kotlin-specific custom extraction
            string node_type = string(node_type_str);
            if (node_type.find("declaration") != string::npos || 
                node_type.find("definition") != string::npos) {
                // Try simple_identifier first (Kotlin-specific)
                string identifier = FindChildByType(node, content, "simple_identifier");
                if (!identifier.empty()) {
                    return identifier;
                }
                // Fallback to regular identifier
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
        // Try simple_identifier first (Kotlin-specific)
        string identifier = FindChildByType(node, content, "simple_identifier");
        if (!identifier.empty()) {
            return identifier;
        }
        // Fallback to regular identifier
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string KotlinAdapter::ExtractNodeValue(TSNode node, const string &content) const {
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

bool KotlinAdapter::IsPublicNode(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return false; // IS_PUBLIC not yet implemented
    }
    
    // Check for Kotlin-specific public visibility
    string node_type = string(node_type_str);
    
    // In Kotlin, declarations are public by default unless marked private/internal/protected
    if (node_type.find("declaration") != string::npos) {
        // Look for visibility modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "private") == 0 || 
                strcmp(child_type, "internal") == 0 || 
                strcmp(child_type, "protected") == 0) {
                return false;
            }
        }
        return true;  // Public by default in Kotlin
    }
    
    return false;
}

const unordered_map<string, NodeConfig>& KotlinAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction KotlinAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto kotlin_adapter = static_cast<const KotlinAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(kotlin_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb