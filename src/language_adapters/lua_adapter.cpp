#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"

// Tree-sitter language declaration
extern "C" {
const TSLanguage *tree_sitter_lua();
}

namespace duckdb {

//==============================================================================
// Lua Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> LuaAdapter::node_configs = {
#include "../language_configs/lua_types.def"
};

#undef DEF_TYPE

string LuaAdapter::GetLanguageName() const {
	return "lua";
}

vector<string> LuaAdapter::GetAliases() const {
	return {"lua"};
}

void LuaAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_lua();
	parser_wrapper_->SetLanguage(ts_language, "Lua");
}

unique_ptr<TSParserWrapper> LuaAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_lua();
	fresh_parser->SetLanguage(ts_language, "Lua");
	return fresh_parser;
}

string LuaAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string LuaAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		if (config->name_strategy == ExtractionStrategy::CUSTOM) {
			// Lua-specific custom extraction
			string node_type = string(node_type_str);

			// For function declarations, look for the function name
			if (node_type == "function_declaration" || node_type == "local_function_declaration") {
				// In Lua, function name can be simple identifier or dot/colon index expression
				// function foo() -> identifier "foo"
				// function M.foo() -> dot_index_expression
				// function M:foo() -> method_index_expression
				string identifier = FindChildByType(node, content, "identifier");
				if (!identifier.empty()) {
					return identifier;
				}
				// Try dot_index_expression for module functions
				string dot_expr = FindChildByType(node, content, "dot_index_expression");
				if (!dot_expr.empty()) {
					return dot_expr;
				}
			}

			return "";
		} else {
			return ExtractByStrategy(node, content, config->name_strategy);
		}
	}

	// Fallback: try to find identifier child for common declaration types
	string node_type = string(node_type_str);
	if (node_type.find("declaration") != string::npos || node_type.find("definition") != string::npos) {
		return FindChildByType(node, content, "identifier");
	}

	return "";
}

string LuaAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// For backward compatibility, return empty string
		return "";
	}

	return "";
}

bool LuaAdapter::IsPublicNode(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		return false; // IS_PUBLIC not yet implemented
	}

	// In Lua, there's no built-in public/private - everything is public by default
	// unless convention (like underscore prefix) is used
	string node_type = string(node_type_str);

	// Function and variable declarations are "public" by default
	if (node_type == "function_declaration" || node_type == "variable_declaration") {
		return true;
	}

	// Local declarations are private to their scope
	if (node_type == "local_function_declaration" || node_type == "local_variable_declaration") {
		return false;
	}

	return false;
}

const unordered_map<string, NodeConfig> &LuaAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction LuaAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto lua_adapter = static_cast<const LuaAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(lua_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
