#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Kotlin-Specific Native Context Extractors
//==============================================================================

// Forward declaration for KotlinAdapter
class KotlinAdapter;

// Base template for Kotlin extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct KotlinNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for FUNCTION_WITH_PARAMS (Kotlin functions)
template <>
struct KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract return type (Kotlin has strong type system with nullable types)
		context.signature_type = ExtractKotlinReturnType(node, content);

		// Extract function parameters with Kotlin type annotations
		context.parameters = ExtractKotlinParameters(node, content);

		// Extract function modifiers (public, private, suspend, inline, etc.)
		auto modifiers = ExtractKotlinModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static string ExtractKotlinReturnType(TSNode node, const string &content) {
		// Look for return type after : in function signature
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type") == 0 || strcmp(child_type, "nullable_type") == 0 ||
			    strcmp(child_type, "user_type") == 0 || strcmp(child_type, "function_type") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}
		return "Unit"; // Default return type in Kotlin
	}

	static vector<ParameterInfo> ExtractKotlinParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find function_value_parameters node
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "function_value_parameters") == 0) {
				params = ExtractKotlinParametersDirect(child, content);
				break;
			}
		}

		return params;
	}

	// #220: this dispatched on `function_value_parameter` and `vararg_parameter`,
	// NEITHER OF WHICH EXISTS in tree-sitter-kotlin. Its symbol table carries
	// `function_value_parameters` (the container, which the caller finds
	// correctly), `parameter` and `class_parameter` -- and no singular
	// `function_value_parameter` at all. So every kotlin function reported zero
	// parameters while the extractor, the traits mapping and the .def
	// declaration were all present and wired. Measured shape:
	//
	//     fun add(a: Int, b: Int = 2, vararg rest: String)
	//
	//     function_value_parameters
	//       "("
	//       parameter              -> simple_identifier(a), ":", user_type(Int)
	//       ","
	//       parameter              -> simple_identifier(b), ":", user_type(Int)
	//       "="                    <- DEFAULT is a SIBLING of the parameter,
	//       integer_literal(2)        not a child of it
	//       ","
	//       parameter_modifiers    <- `vararg` is a PRECEDING SIBLING,
	//       parameter              -> simple_identifier(rest), ...
	//       ")"
	//
	// So defaults and vararg cannot be read from inside `parameter`; they have
	// to be tracked across the sibling walk. That is why this loop carries
	// state rather than mapping each child independently.
	static vector<ParameterInfo> ExtractKotlinParametersDirect(TSNode params_node, const string &content) {
		vector<ParameterInfo> parameters;

		if (ts_node_is_null(params_node)) {
			return parameters;
		}
		uint32_t child_count = ts_node_child_count(params_node);
		if (child_count > 1000) {
			return parameters;
		}

		bool next_is_vararg = false;  // set by a preceding parameter_modifiers
		bool expect_default = false;  // set by an `=` following a parameter

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(params_node, i);
			if (ts_node_is_null(child)) {
				continue;
			}
			const char *child_type = ts_node_type(child);
			if (!child_type) {
				continue;
			}

			if (strcmp(child_type, "parameter") == 0) {
				ParameterInfo param = ExtractKotlinParameter(child, content);
				if (!param.name.empty()) {
					param.is_variadic = next_is_vararg;
					parameters.push_back(param);
				}
				next_is_vararg = false;
				expect_default = false;
			} else if (strcmp(child_type, "parameter_modifiers") == 0) {
				// `vararg` is the only modifier that changes the parameter's
				// shape rather than its annotations; match on the text because
				// the grammar does not give it its own node type.
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (end > start && end <= content.size()) {
					next_is_vararg = content.substr(start, end - start).find("vararg") != string::npos;
				}
			} else if (strcmp(child_type, "=") == 0) {
				expect_default = true;
			} else if (expect_default) {
				// The node after `=` is the default expression, and it belongs
				// to the parameter most recently pushed.
				if (!parameters.empty()) {
					uint32_t start = ts_node_start_byte(child);
					uint32_t end = ts_node_end_byte(child);
					if (end > start && end <= content.size()) {
						parameters.back().default_value = content.substr(start, end - start);
						parameters.back().is_optional = true;
					}
				}
				expect_default = false;
			}
			// "(" "," ")" fall through: punctuation, not parameters.
		}

		return parameters;
	}

	static ParameterInfo ExtractKotlinParameter(TSNode node, const string &content) {
		ParameterInfo param;
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "simple_identifier") == 0) {
				// Parameter name
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "type") == 0 || strcmp(child_type, "nullable_type") == 0 ||
			           strcmp(child_type, "user_type") == 0) {
				// Parameter type
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.type = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "default_value") == 0) {
				// Default value
				param.is_optional = true;
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.default_value = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "parameter_modifier") == 0) {
				// crossinline, noinline, vararg
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.annotations = content.substr(start, end - start);
				}
			}
		}

		return param;
	}

	static vector<string> ExtractKotlinModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		auto extract_from_modifiers_node = [&](TSNode modifiers_node) {
			uint32_t mod_count = ts_node_child_count(modifiers_node);
			for (uint32_t j = 0; j < mod_count; j++) {
				TSNode modifier = ts_node_child(modifiers_node, j);
				const char *mod_type = ts_node_type(modifier);

				if (strcmp(mod_type, "visibility_modifier") == 0 || strcmp(mod_type, "function_modifier") == 0 ||
				    strcmp(mod_type, "member_modifier") == 0 || strcmp(mod_type, "parameter_modifier") == 0 ||
				    strcmp(mod_type, "inheritance_modifier") == 0 || strcmp(mod_type, "class_modifier") == 0 ||
				    strcmp(mod_type, "annotation") == 0) {
					uint32_t start = ts_node_start_byte(modifier);
					uint32_t end = ts_node_end_byte(modifier);
					if (start < content.length() && end <= content.length()) {
						modifiers.push_back(content.substr(start, end - start));
					}
				}
			}
		};

		// tree-sitter-kotlin: modifiers is a direct child of function_declaration
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			if (strcmp(ts_node_type(child), "modifiers") == 0) {
				extract_from_modifiers_node(child);
			}
		}

		// Also check parent's children (for cases where modifiers are siblings)
		if (modifiers.empty()) {
			TSNode parent = ts_node_parent(node);
			if (!ts_node_is_null(parent)) {
				uint32_t parent_count = ts_node_child_count(parent);
				for (uint32_t i = 0; i < parent_count; i++) {
					TSNode sibling = ts_node_child(parent, i);
					if (ts_node_eq(sibling, node))
						break;
					if (strcmp(ts_node_type(sibling), "modifiers") == 0) {
						extract_from_modifiers_node(sibling);
					}
				}
			}
		}

		return modifiers;
	}
};

// Specialization for ARROW_FUNCTION (Kotlin lambdas)
template <>
struct KotlinNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "lambda";

		// Extract lambda parameters
		context.parameters = ExtractKotlinLambdaParameters(node, content);

		return context;
	}

public:
	static vector<ParameterInfo> ExtractKotlinLambdaParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find lambda_parameters node
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "lambda_parameters") == 0) {
				params = KotlinNativeExtractor<
				    NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinParametersDirect(child, content);
				break;
			}
		}

		return params;
	}
};

// Specialization for CLASS_WITH_METHODS (Kotlin classes, interfaces, objects)
template <>
struct KotlinNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Determine if this is a class, interface, object, or enum
		const char *node_type = ts_node_type(node);
		bool has_extends = false;

		if (strcmp(node_type, "class_declaration") == 0) {
			context.signature_type = ExtractClassType(node, content);
		} else if (strcmp(node_type, "interface_declaration") == 0) {
			context.signature_type = "interface";
		} else if (strcmp(node_type, "object_declaration") == 0) {
			context.signature_type = "object";
		} else if (strcmp(node_type, "enum_class_declaration") == 0) {
			context.signature_type = "enum";
		} else {
			context.signature_type = "type";
		}

		// Extract parent types into parameters
		context.parameters = ExtractParentTypes(node, content, has_extends);
		context.modifiers = ExtractKotlinClassModifiers(node, content, has_extends);

		return context;
	}

public:
	static string ExtractClassType(TSNode node, const string &content) {
		// Check for data class, sealed class, abstract class, etc.
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					const char *mod_type = ts_node_type(modifier);

					if (strcmp(mod_type, "class_modifier") == 0) {
						string mod = ExtractNodeText(modifier, content);
						if (mod == "data")
							return "data_class";
						if (mod == "sealed")
							return "sealed_class";
						if (mod == "inner")
							return "inner_class";
						if (mod == "enum")
							return "enum";
					} else if (strcmp(mod_type, "inheritance_modifier") == 0) {
						string mod = ExtractNodeText(modifier, content);
						if (mod == "abstract")
							return "abstract_class";
						if (mod == "open")
							return "open_class";
					}
				}
			}
		}
		return "class";
	}

	// Extract parent types from delegation_specifiers into parameters
	static vector<ParameterInfo> ExtractParentTypes(TSNode node, const string &content, bool &has_extends) {
		vector<ParameterInfo> parents;
		has_extends = false;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "delegation_specifiers") == 0) {
				has_extends = true;
				// Extract each delegation_specifier
				uint32_t spec_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < spec_count; j++) {
					TSNode specifier = ts_node_child(child, j);
					const char *spec_type = ts_node_type(specifier);

					// Skip punctuation
					if (strcmp(spec_type, ",") == 0 || strcmp(spec_type, ":") == 0) {
						continue;
					}

					// Extract type from delegation_specifier or user_type
					string type_name = ExtractTypeName(specifier, content);
					if (!type_name.empty()) {
						parents.push_back({type_name, ""});
					}
				}
				break;
			}
		}

		return parents;
	}

	static string ExtractTypeName(TSNode node, const string &content) {
		const char *node_type = ts_node_type(node);

		// Direct type identifiers
		if (strcmp(node_type, "user_type") == 0 || strcmp(node_type, "simple_identifier") == 0) {
			return ExtractNodeText(node, content);
		}

		// Look for type inside node (e.g., delegation_specifier contains user_type)
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "user_type") == 0 || strcmp(child_type, "simple_identifier") == 0) {
				return ExtractNodeText(child, content);
			} else if (strcmp(child_type, "constructor_invocation") == 0) {
				// class Foo : Bar() - Bar is inside constructor_invocation
				uint32_t constr_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < constr_count; j++) {
					TSNode constr_child = ts_node_child(child, j);
					const char *constr_type = ts_node_type(constr_child);
					if (strcmp(constr_type, "user_type") == 0 || strcmp(constr_type, "simple_identifier") == 0) {
						return ExtractNodeText(constr_child, content);
					}
				}
			}
		}

		return "";
	}

	static vector<string> ExtractKotlinClassModifiers(TSNode node, const string &content, bool has_extends) {
		vector<string> modifiers;

		// Add extends keyword if class has parent types
		if (has_extends) {
			modifiers.push_back("extends");
		}

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				// Extract class modifiers
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					const char *mod_type = ts_node_type(modifier);

					if (strcmp(mod_type, "visibility_modifier") == 0 || strcmp(mod_type, "class_modifier") == 0 ||
					    strcmp(mod_type, "member_modifier") == 0 || strcmp(mod_type, "inheritance_modifier") == 0) {
						string mod = ExtractNodeText(modifier, content);
						if (!mod.empty()) {
							modifiers.push_back(mod);
						}
					}
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

// Specialization for VARIABLE_WITH_TYPE (Kotlin property declarations)
template <>
struct KotlinNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract Kotlin property type (nullable types supported)
		context.signature_type = ExtractKotlinPropertyType(node, content);

		// Extract property modifiers (var/val, access level, etc.)
		auto modifiers = ExtractKotlinPropertyModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static string ExtractKotlinPropertyType(TSNode node, const string &content) {
		// Look for type annotation in property declaration
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type") == 0 || strcmp(child_type, "nullable_type") == 0 ||
			    strcmp(child_type, "user_type") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}

		return ""; // Type inferred
	}

	static vector<string> ExtractKotlinPropertyModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check for property declaration modifiers
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				// Extract individual modifiers
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					const char *mod_type = ts_node_type(modifier);

					if (strcmp(mod_type, "visibility_modifier") == 0 || strcmp(mod_type, "member_modifier") == 0 ||
					    strcmp(mod_type, "property_modifier") == 0) {
						uint32_t start = ts_node_start_byte(modifier);
						uint32_t end = ts_node_end_byte(modifier);
						if (start < content.length() && end <= content.length()) {
							modifiers.push_back(content.substr(start, end - start));
						}
					}
				}
			}
		}

		// Check if this is var or val
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			const char *parent_type = ts_node_type(parent);
			if (strcmp(parent_type, "property_declaration") == 0) {
				// Check for var/val keyword
				uint32_t parent_count = ts_node_child_count(parent);
				for (uint32_t i = 0; i < parent_count; i++) {
					TSNode sibling = ts_node_child(parent, i);
					const char *sibling_type = ts_node_type(sibling);

					if (strcmp(sibling_type, "var") == 0 || strcmp(sibling_type, "val") == 0) {
						modifiers.push_back(sibling_type);
						break;
					}
				}
			}
		}

		return modifiers;
	}
};

// Specialization for ASYNC_FUNCTION (Kotlin suspend functions)
template <>
struct KotlinNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "suspend";

		// Extract parameters and return type (similar to regular functions)
		context.parameters =
		    KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinParameters(node,
		                                                                                                   content);

		// Extract suspend and other modifiers
		auto modifiers = ExtractKotlinSuspendModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static vector<string> ExtractKotlinSuspendModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Look for suspend keyword
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "suspend") == 0) {
				modifiers.push_back("suspend");
				break;
			}
		}

		auto regular_modifiers =
		    KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinModifiers(node,
		                                                                                                  content);
		for (const auto &rm : regular_modifiers) {
			EnsureModifier(modifiers, rm);
		}

		return modifiers;
	}
};

// Specialization for GENERIC_FUNCTION (Kotlin generic functions)
template <>
struct KotlinNativeExtractor<NativeExtractionStrategy::GENERIC_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "generic";

		// Extract parameters and return type
		context.parameters =
		    KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinParameters(node,
		                                                                                                   content);

		// Extract generic type parameters
		auto modifiers = ExtractKotlinGenericModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static vector<string> ExtractKotlinGenericModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Look for type_parameters
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_parameters") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					modifiers.push_back("generics " + content.substr(start, end - start));
				}
				break;
			}
		}

		auto regular_modifiers =
		    KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinModifiers(node,
		                                                                                                  content);
		for (const auto &rm : regular_modifiers) {
			EnsureModifier(modifiers, rm);
		}

		return modifiers;
	}
};

// Specialization for FUNCTION_CALL (Kotlin function calls)
template <>
struct KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		return UnifiedFunctionCallExtractor<KotlinLanguageTag>::Extract(node, content);
	}
};

} // namespace duckdb
