#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "node_config.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_typescript();
}

namespace duckdb {

//==============================================================================
// TypeScript Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> TypeScriptAdapter::node_configs = {
// TypeScript-specific type definitions (includes JavaScript as base)
#include "../language_configs/typescript_types.def"
};

#undef DEF_TYPE

string TypeScriptAdapter::GetLanguageName() const {
	return "typescript";
}

vector<string> TypeScriptAdapter::GetAliases() const {
	return {"typescript", "ts"};
}

void TypeScriptAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_typescript();
	parser_wrapper_->SetLanguage(ts_language, "TypeScript");
}

unique_ptr<TSParserWrapper> TypeScriptAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_typescript();
	fresh_parser->SetLanguage(ts_language, "TypeScript");
	return fresh_parser;
}

string TypeScriptAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string TypeScriptAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Import statements: module source is in a string child
		if (strcmp(node_type_str, "import_statement") == 0 || strcmp(node_type_str, "import_require_clause") == 0) {
			return FindChildByType(node, content, "string");
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	// TypeScript-specific fallbacks
	string node_type = string(node_type_str);
	if (node_type.find("declaration") != string::npos) {
		return FindChildByType(node, content, "identifier");
	}

	return "";
}

string TypeScriptAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool TypeScriptAdapter::IsPublicNode(TSNode node, const string &content) const {
	// In TypeScript, check for explicit access modifiers and export statements
	const char *node_type_str = ts_node_type(node);
	string node_type = string(node_type_str);

	// Check if it's an export declaration
	if (node_type.find("export") != string::npos) {
		return true;
	}

	// Check parent nodes for export context
	TSNode parent = ts_node_parent(node);
	if (!ts_node_is_null(parent)) {
		const char *parent_type = ts_node_type(parent);
		if (string(parent_type).find("export") != string::npos) {
			return true;
		}
	}

	// Check for explicit access modifiers in the node text
	uint32_t start_byte = ts_node_start_byte(node);
	uint32_t end_byte = ts_node_end_byte(node);
	if (start_byte < content.size() && end_byte <= content.size()) {
		string node_text = content.substr(start_byte, end_byte - start_byte);

		// Look for explicit private/protected keywords
		if (node_text.find("private ") != string::npos || node_text.find("protected ") != string::npos) {
			return false;
		}

		// Look for explicit public keyword
		if (node_text.find("public ") != string::npos) {
			return true;
		}
	}

	// Check naming conventions - underscore prefix typically indicates private
	string name = ExtractNodeName(node, content);
	if (!name.empty() && name[0] == '_') {
		return false;
	}

	// Default to public (TypeScript default visibility)
	return true;
}

const unordered_map<string, NodeConfig> &TypeScriptAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction TypeScriptAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const TypeScriptAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
