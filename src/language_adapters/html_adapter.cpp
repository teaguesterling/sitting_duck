#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_html();
}

namespace duckdb {

//==============================================================================
// HTML Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> HTMLAdapter::node_configs = {
#include "../language_configs/html_types.def"
};

#undef DEF_TYPE

string HTMLAdapter::GetLanguageName() const {
	return "html";
}

vector<string> HTMLAdapter::GetAliases() const {
	return {"html", "htm"};
}

void HTMLAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	parser_wrapper_->SetLanguage(tree_sitter_html(), "HTML");
}

unique_ptr<TSParserWrapper> HTMLAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	fresh_parser->SetLanguage(tree_sitter_html(), "HTML");
	return fresh_parser;
}

string HTMLAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string HTMLAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Handle custom HTML strategies
		if (config->name_strategy == ExtractionStrategy::CUSTOM) {
			string node_type = string(node_type_str);
			if (node_type == "element" || node_type == "start_tag" || node_type == "end_tag") {
				return FindChildByType(node, content, "tag_name");
			} else if (node_type == "attribute") {
				return FindChildByType(node, content, "attribute_name");
			}
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	return "";
}

string HTMLAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool HTMLAdapter::IsPublicNode(TSNode node, const string &content) const {
	// In HTML, all elements are "public"
	return true;
}

const unordered_map<string, NodeConfig> &HTMLAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction HTMLAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const HTMLAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
