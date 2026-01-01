#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// C-Specific Native Context Extractors
//==============================================================================

// Forward declaration for CAdapter
class CAdapter;

// Base template for C extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct CNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for FUNCTION_WITH_PARAMS (C functions)
template <>
struct CNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract return type from function declaration/definition
		context.signature_type = ExtractCReturnType(node, content);

		// Extract function parameters with C type annotations
		context.parameters = ExtractCParameters(node, content);

		// Extract function modifiers (static, inline, extern, etc.)
		auto modifiers = ExtractCModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static string ExtractCReturnType(TSNode node, const string &content) {
		// Look for type specifier before function name
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "primitive_type") == 0 || strcmp(child_type, "type_identifier") == 0 ||
			    strcmp(child_type, "struct_specifier") == 0 || strcmp(child_type, "union_specifier") == 0 ||
			    strcmp(child_type, "enum_specifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}
		return ""; // No explicit return type (defaults to int in C)
	}

	static vector<ParameterInfo> ExtractCParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find function_declarator, then parameter_list inside it
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "function_declarator") == 0) {
				// Look for parameter_list inside function_declarator
				uint32_t declarator_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < declarator_count; j++) {
					TSNode declarator_child = ts_node_child(child, j);
					const char *declarator_child_type = ts_node_type(declarator_child);

					if (strcmp(declarator_child_type, "parameter_list") == 0) {
						params = ExtractCParametersDirect(declarator_child, content);
						return params;
					}
				}
			}
		}

		return params;
	}

	static vector<ParameterInfo> ExtractCParametersDirect(TSNode params_node, const string &content) {
		vector<ParameterInfo> parameters;

		uint32_t child_count = ts_node_child_count(params_node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(params_node, i);
			const char *child_type = ts_node_type(child);

			ParameterInfo param;
			bool is_valid_param = false;

			if (strcmp(child_type, "parameter_declaration") == 0) {
				// Standard parameter: (Type param) or (Type param[])
				param = ExtractCParameterDeclaration(child, content);
				is_valid_param = !param.type.empty();
			} else if (strcmp(child_type, "variadic_parameter") == 0) {
				// Variadic parameter: (...)
				param.is_variadic = true;
				param.name = "...";
				param.type = "variadic";
				is_valid_param = true;
			}

			if (is_valid_param) {
				parameters.push_back(param);
			}
		}

		return parameters;
	}

	static ParameterInfo ExtractCParameterDeclaration(TSNode node, const string &content) {
		ParameterInfo param;
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "primitive_type") == 0 || strcmp(child_type, "type_identifier") == 0 ||
			    strcmp(child_type, "struct_specifier") == 0 || strcmp(child_type, "union_specifier") == 0) {
				// Parameter type
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.type = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "identifier") == 0) {
				// Parameter name
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "array_declarator") == 0) {
				// Array parameter: param[]
				param = ExtractCArrayParameter(child, content, param);
			} else if (strcmp(child_type, "pointer_declarator") == 0) {
				// Pointer parameter: *param
				param = ExtractCPointerParameter(child, content, param);
			}
		}

		// If no name found, generate a default one
		if (param.name.empty() && !param.type.empty()) {
			param.name = "param";
		}

		return param;
	}

	static ParameterInfo ExtractCArrayParameter(TSNode node, const string &content, ParameterInfo existing_param) {
		ParameterInfo param = existing_param;

		// Find the identifier within array declarator
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start);
				}
			}
		}

		// Modify type to indicate array
		if (!param.type.empty()) {
			param.type += "[]";
		}

		return param;
	}

	static ParameterInfo ExtractCPointerParameter(TSNode node, const string &content, ParameterInfo existing_param) {
		ParameterInfo param = existing_param;

		// Find the identifier within pointer declarator
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start);
				}
			}
		}

		// Modify type to indicate pointer
		if (!param.type.empty()) {
			param.type += "*";
		}

		return param;
	}

	static vector<string> ExtractCModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check for function specifiers
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			uint32_t parent_count = ts_node_child_count(parent);
			for (uint32_t i = 0; i < parent_count; i++) {
				TSNode sibling = ts_node_child(parent, i);
				const char *sibling_type = ts_node_type(sibling);

				if (strcmp(sibling_type, "storage_class_specifier") == 0 ||
				    strcmp(sibling_type, "function_specifier") == 0) {
					uint32_t start = ts_node_start_byte(sibling);
					uint32_t end = ts_node_end_byte(sibling);
					if (start < content.length() && end <= content.length()) {
						modifiers.push_back(content.substr(start, end - start));
					}
				}
			}
		}

		return modifiers;
	}
};

// Specialization for VARIABLE_WITH_TYPE (C variable declarations)
template <>
struct CNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract C variable type
		context.signature_type = ExtractCVariableType(node, content);

		// Extract variable declaration modifiers (static, extern, const, etc.)
		auto modifiers = ExtractCVariableModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static string ExtractCVariableType(TSNode node, const string &content) {
		// Look for type specifier in variable declaration
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "primitive_type") == 0 || strcmp(child_type, "type_identifier") == 0 ||
			    strcmp(child_type, "struct_specifier") == 0 || strcmp(child_type, "union_specifier") == 0 ||
			    strcmp(child_type, "enum_specifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}

		return "";
	}

	static vector<string> ExtractCVariableModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check for type qualifiers and storage class specifiers
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "storage_class_specifier") == 0 || strcmp(child_type, "type_qualifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					modifiers.push_back(content.substr(start, end - start));
				}
			}
		}

		return modifiers;
	}
};

// Specialization for FUNCTION_CALL (C function calls)
template <>
struct CNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		return UnifiedFunctionCallExtractor<CLanguageTag>::Extract(node, content);
	}
};

// Specialization for CLASS_WITH_METHODS (C structs and unions)
template <>
struct CNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Determine if this is a struct, union, or enum
		const char *node_type = ts_node_type(node);
		if (strcmp(node_type, "struct_specifier") == 0) {
			context.signature_type = "struct";
		} else if (strcmp(node_type, "union_specifier") == 0) {
			context.signature_type = "union";
		} else if (strcmp(node_type, "enum_specifier") == 0) {
			context.signature_type = "enum";
		} else {
			context.signature_type = "type";
		}

		return context;
	}
};

} // namespace duckdb
