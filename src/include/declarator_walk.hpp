#pragma once

#include <tree_sitter/api.h>
#include <cstring>

namespace duckdb {

//==============================================================================
// The C/C++ declarator walk
//==============================================================================
//
// #318. One walk, shared by the two places that need it:
//
//   - the native parameter extractor, which has used it since #283
//     (src/include/cpp_native_extractors.hpp), and
//   - the FIND_IN_DECLARATOR name strategy (src/language_adapter.cpp), which
//     had its own shallower search and got `int` for the name of
//     `int (*fp_ret(int a))(char)`.
//
// It lives in its own header because of layering, not taste:
// src/language_adapter.cpp includes only adapter-level headers, and
// cpp_native_extractors.hpp is included by exactly one file. Neither can
// include the other, so the walk needs a home both may reach.
//
// WHY THE WALK IS EXACT. The grammar bounds it. pointer_declarator,
// array_declarator, function_declarator and init_declarator each carry a
// `declarator` FIELD, and a function_declarator's own parameter list is a
// SEPARATE `parameters` field. So following the field can never descend into a
// function pointer's parameters and mistake one of those for the name.
// reference_declarator, parenthesized_declarator and variadic_declarator have
// no fields at all; their declarator is the single named child.
//
// Returns a null node for an abstract declarator, such as the `int*` in
// `void f(int*)`, which has no name to find.
inline TSNode FindDeclaratorIdNode(TSNode declarator) {
	TSNode current = declarator;
	// The chain is a few links long in real code. The guard is only here so
	// that a malformed tree cannot spin.
	for (uint32_t guard = 0; guard < 32; guard++) {
		if (ts_node_is_null(current)) {
			break;
		}
		const char *node_type = ts_node_type(current);
		if (!node_type) {
			break;
		}
		if (std::strcmp(node_type, "identifier") == 0 || std::strcmp(node_type, "field_identifier") == 0) {
			return current;
		}
		if (std::strcmp(node_type, "pointer_declarator") == 0 || std::strcmp(node_type, "array_declarator") == 0 ||
		    std::strcmp(node_type, "function_declarator") == 0 || std::strcmp(node_type, "init_declarator") == 0) {
			current = ts_node_child_by_field_name(current, "declarator", 10);
			continue;
		}
		if (std::strcmp(node_type, "reference_declarator") == 0 ||
		    std::strcmp(node_type, "parenthesized_declarator") == 0 ||
		    std::strcmp(node_type, "variadic_declarator") == 0) {
			current = ts_node_named_child(current, 0);
			continue;
		}
		break;
	}
	TSNode none = {};
	return none;
}

// #318. The node types whose name IS the declarator-id, so that an ABSENT
// declarator means the node has no name rather than "look somewhere else".
//
// This exists because of a measured trap. `void f(int = x)` parses as
// `optional_parameter_declaration(primitive_type, "=", identifier x)` with no
// declarator field at all. A caller that falls back to a generic
// identifier search when the walk returns null would then report `x` -- the
// DEFAULT VALUE as the name, which is the exact defect #318 is fixing. For
// these node types the walk's null answer is authoritative and the fallback
// must be skipped.
//
// A function_definition is deliberately NOT here: for C and C++ its name is a
// declarator-id too, but a null walk there should still reach the old search,
// because other languages share the same strategy.
inline bool DeclaratorIdIsTheWholeName(const char *node_type) {
	if (!node_type) {
		return false;
	}
	return std::strcmp(node_type, "parameter_declaration") == 0 ||
	       std::strcmp(node_type, "optional_parameter_declaration") == 0 ||
	       std::strcmp(node_type, "variadic_parameter_declaration") == 0;
}

} // namespace duckdb
