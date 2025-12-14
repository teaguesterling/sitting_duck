#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_toml();
}

namespace duckdb {

//==============================================================================
// TOML (Tom's Obvious Minimal Language) Adapter implementation
// Used by Cargo.toml, pyproject.toml, Hugo, etc.
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> TOMLAdapter::node_configs = {
#include "../language_configs/toml_types.def"
};

#undef DEF_TYPE

string TOMLAdapter::GetLanguageName() const {
    return "toml";
}

vector<string> TOMLAdapter::GetAliases() const {
    return {"toml"};
}

void TOMLAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_toml();
    parser_wrapper_->SetLanguage(ts_language, "TOML");
}

unique_ptr<TSParserWrapper> TOMLAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_toml();
    fresh_parser->SetLanguage(ts_language, "TOML");
    return fresh_parser;
}

string TOMLAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

// Helper to extract key text from bare_key, quoted_key, or dotted_key
string TOMLAdapter::ExtractKeyText(TSNode key_node, const string &content) const {
    const char* key_type = ts_node_type(key_node);

    if (strcmp(key_type, "bare_key") == 0) {
        // Simple key - just extract the text
        uint32_t start = ts_node_start_byte(key_node);
        uint32_t end = ts_node_end_byte(key_node);
        return content.substr(start, end - start);
    }

    if (strcmp(key_type, "quoted_key") == 0) {
        // Quoted key - extract text including quotes (or strip them)
        uint32_t start = ts_node_start_byte(key_node);
        uint32_t end = ts_node_end_byte(key_node);
        string text = content.substr(start, end - start);
        // Strip surrounding quotes if present
        if (text.size() >= 2 && (text[0] == '"' || text[0] == '\'')) {
            return text.substr(1, text.size() - 2);
        }
        return text;
    }

    if (strcmp(key_type, "dotted_key") == 0) {
        // Dotted key - join all parts with dots
        string result;
        uint32_t child_count = ts_node_named_child_count(key_node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_named_child(key_node, i);
            string part = ExtractKeyText(child, content);
            if (!part.empty()) {
                if (!result.empty()) {
                    result += ".";
                }
                result += part;
            }
        }
        return result;
    }

    // Fallback - extract raw text
    uint32_t start = ts_node_start_byte(key_node);
    uint32_t end = ts_node_end_byte(key_node);
    return content.substr(start, end - start);
}

string TOMLAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);

    if (config && config->name_strategy == ExtractionStrategy::CUSTOM) {
        string node_type = string(node_type_str);

        if (node_type == "table" || node_type == "table_array_element") {
            // For tables [section] or [[array_section]], extract the key
            // The key is the first child (bare_key, quoted_key, or dotted_key)
            uint32_t child_count = ts_node_named_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_named_child(node, i);
                const char* child_type = ts_node_type(child);
                if (strcmp(child_type, "bare_key") == 0 ||
                    strcmp(child_type, "quoted_key") == 0 ||
                    strcmp(child_type, "dotted_key") == 0) {
                    return ExtractKeyText(child, content);
                }
            }
            return "";
        }

        if (node_type == "pair") {
            // For key-value pairs, extract the key (first child)
            uint32_t child_count = ts_node_named_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_named_child(node, i);
                const char* child_type = ts_node_type(child);
                if (strcmp(child_type, "bare_key") == 0 ||
                    strcmp(child_type, "quoted_key") == 0 ||
                    strcmp(child_type, "dotted_key") == 0) {
                    return ExtractKeyText(child, content);
                }
            }
            return "";
        }

        if (node_type == "dotted_key") {
            return ExtractKeyText(node, content);
        }
    }

    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }

    return "";
}

string TOMLAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);

    // For literal types, extract the value
    if (node_type == "string" || node_type == "integer" || node_type == "float" ||
        node_type == "boolean" || node_type == "local_date" || node_type == "local_time" ||
        node_type == "local_date_time" || node_type == "offset_date_time") {
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        return content.substr(start, end - start);
    }

    return "";
}

bool TOMLAdapter::IsPublicNode(TSNode node, const string &content) const {
    // TOML doesn't have visibility concepts - all nodes are "public"
    return true;
}

const unordered_map<string, NodeConfig>& TOMLAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction TOMLAdapter::GetParsingFunction() const {
    return [](const void* adapter, const string& content, const string& language,
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const TOMLAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb
