#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_yaml();
}

namespace duckdb {

//==============================================================================
// YAML Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> YAMLAdapter::node_configs = {
#include "../language_configs/yaml_types.def"
};

#undef DEF_TYPE

string YAMLAdapter::GetLanguageName() const {
	return "yaml";
}

vector<string> YAMLAdapter::GetAliases() const {
	return {"yaml", "yml"};
}

void YAMLAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_yaml();
	parser_wrapper_->SetLanguage(ts_language, "YAML");
}

unique_ptr<TSParserWrapper> YAMLAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_yaml();
	fresh_parser->SetLanguage(ts_language, "YAML");
	return fresh_parser;
}

string YAMLAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string YAMLAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	// YAML-specific fallbacks
	string node_type = string(node_type_str);
	if (node_type == "block_mapping_pair" || node_type == "flow_pair") {
		// Extract the key from YAML key-value pairs
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			// Look for scalar keys
			if (child_type.find("scalar") != string::npos) {
				return ExtractNodeText(child, content);
			}
		}
	}

	return "";
}

string YAMLAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool YAMLAdapter::IsPublicNode(TSNode node, const string &content) const {
	// YAML doesn't have visibility concepts - all nodes are "public"
	return true;
}

const unordered_map<string, NodeConfig> &YAMLAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction YAMLAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const YAMLAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
