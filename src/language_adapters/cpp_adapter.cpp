#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_cpp();
}

namespace duckdb {

//==============================================================================
// C++ Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> CPPAdapter::node_configs = {
#include "../language_configs/cpp_types.def"
};

#undef DEF_TYPE

string CPPAdapter::GetLanguageName() const {
	return "cpp";
}

vector<string> CPPAdapter::GetAliases() const {
	return {"cpp", "c++", "cxx", "cc"};
}

void CPPAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_cpp();
	parser_wrapper_->SetLanguage(ts_language, "C++");
}

unique_ptr<TSParserWrapper> CPPAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_cpp();
	fresh_parser->SetLanguage(ts_language, "C++");
	return fresh_parser;
}

string CPPAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string CPPAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Include directives: extract just the path child, not full directive text
		if (strcmp(node_type_str, "preproc_include") == 0) {
			string name = FindChildByType(node, content, "system_lib_string");
			if (name.empty()) {
				name = FindChildByType(node, content, "string_literal");
			}
			return name;
		}
		// Using declarations: extract the namespace/type being imported
		if (strcmp(node_type_str, "using_declaration") == 0) {
			string name = FindChildByType(node, content, "qualified_identifier");
			if (name.empty()) {
				name = FindChildByType(node, content, "identifier");
			}
			return name;
		}
		if (config->name_strategy == ExtractionStrategy::CUSTOM) {
			// Custom C++ name extraction logic
			return ExtractCppCustomName(node, content, node_type_str);
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	// C++-specific fallbacks
	string node_type = string(node_type_str);
	if (node_type.find("specifier") != string::npos || node_type.find("definition") != string::npos) {
		// Try multiple identifier types in order of preference
		string result = FindChildByType(node, content, "identifier");
		if (result.empty()) {
			result = FindChildByType(node, content, "qualified_identifier");
		}
		if (result.empty()) {
			result = FindChildByType(node, content, "type_identifier");
		}
		return result;
	}

	return "";
}

string CPPAdapter::ExtractCppCustomName(TSNode node, const string &content, const char *node_type_str) const {
	string node_type = string(node_type_str);

	if (node_type == "function_definition") {
		// For function_definition: Look for function_declarator, possibly inside
		// reference_declarator or pointer_declarator wrappers (for return-type decorators)
		// e.g., Calculator& operator+=(int value) has function_declarator inside reference_declarator

		// Helper: extract name from a function_declarator node
		auto extractFromFuncDecl = [&](TSNode func_decl) -> string {
			uint32_t decl_child_count = ts_node_child_count(func_decl);
			for (uint32_t j = 0; j < decl_child_count; j++) {
				TSNode decl_child = ts_node_child(func_decl, j);
				const char *decl_child_type = ts_node_type(decl_child);

				if (strcmp(decl_child_type, "identifier") == 0) {
					return ExtractNodeText(decl_child, content);
				} else if (strcmp(decl_child_type, "field_identifier") == 0) {
					// Class method defined outside class: void MyClass::method() {}
					return ExtractNodeText(decl_child, content);
				} else if (strcmp(decl_child_type, "operator_name") == 0) {
					// Operator overload: operator+=(int value)
					return ExtractNodeText(decl_child, content);
				} else if (strcmp(decl_child_type, "destructor_name") == 0) {
					// Destructor: ~ClassName()
					return ExtractNodeText(decl_child, content);
				} else if (strcmp(decl_child_type, "qualified_identifier") == 0) {
					// Qualified name like ClassName::methodName
					// Extract just the last identifier (the function name part)
					uint32_t qual_child_count = ts_node_child_count(decl_child);
					for (int k = qual_child_count - 1; k >= 0; k--) {
						TSNode qual_child = ts_node_child(decl_child, k);
						const char *qual_child_type = ts_node_type(qual_child);

						if (strcmp(qual_child_type, "identifier") == 0 ||
						    strcmp(qual_child_type, "operator_name") == 0 ||
						    strcmp(qual_child_type, "destructor_name") == 0) {
							return ExtractNodeText(qual_child, content);
						}
					}
					// Fallback to full qualified name
					return ExtractNodeText(decl_child, content);
				}
			}
			return string();
		};

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "function_declarator") == 0) {
				string result = extractFromFuncDecl(child);
				if (!result.empty()) {
					return result;
				}
			}
			// Check for function_declarator inside wrapper types
			// (reference_declarator for T& return, pointer_declarator for T* return)
			if (strcmp(child_type, "reference_declarator") == 0 ||
			    strcmp(child_type, "pointer_declarator") == 0) {
				// Search for function_declarator inside the wrapper (may be nested)
				TSNode wrapper = child;
				for (int depth = 0; depth < 3; depth++) {
					uint32_t wrapper_child_count = ts_node_child_count(wrapper);
					bool found_deeper = false;
					for (uint32_t w = 0; w < wrapper_child_count; w++) {
						TSNode wchild = ts_node_child(wrapper, w);
						const char *wchild_type = ts_node_type(wchild);
						if (strcmp(wchild_type, "function_declarator") == 0) {
							string result = extractFromFuncDecl(wchild);
							if (!result.empty()) {
								return result;
							}
						}
						if (strcmp(wchild_type, "reference_declarator") == 0 ||
						    strcmp(wchild_type, "pointer_declarator") == 0) {
							wrapper = wchild;
							found_deeper = true;
							break;
						}
					}
					if (!found_deeper) {
						break;
					}
				}
			}
		}
	}

	// Fallback to existing logic
	if (node_type.find("specifier") != string::npos || node_type.find("definition") != string::npos) {
		// Try multiple identifier types in order of preference
		string result = FindChildByType(node, content, "identifier");
		if (result.empty()) {
			result = FindChildByType(node, content, "qualified_identifier");
		}
		if (result.empty()) {
			result = FindChildByType(node, content, "type_identifier");
		}
		return result;
	}

	return "";
}

string CPPAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool CPPAdapter::IsPublicNode(TSNode node, const string &content) const {
	// In C++, check for access specifiers and scope
	const char *node_type_str = ts_node_type(node);
	string node_type = string(node_type_str);

	// Functions and classes at namespace/global scope are generally public
	if (node_type == "function_definition" || node_type == "function_declarator" || node_type == "class_definition") {
		// Check if it's in a namespace (likely public API)
		TSNode parent = ts_node_parent(node);
		while (!ts_node_is_null(parent)) {
			const char *parent_type = ts_node_type(parent);
			if (string(parent_type) == "namespace_definition") {
				return true; // In namespace = public API
			}
			if (string(parent_type) == "class_specifier" || string(parent_type) == "struct_specifier") {
				break; // Inside class, need to check access specifier
			}
			parent = ts_node_parent(parent);
		}

		// If at global scope, consider public
		if (ts_node_is_null(parent) || string(ts_node_type(parent)) == "translation_unit") {
			return true;
		}
	}

	// For class members, check for access specifiers in surrounding context
	// This is simplified - real implementation would track access specifier state
	TSNode sibling = ts_node_prev_sibling(node);
	while (!ts_node_is_null(sibling)) {
		const char *sibling_type = ts_node_type(sibling);
		if (string(sibling_type) == "access_specifier") {
			string specifier_text = ExtractNodeText(sibling, content);
			if (specifier_text.find("public") != string::npos) {
				return true;
			}
			if (specifier_text.find("private") != string::npos || specifier_text.find("protected") != string::npos) {
				return false;
			}
		}
		sibling = ts_node_prev_sibling(sibling);
	}

	// Check naming conventions - underscore suffix often indicates private/internal
	string name = ExtractNodeName(node, content);
	if (!name.empty() && name.back() == '_') {
		return false;
	}

	// Default to public for C++ (conservative approach)
	return true;
}

const unordered_map<string, NodeConfig> &CPPAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction CPPAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const CPPAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
