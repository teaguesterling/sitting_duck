#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

namespace duckdb {

//==============================================================================
// Dynamic (runtime-registered) language adapter implementation
//==============================================================================

DynamicLanguageAdapter::DynamicLanguageAdapter(shared_ptr<const DynamicLanguageInfo> info) : info_(std::move(info)) {
}

string DynamicLanguageAdapter::GetLanguageName() const {
	return info_->name;
}

vector<string> DynamicLanguageAdapter::GetAliases() const {
	vector<string> aliases;
	aliases.push_back(info_->name);
	aliases.insert(aliases.end(), info_->aliases.begin(), info_->aliases.end());
	return aliases;
}

void DynamicLanguageAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	parser_wrapper_->SetLanguage(info_->language, info_->name);
}

unique_ptr<TSParserWrapper> DynamicLanguageAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	fresh_parser->SetLanguage(info_->language, info_->name);
	return fresh_parser;
}

string DynamicLanguageAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string DynamicLanguageAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	// Heuristics for unconfigured node types: prefer the grammar's own "name"
	// field, then an identifier child for definition/declaration-like types
	TSNode name_child = ts_node_child_by_field_name(node, "name", 4);
	if (!ts_node_is_null(name_child)) {
		return ExtractNodeText(name_child, content);
	}
	// strstr on the raw type: this runs for every unconfigured node, so avoid
	// allocating a string per node
	if (strstr(node_type_str, "definition") || strstr(node_type_str, "declaration")) {
		return FindChildByType(node, content, "identifier");
	}
	return "";
}

string DynamicLanguageAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	return "";
}

bool DynamicLanguageAdapter::IsPublicNode(TSNode node, const string &content) const {
	// No visibility conventions are known for runtime-registered grammars
	return true;
}

const unordered_map<string, NodeConfig> &DynamicLanguageAdapter::GetNodeConfigs() const {
	return info_->node_configs;
}

ParsingFunction DynamicLanguageAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto dynamic_adapter = static_cast<const DynamicLanguageAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(dynamic_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
