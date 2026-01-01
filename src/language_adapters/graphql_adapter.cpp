#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_graphql();
}

namespace duckdb {

//==============================================================================
// GraphQL Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> GraphQLAdapter::node_configs = {
#include "../language_configs/graphql_types.def"
};

#undef DEF_TYPE

string GraphQLAdapter::GetLanguageName() const {
	return "graphql";
}

vector<string> GraphQLAdapter::GetAliases() const {
	return {"graphql", "gql"};
}

void GraphQLAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_graphql();
	parser_wrapper_->SetLanguage(ts_language, "GraphQL");
}

unique_ptr<TSParserWrapper> GraphQLAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_graphql();
	fresh_parser->SetLanguage(ts_language, "GraphQL");
	return fresh_parser;
}

string GraphQLAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string GraphQLAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Handle CUSTOM strategy for GraphQL - look for "name" children instead of "identifier"
		if (config->name_strategy == ExtractionStrategy::CUSTOM) {
			string node_type = string(node_type_str);

			// For fragment_definition and fragment_spread, look for fragment_name first
			if (node_type == "fragment_definition" || node_type == "fragment_spread") {
				return FindChildByType(node, content, "fragment_name");
			}

			// For operation_definition, try name first, fallback to operation_type
			if (node_type == "operation_definition") {
				string name = FindChildByType(node, content, "name");
				if (!name.empty()) {
					return name;
				}
				return FindChildByType(node, content, "operation_type");
			}

			// For variable_definition, look for the variable child then its name
			if (node_type == "variable_definition") {
				return FindChildByType(node, content, "variable");
			}

			// GraphQL uses "name" nodes for identifiers, not "identifier"
			string name = FindChildByType(node, content, "name");
			if (!name.empty()) {
				return name;
			}

			return "";
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	return "";
}

string GraphQLAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		return "";
	}

	return "";
}

bool GraphQLAdapter::IsPublicNode(TSNode node, const string &content) const {
	// GraphQL doesn't have visibility concepts - all nodes are "public"
	return true;
}

const unordered_map<string, NodeConfig> &GraphQLAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction GraphQLAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const GraphQLAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
