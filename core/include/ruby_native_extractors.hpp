#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Ruby-Specific Native Context Extractors
//==============================================================================

// Forward declaration for RubyAdapter
class RubyAdapter;

// Base template for Ruby extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct RubyNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for FUNCTION_WITH_PARAMS (Ruby methods)
template <>
struct RubyNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Infer return type from method body patterns
		context.signature_type = InferRubyReturnType(node, content);

		// Extract method parameters
		context.parameters = ExtractRubyParameters(node, content);

		// Extract method modifiers (private, protected, public)
		auto modifiers = ExtractRubyModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static vector<ParameterInfo> ExtractRubyParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find method_parameters node
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "method_parameters") == 0) {
				params = ExtractRubyParametersDirect(child, content);
				break;
			}
		}

		return params;
	}

	static vector<ParameterInfo> ExtractRubyParametersDirect(TSNode params_node, const string &content) {
		vector<ParameterInfo> parameters;

		uint32_t child_count = ts_node_child_count(params_node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(params_node, i);
			const char *child_type = ts_node_type(child);

			ParameterInfo param;
			bool is_valid_param = false;

			if (strcmp(child_type, "identifier") == 0) {
				// Simple parameter: def method(param)
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start);
					is_valid_param = true;
				}
			} else if (strcmp(child_type, "optional_parameter") == 0) {
				// Optional parameter: def method(param = default)
				param = ExtractRubyOptionalParameter(child, content);
				is_valid_param = !param.name.empty();
			} else if (strcmp(child_type, "splat_parameter") == 0) {
				// Splat parameter: def method(*args)
				param = ExtractRubySplatParameter(child, content);
				is_valid_param = !param.name.empty();
			} else if (strcmp(child_type, "hash_splat_parameter") == 0) {
				// Hash splat parameter: def method(**kwargs)
				param = ExtractRubyHashSplatParameter(child, content);
				is_valid_param = !param.name.empty();
			} else if (strcmp(child_type, "block_parameter") == 0) {
				// Block parameter: def method(&block)
				param = ExtractRubyBlockParameter(child, content);
				is_valid_param = !param.name.empty();
			} else if (strcmp(child_type, "keyword_parameter") == 0) {
				// Keyword parameter: def method(key:)
				param = ExtractRubyKeywordParameter(child, content);
				is_valid_param = !param.name.empty();
			}

			if (is_valid_param) {
				parameters.push_back(param);
			}
		}

		return parameters;
	}

	static ParameterInfo ExtractRubyOptionalParameter(TSNode node, const string &content) {
		ParameterInfo param;
		param.is_optional = true;
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
			} else if (i > 0) { // Skip the identifier, get the default value
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.default_value = content.substr(start, end - start);
				}
			}
		}

		return param;
	}

	static ParameterInfo ExtractRubySplatParameter(TSNode node, const string &content) {
		ParameterInfo param;
		param.is_variadic = true;
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = "*" + content.substr(start, end - start);
				}
				break;
			}
		}

		// If no identifier found, it's just a bare splat *
		if (param.name.empty()) {
			param.name = "*";
		}

		return param;
	}

	static ParameterInfo ExtractRubyHashSplatParameter(TSNode node, const string &content) {
		ParameterInfo param;
		param.is_variadic = true;
		param.annotations = "hash_splat";
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = "**" + content.substr(start, end - start);
				}
				break;
			}
		}

		if (param.name.empty()) {
			param.name = "**";
		}

		return param;
	}

	static ParameterInfo ExtractRubyBlockParameter(TSNode node, const string &content) {
		ParameterInfo param;
		param.annotations = "block";
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = "&" + content.substr(start, end - start);
				}
				break;
			}
		}

		return param;
	}

	static ParameterInfo ExtractRubyKeywordParameter(TSNode node, const string &content) {
		ParameterInfo param;
		param.annotations = "keyword";
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start) + ":";
				}
				break;
			}
		}

		return param;
	}

	static vector<string> ExtractRubyModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Ruby method visibility is typically set via method calls rather than modifiers
		// But we can check if the method is within a class and look for visibility declarations

		return modifiers; // Ruby methods don't have explicit modifiers in the signature
	}

	static string InferRubyReturnType(TSNode node, const string &content) {
		// Basic return type inference for Ruby methods
		uint32_t child_count = ts_node_child_count(node);
		bool has_string_return = false;
		bool has_boolean_return = false;
		bool has_numeric_return = false;
		bool has_instance_var = false;

		// Check method body for return patterns
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			uint32_t start = ts_node_start_byte(child);
			uint32_t end = ts_node_end_byte(child);

			if (start < content.length() && end <= content.length()) {
				string child_text = content.substr(start, end - start);

				// Check for common Ruby return patterns
				if (child_text.find("\"") != string::npos || child_text.find("'") != string::npos) {
					has_string_return = true;
				} else if (child_text.find("true") != string::npos || child_text.find("false") != string::npos) {
					has_boolean_return = true;
				} else if (child_text.find("@") != string::npos) {
					has_instance_var = true;
				} else if (child_text.find(".to_i") != string::npos || child_text.find(".length") != string::npos ||
				           child_text.find(".count") != string::npos) {
					has_numeric_return = true;
				}
			}
		}

		// Determine return type based on patterns
		if (has_string_return)
			return "String";
		if (has_boolean_return)
			return "Boolean";
		if (has_numeric_return)
			return "Integer";
		if (has_instance_var)
			return "Object";

		// Check for special Ruby method names
		uint32_t node_start = ts_node_start_byte(node);
		uint32_t node_end = ts_node_end_byte(node);
		if (node_start < content.length() && node_end <= content.length()) {
			string method_text = content.substr(node_start, node_end - node_start);
			if (method_text.find("initialize") != string::npos) {
				return "self";
			}
		}

		return "Object"; // Default Ruby return type
	}
};

// Specialization for ARROW_FUNCTION (Ruby blocks and lambdas)
template <>
struct RubyNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "lambda";

		// Extract block/lambda parameters
		context.parameters = ExtractRubyLambdaParameters(node, content);

		return context;
	}

public:
	static vector<ParameterInfo> ExtractRubyLambdaParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find lambda_parameters or block_parameters node
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "lambda_parameters") == 0 || strcmp(child_type, "block_parameters") == 0) {
				params =
				    RubyNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractRubyParametersDirect(
				        child, content);
				break;
			}
		}

		return params;
	}
};

// Specialization for CLASS_WITH_METHODS (Ruby classes and modules)
template <>
struct RubyNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Determine if this is a class or module
		const char *node_type = ts_node_type(node);
		if (strcmp(node_type, "class") == 0) {
			context.signature_type = "class";
		} else if (strcmp(node_type, "module") == 0) {
			context.signature_type = "module";
		} else {
			context.signature_type = "class";
		}

		// Extract inheritance information
		auto modifiers = ExtractRubyClassModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static vector<string> ExtractRubyClassModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Find superclass if present
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "superclass") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					modifiers.push_back("< " + content.substr(start, end - start));
				}
			}
		}

		return modifiers;
	}
};

// Specialization for VARIABLE_WITH_TYPE (Ruby variable assignments)
template <>
struct RubyNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Ruby variables don't have explicit types (dynamic typing)
		context.signature_type = "";

		// Extract variable scope modifiers (@, @@, $)
		auto modifiers = ExtractRubyVariableModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

public:
	static vector<string> ExtractRubyVariableModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check variable type based on naming convention
		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);
		if (start < content.length() && end <= content.length()) {
			string var_name = content.substr(start, end - start);

			if (var_name.length() > 0) {
				if (var_name[0] == '@') {
					if (var_name.length() > 1 && var_name[1] == '@') {
						modifiers.push_back("class_variable");
					} else {
						modifiers.push_back("instance_variable");
					}
				} else if (var_name[0] == '$') {
					modifiers.push_back("global_variable");
				} else if (var_name[0] >= 'A' && var_name[0] <= 'Z') {
					modifiers.push_back("constant");
				} else {
					modifiers.push_back("local_variable");
				}
			}
		}

		return modifiers;
	}
};

// Specialization for FUNCTION_CALL (Ruby function calls and method calls)
template <>
struct RubyNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		return UnifiedFunctionCallExtractor<RubyLanguageTag>::Extract(node, content);
	}
};

} // namespace duckdb
