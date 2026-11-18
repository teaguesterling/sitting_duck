#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// C#-Specific Native Context Extractors
//==============================================================================

// Forward declaration for CSharpAdapter
class CSharpAdapter;

// Base template for C# extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct CSharpNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for FUNCTION_WITH_PARAMS (C# methods)
template <>
struct CSharpNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		try {
			// Extract return type
			context.signature_type = ExtractReturnType(node, content);

			// Extract method parameters
			context.parameters = ExtractParameters(node, content);

			// Extract method modifiers (public, private, static, async, etc.)
			context.modifiers = ExtractModifiers(node, content);

		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}

		return context;
	}

public:
	static string ExtractReturnType(TSNode node, const string &content) {
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			// Look for type nodes (predefined_type, identifier, generic_name, etc.)
			if (strcmp(child_type, "predefined_type") == 0 || strcmp(child_type, "identifier") == 0 ||
			    strcmp(child_type, "generic_name") == 0 || strcmp(child_type, "nullable_type") == 0 ||
			    strcmp(child_type, "array_type") == 0 || strcmp(child_type, "qualified_name") == 0) {
				return ExtractNodeText(child, content);
			}
		}
		return "void";
	}

	static vector<ParameterInfo> ExtractParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "parameter_list") == 0) {
				params = ExtractParameterList(child, content);
				break;
			}
		}

		return params;
	}

	static vector<ParameterInfo> ExtractParameterList(TSNode params_node, const string &content) {
		vector<ParameterInfo> parameters;

		uint32_t child_count = ts_node_child_count(params_node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(params_node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "parameter") == 0) {
				ParameterInfo param = ExtractParameter(child, content);
				if (!param.name.empty()) {
					parameters.push_back(param);
				}
			}
		}

		return parameters;
	}

	static ParameterInfo ExtractParameter(TSNode node, const string &content) {
		ParameterInfo param;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				param.name = ExtractNodeText(child, content);
			} else if (strcmp(child_type, "predefined_type") == 0 || strcmp(child_type, "generic_name") == 0 ||
			           strcmp(child_type, "nullable_type") == 0 || strcmp(child_type, "array_type") == 0 ||
			           strcmp(child_type, "qualified_name") == 0) {
				param.type = ExtractNodeText(child, content);
			} else if (strcmp(child_type, "parameter_modifier") == 0) {
				// ref, out, in, params, this
				param.annotations = ExtractNodeText(child, content);
			} else if (strcmp(child_type, "equals_value_clause") == 0) {
				param.is_optional = true;
				param.default_value = ExtractNodeText(child, content);
			}
		}

		return param;
	}

	static vector<string> ExtractModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifier") == 0) {
				string mod = ExtractNodeText(child, content);
				if (!mod.empty()) {
					modifiers.push_back(mod);
				}
			}
		}

		return modifiers;
	}

	static string ExtractNodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node))
			return "";

		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start < content.length() && end <= content.length() && end > start) {
			return content.substr(start, end - start);
		}

		return "";
	}
};

// Specialization for CLASS_WITH_METHODS (C# classes, interfaces, structs, enums, records)
template <>
struct CSharpNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		try {
			string node_type = ts_node_type(node);
			bool has_extends = false;
			bool has_implements = false;

			// Determine type kind
			if (node_type == "class_declaration") {
				context.signature_type = ExtractClassType(node, content);
			} else if (node_type == "interface_declaration") {
				context.signature_type = "interface";
			} else if (node_type == "struct_declaration") {
				context.signature_type = "struct";
			} else if (node_type == "enum_declaration") {
				context.signature_type = "enum";
			} else if (node_type == "record_declaration") {
				context.signature_type = "record";
			} else {
				context.signature_type = "class";
			}

			// Extract base types into parameters
			context.parameters = ExtractBaseTypes(node, content, has_extends, has_implements);
			context.modifiers = ExtractClassModifiers(node, content, has_extends, has_implements);

		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}

		return context;
	}

public:
	static string ExtractClassType(TSNode node, const string &content) {
		// Check for abstract or static class
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifier") == 0) {
				string mod = ExtractNodeText(child, content);
				if (mod == "abstract") {
					return "abstract_class";
				} else if (mod == "static") {
					return "static_class";
				} else if (mod == "sealed") {
					return "sealed_class";
				}
			}
		}
		return "class";
	}

	// Extract base class and implemented interfaces into parameters
	static vector<ParameterInfo> ExtractBaseTypes(TSNode node, const string &content, bool &has_extends,
	                                              bool &has_implements) {
		vector<ParameterInfo> parents;
		has_extends = false;
		has_implements = false;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "base_list") == 0) {
				// In C#, the base_list contains both base class and interfaces
				// First type could be base class (if not an interface)
				// Remaining are interfaces
				uint32_t base_count = ts_node_child_count(child);
				bool first_type = true;

				for (uint32_t j = 0; j < base_count; j++) {
					TSNode base_child = ts_node_child(child, j);
					const char *base_type = ts_node_type(base_child);

					// Skip punctuation
					if (strcmp(base_type, ":") == 0 || strcmp(base_type, ",") == 0) {
						continue;
					}

					string type_name = "";
					if (strcmp(base_type, "identifier") == 0 || strcmp(base_type, "generic_name") == 0 ||
					    strcmp(base_type, "qualified_name") == 0) {
						type_name = ExtractNodeText(base_child, content);
					} else if (strcmp(base_type, "base_type") == 0 || strcmp(base_type, "simple_base_type") == 0) {
						// Extract the type from inside
						uint32_t inner_count = ts_node_child_count(base_child);
						for (uint32_t k = 0; k < inner_count; k++) {
							TSNode inner = ts_node_child(base_child, k);
							const char *inner_type = ts_node_type(inner);
							if (strcmp(inner_type, "identifier") == 0 || strcmp(inner_type, "generic_name") == 0 ||
							    strcmp(inner_type, "qualified_name") == 0) {
								type_name = ExtractNodeText(inner, content);
								break;
							}
						}
					}

					if (!type_name.empty()) {
						parents.push_back({type_name, ""});

						// In C#, convention is first item is base class (if not interface),
						// rest are interfaces. We mark any inheritance as "extends" for simplicity.
						if (first_type) {
							has_extends = true;
							first_type = false;
						} else {
							has_implements = true;
						}
					}
				}
				break;
			}
		}

		return parents;
	}

	static vector<string> ExtractClassModifiers(TSNode node, const string &content, bool has_extends,
	                                            bool has_implements) {
		vector<string> modifiers;

		// Add inheritance keywords
		if (has_extends) {
			modifiers.push_back("extends");
		}
		if (has_implements) {
			modifiers.push_back("implements");
		}

		// Extract access and other modifiers
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifier") == 0) {
				string mod = ExtractNodeText(child, content);
				if (!mod.empty()) {
					modifiers.push_back(mod);
				}
			}
		}

		return modifiers;
	}

	static string ExtractNodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node))
			return "";

		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start < content.length() && end <= content.length() && end > start) {
			return content.substr(start, end - start);
		}

		return "";
	}
};

// Specialization for ARROW_FUNCTION (C# lambda expressions)
template <>
struct CSharpNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "lambda";

		// Extract lambda parameters
		context.parameters = ExtractLambdaParameters(node, content);

		return context;
	}

public:
	static vector<ParameterInfo> ExtractLambdaParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "parameter_list") == 0) {
				params = CSharpNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractParameterList(
				    child, CSharpNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractNodeText(
				               node, content));
				break;
			} else if (strcmp(child_type, "identifier") == 0) {
				// Single parameter without parentheses: x => x + 1
				ParameterInfo param;
				param.name = CSharpNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractNodeText(
				    child, content);
				if (!param.name.empty()) {
					params.push_back(param);
				}
				break;
			}
		}

		return params;
	}
};

// Specialization for VARIABLE_WITH_TYPE (C# fields and properties)
template <>
struct CSharpNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract type
		context.signature_type = ExtractVariableType(node, content);

		// Extract modifiers
		context.modifiers = ExtractVariableModifiers(node, content);

		return context;
	}

public:
	static string ExtractVariableType(TSNode node, const string &content) {
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "predefined_type") == 0 || strcmp(child_type, "identifier") == 0 ||
			    strcmp(child_type, "generic_name") == 0 || strcmp(child_type, "nullable_type") == 0 ||
			    strcmp(child_type, "array_type") == 0 || strcmp(child_type, "qualified_name") == 0) {
				return ExtractNodeText(child, content);
			} else if (strcmp(child_type, "variable_declaration") == 0) {
				// Look inside variable_declaration for type
				uint32_t var_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < var_count; j++) {
					TSNode var_child = ts_node_child(child, j);
					const char *var_type = ts_node_type(var_child);

					if (strcmp(var_type, "predefined_type") == 0 || strcmp(var_type, "identifier") == 0 ||
					    strcmp(var_type, "generic_name") == 0 || strcmp(var_type, "nullable_type") == 0) {
						return ExtractNodeText(var_child, content);
					}
				}
			}
		}
		return "";
	}

	static vector<string> ExtractVariableModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifier") == 0) {
				string mod = ExtractNodeText(child, content);
				if (!mod.empty()) {
					modifiers.push_back(mod);
				}
			}
		}

		return modifiers;
	}

	static string ExtractNodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node))
			return "";

		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start < content.length() && end <= content.length() && end > start) {
			return content.substr(start, end - start);
		}

		return "";
	}
};

// Specialization for FUNCTION_CALL (C# method invocations)
template <>
struct CSharpNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		return UnifiedFunctionCallExtractor<CSharpLanguageTag>::Extract(node, content);
	}
};

} // namespace duckdb
