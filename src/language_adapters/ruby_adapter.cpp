#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_ruby();
}

namespace duckdb {

//==============================================================================
// Ruby Adapter implementation
//==============================================================================

// ruby_types.def carries BOTH tables (#215): DEF_TYPE for the named rules and
// DEF_TYPE_ANON for the anonymous keyword tokens that share their type string.
// The file is included twice, once with each macro live and the other expanded
// to nothing, so the two declarations of a colliding type sit next to each
// other in one file instead of in two that can drift apart.
#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},
#define DEF_TYPE_ANON(raw_type, semantic_type, name_strat, native_strat, flags)

const unordered_map<string, NodeConfig> RubyAdapter::node_configs = {
#include "../language_configs/ruby_types.def"
};

#undef DEF_TYPE
#undef DEF_TYPE_ANON

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)
#define DEF_TYPE_ANON(raw_type, semantic_type, name_strat, native_strat, flags)                                        \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> RubyAdapter::node_configs_anon = {
#include "../language_configs/ruby_types.def"
};

#undef DEF_TYPE
#undef DEF_TYPE_ANON

string RubyAdapter::GetLanguageName() const {
	return "ruby";
}

vector<string> RubyAdapter::GetAliases() const {
	return {"ruby", "rb"};
}

void RubyAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	parser_wrapper_->SetLanguage(tree_sitter_ruby(), "Ruby");
}

unique_ptr<TSParserWrapper> RubyAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	fresh_parser->SetLanguage(tree_sitter_ruby(), "Ruby");
	return fresh_parser;
}

string RubyAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string RubyAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	// Ruby-specific fallbacks
	string node_type = string(node_type_str);
	if (node_type == "method" || node_type == "singleton_method" || node_type == "class" || node_type == "module") {
		return FindChildByType(node, content, "identifier");
	} else if (node_type == "assignment") {
		return FindChildByType(node, content, "identifier");
	}

	return "";
}

string RubyAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool RubyAdapter::IsPublicNode(TSNode node, const string &content) const {
	// In Ruby, methods are public by default. Check preceding siblings for
	// a bare `private` or `protected` call that changes visibility.
	TSNode sibling = ts_node_prev_sibling(node);
	while (!ts_node_is_null(sibling)) {
		const char *sib_type = ts_node_type(sibling);
		if (strcmp(sib_type, "call") == 0 || strcmp(sib_type, "identifier") == 0) {
			string sib_text = ExtractNodeText(sibling, content);
			if (sib_text == "private" || sib_text == "protected") {
				return false;
			}
			if (sib_text == "public") {
				return true;
			}
		}
		sibling = ts_node_prev_sibling(sibling);
	}
	return true;
}

const unordered_map<string, NodeConfig> &RubyAdapter::GetNodeConfigs() const {
	return node_configs;
}

const unordered_map<string, NodeConfig> &RubyAdapter::GetAnonNodeConfigs() const {
	return node_configs_anon;
}

ParsingFunction RubyAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const RubyAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
