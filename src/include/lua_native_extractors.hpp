#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Lua-Specific Native Context Extractors
//==============================================================================
//
// WHY THIS FILE EXISTS (#220). lua_types.def declared five native strategies --
// FUNCTION_WITH_PARAMS, ARROW_FUNCTION, two FUNCTION_CALLs and
// VARIABLE_WITH_TYPE -- and lua had no entry in NativeExtractionTraits, so it
// fell through to the DEFAULT traits specialization:
//
//     template <typename AdapterType>
//     struct NativeExtractionTraits {
//         template <NativeExtractionStrategy Strategy>
//         using ExtractorType = NativeExtractor<Strategy>;   // returns NativeContext()
//     };
//
// Every one of those declarations therefore delivered an empty context, with
// no compile error and no warning: the declaration surface (the .def) and the
// implementation surface (a traits specialization) can disagree silently. 19
// adapters were specialized and lua was not. The conformance kit caught it as
// `FAIL lua NATIVE-DECL  12 of 14 function defs take parameters in source, 0
// delivered`, which is exactly the "declared capability with nothing behind
// it" class the kit exists to find.
//
// GRAMMAR SHAPE, measured rather than assumed. tree-sitter-lua spells the
// parameter list `parameters`, and its children are the punctuation plus the
// names:
//
//     function M.create(name, value, ...)
//       parameters -> "(", identifier(name), ",", identifier(value), ",",
//                     vararg_expression(...), ")"
//
// So the names are `identifier` children and `...` is a `vararg_expression`.
// Lua has no parameter type annotations and no defaults in the parameter list
// (defaults are done in the body with `or`), so ParameterInfo.type and
// .default_value stay empty and only .is_variadic is ever set.

// Forward declaration for LuaAdapter
class LuaAdapter;

// Base template for Lua extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct LuaNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

namespace lua_native_detail {

// Byte-range text, guarded against a null node and against end < start (which
// a damaged tree can produce) rather than trusting the offsets.
inline string NodeText(TSNode node, const string &content) {
	if (ts_node_is_null(node)) {
		return "";
	}
	uint32_t start = ts_node_start_byte(node);
	uint32_t end = ts_node_end_byte(node);
	if (end <= start || end > content.size()) {
		return "";
	}
	return content.substr(start, end - start);
}

// Pull the parameter names out of a `parameters` node.
inline vector<ParameterInfo> ExtractFromParametersNode(TSNode params_node, const string &content) {
	vector<ParameterInfo> parameters;
	if (ts_node_is_null(params_node)) {
		return parameters;
	}
	uint32_t child_count = ts_node_child_count(params_node);
	if (child_count > 1000) { // same defensive bound the other extractors use
		return parameters;
	}
	for (uint32_t i = 0; i < child_count; i++) {
		TSNode child = ts_node_child(params_node, i);
		if (ts_node_is_null(child)) {
			continue;
		}
		const char *child_type = ts_node_type(child);
		if (!child_type) {
			continue;
		}
		if (strcmp(child_type, "identifier") == 0) {
			string name = NodeText(child, content);
			if (!name.empty()) {
				parameters.emplace_back(name, "", "", false, false);
			}
		} else if (strcmp(child_type, "vararg_expression") == 0) {
			// `...` -- variadic, and the only parameter form lua marks specially.
			parameters.emplace_back("...", "", "", false, true);
		}
		// "(" "," ")" are skipped: punctuation, not parameters.
	}
	return parameters;
}

// Find the `parameters` child of a function node and extract from it. Returns
// empty for `function noargs()`, which is a real answer rather than a failure
// -- the kit's NATIVE-DECL gate counts only functions that take a parameter in
// source, so a no-arg function must not look like a miss.
inline vector<ParameterInfo> ExtractParameters(TSNode node, const string &content) {
	vector<ParameterInfo> params;
	if (ts_node_is_null(node)) {
		return params;
	}
	uint32_t child_count = ts_node_child_count(node);
	if (child_count > 1000) {
		return params;
	}
	for (uint32_t i = 0; i < child_count; i++) {
		TSNode child = ts_node_child(node, i);
		if (ts_node_is_null(child)) {
			continue;
		}
		const char *child_type = ts_node_type(child);
		if (!child_type) {
			continue;
		}
		if (strcmp(child_type, "parameters") == 0) {
			params = ExtractFromParametersNode(child, content);
			break;
		}
	}
	return params;
}

// `local function f()` and `local f = function()` are the only visibility
// marker lua has: a name declared `local` is file-scoped, anything else is a
// global. The keyword sits on the declaration node for `local function`, and on
// the enclosing variable_declaration for `local f = function() ... end`.
inline vector<string> ExtractModifiers(TSNode node, const string &content) {
	vector<string> modifiers;
	if (ts_node_is_null(node)) {
		return modifiers;
	}
	uint32_t child_count = ts_node_child_count(node);
	if (child_count > 1000) {
		return modifiers;
	}
	for (uint32_t i = 0; i < child_count; i++) {
		TSNode child = ts_node_child(node, i);
		if (ts_node_is_null(child)) {
			continue;
		}
		const char *child_type = ts_node_type(child);
		if (child_type && strcmp(child_type, "local") == 0) {
			modifiers.push_back("local");
			break;
		}
	}
	if (!modifiers.empty()) {
		return modifiers;
	}
	// `local f = function() ... end` keeps the `local` THREE levels up, not on
	// the parent. Measured chain:
	//
	//     variable_declaration        <- holds the `local` child
	//       local
	//       assignment_statement
	//         variable_list -> identifier(f)
	//         "="
	//         expression_list
	//           function_definition   <- the node we were handed
	//
	// The first version of this walked only ts_node_parent(node) and looked for
	// `variable_declaration`; the real parent is `expression_list`, so that
	// branch could never fire while its comment claimed it handled this form.
	// Walk a bounded number of ancestors instead and take the first `local`.
	TSNode ancestor = node;
	for (int depth = 0; depth < 4; depth++) {
		ancestor = ts_node_parent(ancestor);
		if (ts_node_is_null(ancestor)) {
			break;
		}
		uint32_t pc = ts_node_child_count(ancestor);
		if (pc > 1000) {
			break;
		}
		for (uint32_t i = 0; i < pc; i++) {
			TSNode child = ts_node_child(ancestor, i);
			if (ts_node_is_null(child)) {
				continue;
			}
			const char *ct = ts_node_type(child);
			if (ct && strcmp(ct, "local") == 0) {
				modifiers.push_back("local");
				return modifiers;
			}
		}
	}
	return modifiers;
}

} // namespace lua_native_detail

// `function f(a, b) ... end` and `local function f(a, b) ... end`.
template <>
struct LuaNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		try {
			// Lua is untyped: there is no return annotation to read, so
			// signature_type stays empty rather than being invented.
			context.parameters = lua_native_detail::ExtractParameters(node, content);
			context.modifiers = lua_native_detail::ExtractModifiers(node, content);
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}
		return context;
	}
};

// `local f = function(a, b) ... end` -- lua's anonymous function. Declared
// ARROW_FUNCTION in lua_types.def; the parameter shape is identical to the
// named form, so the same helpers apply.
template <>
struct LuaNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		try {
			context.parameters = lua_native_detail::ExtractParameters(node, content);
			context.modifiers = lua_native_detail::ExtractModifiers(node, content);
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}
		return context;
	}
};

} // namespace duckdb
