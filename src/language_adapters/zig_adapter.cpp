#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_zig();
}

namespace duckdb {

//==============================================================================
// Zig Adapter implementation
// Modern systems programming language with focus on safety and performance
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> ZigAdapter::node_configs = {
#include "../language_configs/zig_types.def"
};

#undef DEF_TYPE

string ZigAdapter::GetLanguageName() const {
	return "zig";
}

vector<string> ZigAdapter::GetAliases() const {
	return {"zig"};
}

void ZigAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_zig();
	parser_wrapper_->SetLanguage(ts_language, "Zig");
}

unique_ptr<TSParserWrapper> ZigAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_zig();
	fresh_parser->SetLanguage(ts_language, "Zig");
	return fresh_parser;
}

string ZigAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string ZigAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		if (config->name_strategy == ExtractionStrategy::CUSTOM) {
			string node_type = string(node_type_str);

			// Type declarations: name lives in parent variable_declaration
			// Pattern: `const Point = struct { }` -> name is "Point"
			if (node_type == "struct_declaration" || node_type == "enum_declaration" ||
			    node_type == "union_declaration" || node_type == "opaque_declaration" ||
			    node_type == "error_set_declaration") {
				TSNode parent = ts_node_parent(node);
				if (!ts_node_is_null(parent)) {
					const char *parent_type = ts_node_type(parent);
					if (strcmp(parent_type, "variable_declaration") == 0) {
						return FindChildByType(parent, content, "identifier");
					}
				}
				return "";
			}

			// Test declarations: name is a string literal
			// Pattern: `test "basic test" { }` -> name is "basic test"
			if (node_type == "test_declaration") {
				// Try string_content first (the inner text without quotes)
				string name = FindChildByType(node, content, "string_content");
				if (!name.empty()) {
					return name;
				}
				// Fall back to string child and strip quotes
				name = FindChildByType(node, content, "string");
				if (name.size() >= 2 && name.front() == '"' && name.back() == '"') {
					return name.substr(1, name.size() - 2);
				}
				return name;
			}

			return "";
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	return "";
}

string ZigAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	string node_type = string(node_type_str);

	// For literal types, extract the value
	if (node_type == "integer" || node_type == "float" || node_type == "string" || node_type == "multiline_string" ||
	    node_type == "character" || node_type == "boolean" || node_type == "null" || node_type == "undefined") {
		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);
		return content.substr(start, end - start);
	}

	return "";
}

bool ZigAdapter::IsPublicNode(TSNode node, const string &content) const {
	// In Zig, public declarations are marked with 'pub' keyword
	// Check if parent or sibling has 'pub' modifier
	TSNode parent = ts_node_parent(node);
	if (!ts_node_is_null(parent)) {
		uint32_t child_count = ts_node_child_count(parent);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode sibling = ts_node_child(parent, i);
			const char *sibling_type = ts_node_type(sibling);
			if (strcmp(sibling_type, "pub") == 0) {
				return true;
			}
		}
	}
	// Default to public if no visibility modifier found
	return true;
}

const unordered_map<string, NodeConfig> &ZigAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction ZigAdapter::GetParsingFunction() const {
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const ZigAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
