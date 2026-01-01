#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_markdown();
}

namespace duckdb {

//==============================================================================
// Markdown Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> MarkdownAdapter::node_configs = {
#include "../language_configs/markdown_types.def"
};

#undef DEF_TYPE

string MarkdownAdapter::GetLanguageName() const {
	return "markdown";
}

vector<string> MarkdownAdapter::GetAliases() const {
	return {"markdown", "md"};
}

void MarkdownAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	parser_wrapper_->SetLanguage(tree_sitter_markdown(), "Markdown");
}

unique_ptr<TSParserWrapper> MarkdownAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	fresh_parser->SetLanguage(tree_sitter_markdown(), "Markdown");
	return fresh_parser;
}

string MarkdownAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string MarkdownAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Handle custom extraction strategies
		if (config->name_strategy == ExtractionStrategy::CUSTOM) {
			string node_type = string(node_type_str);
			if (node_type == "atx_heading" || node_type == "setext_heading" || node_type == "section") {
				// For headings and sections, find the inline child that contains the heading text
				return FindChildByType(node, content, "inline");
			} else if (node_type == "link" || node_type == "image") {
				return FindChildByType(node, content, "link_text");
			} else if (node_type == "fenced_code_block") {
				return FindChildByType(node, content, "info_string");
			} else if (node_type == "link_reference_definition") {
				return FindChildByType(node, content, "link_label");
			}
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	return "";
}

string MarkdownAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool MarkdownAdapter::IsPublicNode(TSNode node, const string &content) const {
	// In Markdown, all content is essentially "public"
	return true;
}

const unordered_map<string, NodeConfig> &MarkdownAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction MarkdownAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const MarkdownAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
