#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Swift-Specific Native Context Extractors
//==============================================================================

// Forward declaration for SwiftAdapter
class SwiftAdapter;

// Base template for Swift extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct SwiftNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for FUNCTION_WITH_PARAMS (Swift functions)
template <>
struct SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract return type (Swift has strong type system)
		context.signature_type = ExtractSwiftReturnType(node, content);

		// Extract function parameters with Swift type annotations
		context.parameters = ExtractSwiftParameters(node, content);

		// Extract function modifiers (public, private, static, mutating, etc.)
		auto modifiers = ExtractSwiftModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static string ExtractSwiftReturnType(TSNode node, const string &content) {
		// Swift AST: return type appears after "->" as a direct child (user_type, optional_type, etc.)
		uint32_t child_count = ts_node_child_count(node);
		bool found_arrow = false;
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "->") == 0) {
				found_arrow = true;
				continue;
			}

			if (found_arrow && IsSwiftTypeNode(child_type)) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}
		return ""; // Void or inferred return type
	}

	static bool IsSwiftTypeNode(const char *type) {
		return strcmp(type, "user_type") == 0 || strcmp(type, "type_identifier") == 0 ||
		       strcmp(type, "optional_type") == 0 || strcmp(type, "array_type") == 0 ||
		       strcmp(type, "dictionary_type") == 0 || strcmp(type, "tuple_type") == 0 ||
		       strcmp(type, "function_type") == 0;
	}

	static vector<ParameterInfo> ExtractSwiftParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Swift AST: parameters are direct children of function_declaration/init_declaration
		// (no wrapper node like parameter_list)
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "parameter") == 0) {
				ParameterInfo param = ExtractSwiftParameter(child, content);
				if (!param.name.empty()) {
					params.push_back(param);
				}
			} else if (strcmp(child_type, "variadic_parameter") == 0) {
				ParameterInfo param = ExtractSwiftVariadicParameter(child, content);
				if (!param.name.empty()) {
					params.push_back(param);
				}
			}
		}

		return params;
	}

	static ParameterInfo ExtractSwiftParameter(TSNode node, const string &content) {
		ParameterInfo param;
		uint32_t child_count = ts_node_child_count(node);

		string external_name = "";
		string internal_name = "";

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "simple_identifier") == 0) {
				// Could be external name or internal name
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					string name = content.substr(start, end - start);
					if (external_name.empty()) {
						external_name = name;
					} else {
						internal_name = name;
					}
				}
			} else if (strcmp(child_type, "user_type") == 0 || strcmp(child_type, "type_identifier") == 0 ||
			           strcmp(child_type, "optional_type") == 0 || strcmp(child_type, "array_type") == 0 ||
			           strcmp(child_type, "dictionary_type") == 0 || strcmp(child_type, "tuple_type") == 0 ||
			           strcmp(child_type, "function_type") == 0) {
				// Parameter type (direct child, not wrapped in type_annotation)
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.type = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "type_annotation") == 0) {
				// Fallback: some versions may use type_annotation
				param.type = ExtractSwiftTypeAnnotation(child, content);
			} else if (strcmp(child_type, "default_parameter_clause") == 0) {
				// Default value
				param.is_optional = true;
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.default_value = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "inout") == 0) {
				// inout parameter
				param.annotations = "inout";
			}
		}

		// Swift parameter naming: external_name internal_name: Type
		if (!internal_name.empty()) {
			param.name = external_name + " " + internal_name;
		} else {
			param.name = external_name;
		}

		return param;
	}

	static ParameterInfo ExtractSwiftVariadicParameter(TSNode node, const string &content) {
		ParameterInfo param;
		param.is_variadic = true;
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "simple_identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start);
				}
			} else if (IsSwiftTypeNode(child_type)) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.type = content.substr(start, end - start) + "...";
				}
			} else if (strcmp(child_type, "type_annotation") == 0) {
				param.type = ExtractSwiftTypeAnnotation(child, content) + "...";
			}
		}

		return param;
	}

	static string ExtractSwiftTypeAnnotation(TSNode node, const string &content) {
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (IsSwiftTypeNode(child_type)) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}

		return "";
	}

	static vector<string> ExtractSwiftModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Swift AST: modifiers are in a "modifiers" child node of the function_declaration
		// modifiers > {mutation_modifier, member_modifier, property_modifier, etc.} > keyword
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode mod = ts_node_child(child, j);
					const char *mod_type = ts_node_type(mod);

					if (strcmp(mod_type, "mutation_modifier") == 0 ||
					    strcmp(mod_type, "member_modifier") == 0 ||
					    strcmp(mod_type, "property_modifier") == 0 ||
					    strcmp(mod_type, "access_level_modifier") == 0 ||
					    strcmp(mod_type, "inheritance_modifier") == 0 ||
					    strcmp(mod_type, "attribute") == 0) {
						// Extract the text of the modifier (e.g., "mutating", "override", "static")
						uint32_t start = ts_node_start_byte(mod);
						uint32_t end = ts_node_end_byte(mod);
						if (start < content.length() && end <= content.length()) {
							modifiers.push_back(content.substr(start, end - start));
						}
					}
				}
				break;
			}
		}

		return modifiers;
	}
};

// Specialization for ARROW_FUNCTION (Swift closures)
template <>
struct SwiftNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "closure";

		// Extract closure parameters
		context.parameters = ExtractSwiftClosureParameters(node, content);

		// Extract closure modifiers (@escaping, @autoclosure, etc.)
		auto modifiers = ExtractSwiftClosureModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static vector<ParameterInfo> ExtractSwiftClosureParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find closure_parameters node
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "closure_parameters") == 0) {
				// Extract parameters from closure_parameters node (same structure as function params)
				params =
				    SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractSwiftParameters(
				        child, content);
				break;
			}
		}

		return params;
	}

	static vector<string> ExtractSwiftClosureModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Look for closure attributes
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "attribute") == 0) {
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

// Specialization for CLASS_WITH_METHODS (Swift classes, structs, protocols)
template <>
struct SwiftNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Determine the actual type by looking at keyword children
		// Swift grammar uses class_declaration for all type declarations
		const char *node_type = ts_node_type(node);
		context.signature_type = "type"; // Default

		if (strcmp(node_type, "class_declaration") == 0 || strcmp(node_type, "protocol_declaration") == 0) {
			// Look for keyword child to determine actual type
			uint32_t child_count = ts_node_child_count(node);
			for (uint32_t i = 0; i < child_count; i++) {
				TSNode child = ts_node_child(node, i);
				const char *child_type = ts_node_type(child);

				if (strcmp(child_type, "class") == 0) {
					context.signature_type = "class";
					break;
				} else if (strcmp(child_type, "struct") == 0) {
					context.signature_type = "struct";
					break;
				} else if (strcmp(child_type, "protocol") == 0) {
					context.signature_type = "protocol";
					break;
				} else if (strcmp(child_type, "enum") == 0) {
					context.signature_type = "enum";
					break;
				} else if (strcmp(child_type, "actor") == 0) {
					context.signature_type = "actor";
					break;
				}
			}
		}

		// Extract parent types into parameters
		bool has_inheritance = false;
		context.parameters = ExtractParentTypes(node, content, has_inheritance);
		context.modifiers = ExtractSwiftTypeModifiers(node, content, has_inheritance);

		return context;
	}

public:
	// Extract parent types from type_inheritance_clause or inheritance_specifier
	static vector<ParameterInfo> ExtractParentTypes(TSNode node, const string &content, bool &has_inheritance) {
		vector<ParameterInfo> parents;
		has_inheritance = false;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			// Check for type_inheritance_clause (classes) or inheritance_specifier (protocols)
			if (strcmp(child_type, "type_inheritance_clause") == 0 ||
			    strcmp(child_type, "inheritance_specifier") == 0) {
				has_inheritance = true;
				// : SuperClass, Protocol1, Protocol2
				uint32_t inherit_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < inherit_count; j++) {
					TSNode inherit_child = ts_node_child(child, j);
					const char *inherit_type = ts_node_type(inherit_child);

					// Skip punctuation (: and ,)
					if (strcmp(inherit_type, ":") == 0 || strcmp(inherit_type, ",") == 0) {
						continue;
					}

					if (strcmp(inherit_type, "type_identifier") == 0 || strcmp(inherit_type, "identifier") == 0) {
						string type_name = ExtractNodeText(inherit_child, content);
						if (!type_name.empty()) {
							// Swift uses unified inheritance syntax (:) for both class and protocol
							parents.push_back({type_name, "extends"});
						}
					} else if (strcmp(inherit_type, "user_type") == 0 || strcmp(inherit_type, "generic_type") == 0) {
						// Complex type - extract the identifier from within
						string type_name = ExtractTypeName(inherit_child, content);
						if (!type_name.empty()) {
							parents.push_back({type_name, "extends"});
						}
					}
				}
				// Don't break - there might be multiple inheritance_specifier nodes
			}
		}

		return parents;
	}

	static string ExtractTypeName(TSNode node, const string &content) {
		// For complex types, try to get the full text
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "identifier") == 0) {
				return ExtractNodeText(child, content);
			}
		}
		// Fallback: get the whole node text
		return ExtractNodeText(node, content);
	}

	static vector<string> ExtractSwiftTypeModifiers(TSNode node, const string &content, bool has_inheritance) {
		vector<string> modifiers;

		// Note: inheritance kind is now in ParameterInfo.type, not in modifiers

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "access_level_modifier") == 0 || strcmp(child_type, "member_modifier") == 0) {
				string mod = ExtractNodeText(child, content);
				if (!mod.empty()) {
					modifiers.push_back(mod);
				}
			} else if (strcmp(child_type, "attribute") == 0) {
				// @objc, @available, etc.
				string attr = ExtractNodeText(child, content);
				if (!attr.empty()) {
					modifiers.push_back(attr);
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

// Specialization for VARIABLE_WITH_TYPE (Swift variable declarations)
template <>
struct SwiftNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract Swift variable type (strong type system)
		context.signature_type = ExtractSwiftVariableType(node, content);

		// Extract variable modifiers (var/let, access level, etc.)
		auto modifiers = ExtractSwiftVariableModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static string ExtractSwiftVariableType(TSNode node, const string &content) {
		// Look for type annotation in variable declaration
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_annotation") == 0) {
				return SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractSwiftTypeAnnotation(
				    child, content);
			}
		}

		return ""; // Type inferred
	}

	static vector<string> ExtractSwiftVariableModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Swift AST: property_declaration children include modifiers, value_binding_pattern, etc.
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				// Extract modifiers from wrapper (same as function modifiers)
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode mod = ts_node_child(child, j);
					const char *mod_type = ts_node_type(mod);
					if (strcmp(mod_type, "access_level_modifier") == 0 ||
					    strcmp(mod_type, "member_modifier") == 0 ||
					    strcmp(mod_type, "property_modifier") == 0 ||
					    strcmp(mod_type, "ownership_modifier") == 0 ||
					    strcmp(mod_type, "attribute") == 0) {
						uint32_t start = ts_node_start_byte(mod);
						uint32_t end = ts_node_end_byte(mod);
						if (start < content.length() && end <= content.length()) {
							modifiers.push_back(content.substr(start, end - start));
						}
					}
				}
			} else if (strcmp(child_type, "value_binding_pattern") == 0) {
				// Extract var/let from value_binding_pattern child
				uint32_t vbp_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < vbp_count; j++) {
					TSNode vbp_child = ts_node_child(child, j);
					const char *vbp_type = ts_node_type(vbp_child);
					if (strcmp(vbp_type, "var") == 0 || strcmp(vbp_type, "let") == 0) {
						modifiers.push_back(vbp_type);
						break;
					}
				}
			}
		}

		return modifiers;
	}
};

// Specialization for ASYNC_FUNCTION (Swift async functions)
template <>
struct SwiftNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "async";

		// Extract parameters and return type (similar to regular functions)
		context.parameters =
		    SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractSwiftParameters(node, content);

		// Extract async/throws modifiers
		auto modifiers = ExtractSwiftAsyncModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static vector<string> ExtractSwiftAsyncModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Look for async/throws keywords
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "async") == 0 || strcmp(child_type, "throws") == 0 ||
			    strcmp(child_type, "rethrows") == 0) {
				modifiers.push_back(child_type);
			}
		}

		// Also get regular function modifiers
		auto regular_modifiers =
		    SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractSwiftModifiers(node, content);
		modifiers.insert(modifiers.end(), regular_modifiers.begin(), regular_modifiers.end());

		return modifiers;
	}
};

// Specialization for CONSTRUCTOR_DEFINITION (Swift init declarations)
// Constructors are like functions but have no return type
template <>
struct SwiftNativeExtractor<NativeExtractionStrategy::CONSTRUCTOR_DEFINITION> {
	static NativeContext Extract(TSNode node, const string &content) {
		// Reuse FUNCTION_WITH_PARAMS for parameter and modifier extraction
		auto context =
		    SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
		context.signature_type = ""; // Constructors have no return type
		return context;
	}
};

// Specialization for FUNCTION_CALL (Swift function calls)
template <>
struct SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		return UnifiedFunctionCallExtractor<SwiftLanguageTag>::Extract(node, content);
	}
};

} // namespace duckdb
