#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_hcl();
}

namespace duckdb {

//==============================================================================
// HCL (HashiCorp Configuration Language) Adapter implementation
// Used by Terraform, Vault, Nomad, Waypoint, etc.
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> HCLAdapter::node_configs = {
#include "../language_configs/hcl_types.def"
};

#undef DEF_TYPE

string HCLAdapter::GetLanguageName() const {
    return "hcl";
}

vector<string> HCLAdapter::GetAliases() const {
    return {"hcl", "terraform", "tf", "tfvars"};
}

void HCLAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_hcl();
    parser_wrapper_->SetLanguage(ts_language, "HCL");
}

unique_ptr<TSParserWrapper> HCLAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_hcl();
    fresh_parser->SetLanguage(ts_language, "HCL");
    return fresh_parser;
}

string HCLAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string HCLAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);

    if (config) {
        if (config->name_strategy == ExtractionStrategy::CUSTOM) {
            // HCL-specific custom extraction
            string node_type = string(node_type_str);

            if (node_type == "block") {
                // For HCL blocks, extract the labels (not the block type)
                // e.g., resource "aws_instance" "example" -> "aws_instance.example"
                uint32_t child_count = ts_node_named_child_count(node);
                string result;
                bool found_type = false;

                for (uint32_t i = 0; i < child_count; i++) {
                    TSNode child = ts_node_named_child(node, i);
                    const char* child_type = ts_node_type(child);

                    if (strcmp(child_type, "identifier") == 0) {
                        if (!found_type) {
                            // Skip the block type identifier (resource, variable, etc.)
                            found_type = true;
                            continue;
                        }
                        // Additional identifiers are labels
                        uint32_t start = ts_node_start_byte(child);
                        uint32_t end = ts_node_end_byte(child);
                        string text = content.substr(start, end - start);
                        if (result.empty()) {
                            result = text;
                        } else {
                            result += "." + text;
                        }
                    } else if (strcmp(child_type, "string_lit") == 0) {
                        // String literals contain labels - extract from template_literal child
                        uint32_t str_child_count = ts_node_named_child_count(child);
                        for (uint32_t j = 0; j < str_child_count; j++) {
                            TSNode str_child = ts_node_named_child(child, j);
                            if (strcmp(ts_node_type(str_child), "template_literal") == 0) {
                                uint32_t start = ts_node_start_byte(str_child);
                                uint32_t end = ts_node_end_byte(str_child);
                                string text = content.substr(start, end - start);
                                if (result.empty()) {
                                    result = text;
                                } else {
                                    result += "." + text;
                                }
                                break;
                            }
                        }
                    } else if (strcmp(child_type, "body") == 0 || strcmp(child_type, "block_start") == 0) {
                        // Stop when we hit the body or block start
                        break;
                    }
                }
                return result;
            }
            // Fall through for other CUSTOM types
        }
        return ExtractByStrategy(node, content, config->name_strategy);
    }

    // HCL-specific fallbacks for types without config
    string node_type = string(node_type_str);

    if (node_type == "attribute") {
        // For attributes, extract the key name (first identifier child)
        uint32_t child_count = ts_node_named_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_named_child(node, i);
            const char* child_type = ts_node_type(child);
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                return content.substr(start, end - start);
            }
        }
    }

    if (node_type == "function_call") {
        // For function calls, extract the function name (first identifier)
        uint32_t child_count = ts_node_named_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_named_child(node, i);
            const char* child_type = ts_node_type(child);
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                return content.substr(start, end - start);
            }
        }
    }

    if (node_type == "variable_expr") {
        // For variable expressions, extract the identifier
        uint32_t child_count = ts_node_named_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_named_child(node, i);
            const char* child_type = ts_node_type(child);
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                return content.substr(start, end - start);
            }
        }
    }

    return "";
}

string HCLAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);

    if (config) {
        // For literals, extract the value
        string node_type = string(node_type_str);
        if (node_type == "numeric_lit" || node_type == "bool_lit" ||
            node_type == "true" || node_type == "false" || node_type == "null_lit") {
            uint32_t start = ts_node_start_byte(node);
            uint32_t end = ts_node_end_byte(node);
            return content.substr(start, end - start);
        }
    }

    return "";
}

bool HCLAdapter::IsPublicNode(TSNode node, const string &content) const {
    // HCL doesn't have visibility concepts - all nodes are "public"
    // Everything in a config file is accessible
    return true;
}

const unordered_map<string, NodeConfig>& HCLAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction HCLAdapter::GetParsingFunction() const {
    return [](const void* adapter, const string& content, const string& language,
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const HCLAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}

} // namespace duckdb
