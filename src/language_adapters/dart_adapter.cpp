#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_dart();
}

namespace duckdb {

//==============================================================================
// Dart Adapter implementation
// Client-optimized language with sound null safety and async support
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> DartAdapter::node_configs = {
#include "../language_configs/dart_types.def"
};

#undef DEF_TYPE

string DartAdapter::GetLanguageName() const {
	return "dart";
}

vector<string> DartAdapter::GetAliases() const {
	return {"dart"};
}

void DartAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_dart();
	parser_wrapper_->SetLanguage(ts_language, "Dart");
}

unique_ptr<TSParserWrapper> DartAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_dart();
	fresh_parser->SetLanguage(ts_language, "Dart");
	return fresh_parser;
}

string DartAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string DartAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Import specifications: URI is in configurable_uri or uri child
		if (strcmp(node_type_str, "import_specification") == 0) {
			string name = FindChildByType(node, content, "configurable_uri");
			if (name.empty()) {
				name = FindChildByType(node, content, "uri");
			}
			return name;
		}
		// Part directives: URI is a direct child
		if (strcmp(node_type_str, "part_directive") == 0) {
			return FindChildByType(node, content, "uri");
		}
		// Part-of directives: either dotted identifier or URI
		if (strcmp(node_type_str, "part_of_directive") == 0) {
			string name = FindChildByType(node, content, "dotted_identifier_list");
			if (name.empty()) {
				name = FindChildByType(node, content, "uri");
			}
			return name;
		}
		// Wrapper nodes: delegate to inner import_specification or library_import
		if (strcmp(node_type_str, "library_import") == 0 || strcmp(node_type_str, "library_export") == 0) {
			uint32_t child_count = ts_node_child_count(node);
			for (uint32_t i = 0; i < child_count; i++) {
				TSNode child = ts_node_child(node, i);
				if (strcmp(ts_node_type(child), "import_specification") == 0) {
					return ExtractNodeName(child, content);
				}
			}
			return "";
		}
		if (strcmp(node_type_str, "import_or_export") == 0) {
			uint32_t child_count = ts_node_child_count(node);
			for (uint32_t i = 0; i < child_count; i++) {
				TSNode child = ts_node_child(node, i);
				const char *child_type = ts_node_type(child);
				if (strcmp(child_type, "library_import") == 0 || strcmp(child_type, "library_export") == 0) {
					return ExtractNodeName(child, content);
				}
			}
			return "";
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	return "";
}

string DartAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	string node_type = string(node_type_str);

	// For literal types, extract the value
	if (node_type == "decimal_integer_literal" || node_type == "hex_integer_literal" ||
	    node_type == "decimal_floating_point_literal" || node_type == "string_literal" || node_type == "null_literal" ||
	    node_type == "true" || node_type == "false" || node_type == "symbol_literal") {
		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);
		return content.substr(start, end - start);
	}

	return "";
}

bool DartAdapter::IsPublicNode(TSNode node, const string &content) const {
	// In Dart, declarations are public by default
	// Private declarations start with underscore '_' in the name
	// Check if this is a named node and if its name starts with underscore

	// Try to find an identifier child that represents the name
	uint32_t child_count = ts_node_named_child_count(node);
	for (uint32_t i = 0; i < child_count; i++) {
		TSNode child = ts_node_named_child(node, i);
		const char *child_type = ts_node_type(child);
		if (strcmp(child_type, "identifier") == 0) {
			uint32_t start = ts_node_start_byte(child);
			uint32_t end = ts_node_end_byte(child);
			if (start < end && start < content.size()) {
				// Check if identifier starts with underscore (private in Dart)
				if (content[start] == '_') {
					return false;
				}
			}
			break;
		}
	}

	// Default to public
	return true;
}

const unordered_map<string, NodeConfig> &DartAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction DartAdapter::GetParsingFunction() const {
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const DartAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
