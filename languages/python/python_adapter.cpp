#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declaration
extern "C" {
const TSLanguage *tree_sitter_python();
}

namespace duckdb {

//==============================================================================
// Python Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> PythonAdapter::node_configs = {
#include "../language_configs/python_types.def"
};

#undef DEF_TYPE

string PythonAdapter::GetLanguageName() const {
	return "python";
}

vector<string> PythonAdapter::GetAliases() const {
	return {"python", "py"};
}

void PythonAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_python();
	parser_wrapper_->SetLanguage(ts_language, "Python");
}

unique_ptr<TSParserWrapper> PythonAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_python();
	fresh_parser->SetLanguage(ts_language, "Python");
	return fresh_parser;
}

string PythonAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string PythonAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		if (strcmp(node_type_str, "import_from_statement") == 0) {
			// Source module is before the 'import' keyword.
			// Check for relative_import child first (from . import X, from ..foo import X)
			TSNode relative = FindChildByTypeNode(node, "relative_import");
			if (!ts_node_is_null(relative)) {
				string prefix;
				TSNode import_prefix = FindChildByTypeNode(relative, "import_prefix");
				if (!ts_node_is_null(import_prefix)) {
					prefix = ExtractNodeText(import_prefix, content);
				}
				string module_name = FindChildByType(relative, content, "dotted_name");
				return prefix + module_name;
			}
			// Non-relative: first dotted_name child is the source module
			return FindChildByType(node, content, "dotted_name");
		}
		if (strcmp(node_type_str, "import_statement") == 0) {
			// Check for aliased_import (import os as alias) — extract original module name
			TSNode aliased = FindChildByTypeNode(node, "aliased_import");
			if (!ts_node_is_null(aliased)) {
				return FindChildByType(aliased, content, "dotted_name");
			}
			return FindChildByType(node, content, "dotted_name");
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	// Fallback: try to find identifier child for common declaration types
	string node_type = string(node_type_str);
	if (node_type.find("definition") != string::npos || node_type.find("declaration") != string::npos) {
		return FindChildByType(node, content, "identifier");
	}

	return "";
}

string PythonAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool PythonAdapter::IsPublicNode(TSNode node, const string &content) const {
	// In Python, names starting with underscore are typically private
	string name = ExtractNodeName(node, content);
	return !name.empty() && name[0] != '_';
}

const unordered_map<string, NodeConfig> &PythonAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction PythonAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto python_adapter = static_cast<const PythonAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(python_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
