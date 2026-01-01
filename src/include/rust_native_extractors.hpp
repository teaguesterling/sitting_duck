#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Rust-Specific Native Context Extractors
//==============================================================================

// Forward declaration for RustAdapter
class RustAdapter;

// Base template for Rust extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct RustNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for FUNCTION_WITH_PARAMS (Rust functions)
template <>
struct RustNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		try {
			// Extract return type (Rust functions use -> Type syntax)
			context.signature_type = ExtractRustReturnType(node, content);

			// If no explicit return type, default to unit type ()
			if (context.signature_type.empty()) {
				context.signature_type = "()";
			}

			// Extract function parameters with Rust type annotations
			context.parameters = ExtractRustParameters(node, content);

			// Extract function modifiers and attributes
			auto modifiers = ExtractRustModifiers(node, content);
			context.modifiers = modifiers;

		} catch (...) {
			context.signature_type = "()"; // Default to unit type
			context.parameters.clear();
			context.modifiers.clear();
		}

		return context;
	}

private:
	static string ExtractRustReturnType(TSNode node, const string &content) {
		// Look for -> return_type pattern
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "->") == 0 && i + 1 < child_count) {
				// Next child should be the return type
				TSNode type_node = ts_node_child(node, i + 1);
				uint32_t start = ts_node_start_byte(type_node);
				uint32_t end = ts_node_end_byte(type_node);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}
		return ""; // No return type annotation (defaults to unit type ())
	}

	static vector<ParameterInfo> ExtractRustParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find parameters node
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "parameters") == 0) {
				params = ExtractRustParametersDirect(child, content);
				break;
			}
		}

		return params;
	}

	static vector<ParameterInfo> ExtractRustParametersDirect(TSNode params_node, const string &content) {
		vector<ParameterInfo> parameters;

		uint32_t child_count = ts_node_child_count(params_node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(params_node, i);
			const char *child_type = ts_node_type(child);

			ParameterInfo param;
			bool is_valid_param = false;

			if (strcmp(child_type, "parameter") == 0) {
				// Standard parameter: param: Type
				param = ExtractRustParameter(child, content);
				is_valid_param = !param.name.empty();
			} else if (strcmp(child_type, "self_parameter") == 0) {
				// Self parameter: self, &self, &mut self
				param = ExtractRustSelfParameter(child, content);
				is_valid_param = !param.name.empty();
			}

			if (is_valid_param) {
				parameters.push_back(param);
			}
		}

		return parameters;
	}

	static ParameterInfo ExtractRustParameter(TSNode node, const string &content) {
		ParameterInfo param;
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				// Parameter name
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "primitive_type") == 0 ||
			           strcmp(child_type, "generic_type") == 0 || strcmp(child_type, "reference_type") == 0 ||
			           strcmp(child_type, "pointer_type") == 0) {
				// Parameter type
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.type = content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "mutable_specifier") == 0) {
				// mut keyword
				param.annotations = "mut";
			}
		}

		return param;
	}

	static ParameterInfo ExtractRustSelfParameter(TSNode node, const string &content) {
		ParameterInfo param;
		param.name = "self";

		// Extract the full self parameter text to get &, &mut, etc.
		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);
		if (start < content.length() && end <= content.length()) {
			param.type = content.substr(start, end - start);
		}

		return param;
	}

	static vector<string> ExtractRustModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check for function modifiers and attributes
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			uint32_t parent_count = ts_node_child_count(parent);
			for (uint32_t i = 0; i < parent_count; i++) {
				TSNode sibling = ts_node_child(parent, i);
				const char *sibling_type = ts_node_type(sibling);

				if (strcmp(sibling_type, "visibility_modifier") == 0) {
					// pub, pub(crate), etc.
					uint32_t start = ts_node_start_byte(sibling);
					uint32_t end = ts_node_end_byte(sibling);
					if (start < content.length() && end <= content.length()) {
						modifiers.push_back(content.substr(start, end - start));
					}
				} else if (strcmp(sibling_type, "attribute_item") == 0) {
					// #[...] attributes
					uint32_t start = ts_node_start_byte(sibling);
					uint32_t end = ts_node_end_byte(sibling);
					if (start < content.length() && end <= content.length()) {
						modifiers.push_back(content.substr(start, end - start));
					}
				} else if (strcmp(sibling_type, "async") == 0 || strcmp(sibling_type, "unsafe") == 0 ||
				           strcmp(sibling_type, "extern") == 0 || strcmp(sibling_type, "const") == 0) {
					// Function keywords
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

// Specialization for ASYNC_FUNCTION
template <>
struct RustNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		// Reuse FUNCTION_WITH_PARAMS logic and add async modifier
		auto context = RustNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
		context.modifiers.insert(context.modifiers.begin(), "async");
		return context;
	}
};

// Specialization for CLASS_WITH_METHODS (Rust structs/enums with impl blocks)
template <>
struct RustNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Determine if this is a struct, enum, or trait
		const char *node_type = ts_node_type(node);
		if (strcmp(node_type, "struct_item") == 0) {
			context.signature_type = "struct";
		} else if (strcmp(node_type, "enum_item") == 0) {
			context.signature_type = "enum";
		} else if (strcmp(node_type, "trait_item") == 0) {
			context.signature_type = "trait";
		} else if (strcmp(node_type, "impl_item") == 0) {
			context.signature_type = "impl";
		} else {
			context.signature_type = "type";
		}

		// Extract type modifiers and attributes
		auto modifiers = ExtractRustTypeModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

private:
	static vector<string> ExtractRustTypeModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "visibility_modifier") == 0) {
				// pub visibility
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					modifiers.push_back(content.substr(start, end - start));
				}
			} else if (strcmp(child_type, "attribute_item") == 0) {
				// #[derive(...)] and other attributes
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					modifiers.push_back(content.substr(start, end - start));
				}
			} else if (strcmp(child_type, "type_parameters") == 0) {
				// Generic parameters <T, U>
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					modifiers.push_back("generic" + content.substr(start, end - start));
				}
			}
		}

		return modifiers;
	}
};

// Reuse CLASS_WITH_METHODS for CLASS_WITH_INHERITANCE
template <>
struct RustNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
	static NativeContext Extract(TSNode node, const string &content) {
		return RustNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
	}
};

// Specialization for VARIABLE_WITH_TYPE (Rust variable bindings)
template <>
struct RustNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract Rust variable type annotation
		context.signature_type = ExtractRustVariableType(node, content);

		// Extract variable modifiers (mut, pub, etc.)
		auto modifiers = ExtractRustVariableModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

private:
	static string ExtractRustVariableType(TSNode node, const string &content) {
		// Look for type annotation in let binding
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "primitive_type") == 0 ||
			    strcmp(child_type, "generic_type") == 0 || strcmp(child_type, "reference_type") == 0 ||
			    strcmp(child_type, "pointer_type") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}

		return "";
	}

	static vector<string> ExtractRustVariableModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check for let, mut, const, static keywords
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			uint32_t parent_count = ts_node_child_count(parent);
			for (uint32_t i = 0; i < parent_count; i++) {
				TSNode child = ts_node_child(parent, i);
				const char *child_type = ts_node_type(child);

				if (strcmp(child_type, "mutable_specifier") == 0 || strcmp(child_type, "let") == 0 ||
				    strcmp(child_type, "const") == 0 || strcmp(child_type, "static") == 0) {
					uint32_t start = ts_node_start_byte(child);
					uint32_t end = ts_node_end_byte(child);
					if (start < content.length() && end <= content.length()) {
						modifiers.push_back(content.substr(start, end - start));
					}
				} else if (strcmp(child_type, "visibility_modifier") == 0) {
					// pub for static/const items
					uint32_t start = ts_node_start_byte(child);
					uint32_t end = ts_node_end_byte(child);
					if (start < content.length() && end <= content.length()) {
						modifiers.push_back(content.substr(start, end - start));
					}
				}
			}
		}

		return modifiers;
	}
};

// Specialization for FUNCTION_CALL (Rust function calls and method calls)
template <>
struct RustNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		return UnifiedFunctionCallExtractor<RustLanguageTag>::Extract(node, content);
	}
};

} // namespace duckdb
