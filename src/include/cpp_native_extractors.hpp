#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// C++ Native Context Extractors
//==============================================================================

// Forward declaration for CppAdapter
class CppAdapter;

// Base template for C++ extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct CppNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for FUNCTION_WITH_PARAMS (C++ functions/methods)
template <>
struct CppNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		try {
			// Extract return type (C++ functions have explicit return types)
			context.signature_type = ExtractCppReturnType(node, content);

			// Extract function parameters with C++ type annotations
			context.parameters = ExtractCppParameters(node, content);

			// Extract function specifiers and qualifiers
			auto modifiers = ExtractCppModifiers(node, content);
			context.modifiers = modifiers;

		} catch (...) {
			// Fallback on any error: provide minimal working context
			context.signature_type = ""; // Empty string becomes NULL in output
			context.parameters.clear();
			context.modifiers.clear();
		}

		return context;
	}

	// Public helper methods accessible to other extractors

	static string NodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node)) {
			return string();
		}
		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);
		if (start < content.length() && end <= content.length() && end > start) {
			return content.substr(start, end - start);
		}
		return string();
	}

	// #283. Walks a declarator chain to the declarator-id, which is the
	// parameter's name. The grammar bounds the walk exactly:
	// pointer_declarator, array_declarator and function_declarator each carry a
	// `declarator` FIELD, and function_declarator's own parameter list is a
	// SEPARATE `parameters` field, so following the field can never descend
	// into a function pointer's parameters and mistake one of those for the
	// name. reference_declarator, parenthesized_declarator and
	// variadic_declarator have no fields at all; their declarator is the single
	// named child.
	//
	// Returns a null node for an abstract declarator, such as the `int*` in
	// `void f(int*)`, which has no name to find.
	static TSNode FindDeclaratorId(TSNode declarator) {
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
			if (strcmp(node_type, "identifier") == 0 || strcmp(node_type, "field_identifier") == 0) {
				return current;
			}
			if (strcmp(node_type, "pointer_declarator") == 0 || strcmp(node_type, "array_declarator") == 0 ||
			    strcmp(node_type, "function_declarator") == 0 || strcmp(node_type, "init_declarator") == 0) {
				current = ts_node_child_by_field_name(current, "declarator", 10);
				continue;
			}
			if (strcmp(node_type, "reference_declarator") == 0 || strcmp(node_type, "parenthesized_declarator") == 0 ||
			    strcmp(node_type, "variadic_declarator") == 0) {
				current = ts_node_named_child(current, 0);
				continue;
			}
			break;
		}
		TSNode none = {};
		return none;
	}

	// #283. Collapses the whitespace that excision leaves behind, and closes the
	// gap an array bound opens: `int arr[10]` without `arr` reads `int [10]`,
	// and the type-id is written `int[10]`.
	static string NormalizeDeclaredType(const string &raw) {
		string out;
		bool pending_space = false;
		for (size_t i = 0; i < raw.size(); i++) {
			char c = raw[i];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
				pending_space = !out.empty();
				continue;
			}
			if (pending_space && c != '[') {
				out += ' ';
			}
			pending_space = false;
			out += c;
		}
		return out;
	}

	// #283. A parameter's declared type is its declaration with the name removed,
	// which is C++'s own abstract declarator. `int&` is a different type from
	// `int`, so the qualifiers stay. The previous revision matched a list of type
	// node names instead, which dropped every `const`, `&`, `&&` and `*`.
	//
	// Removed along with the name: the specifiers that are not part of a type.
	// node-types.json gives parameter_declaration's non-field children as
	// attribute_declaration, attribute_specifier, ms_declspec_modifier,
	// storage_class_specifier and type_qualifier. Only type_qualifier belongs to
	// the type, so it is the only one kept.
	//
	// A trailing attribute, as in `int x [[attr]]`, is NOT removed: a cut is
	// taken only where it precedes the name, so that the cuts stay in source
	// order and can never overlap.
	static string DeclaredTypeText(TSNode decl, TSNode name_id, const string &content) {
		uint32_t decl_start = ts_node_start_byte(decl);
		uint32_t decl_end = ts_node_end_byte(decl);

		// #315. An optional_parameter_declaration carries `= <expression>` inside
		// the node, and a default value is not part of the declared type. The
		// window is CLAMPED to the start of the `=` token -- the previous sibling
		// of the `default_value` field -- rather than excised as a third cut.
		//
		// A cut would have to be pushed after the name cut to keep the cuts in
		// source order, and the apply loop below skips any cut starting behind
		// its cursor, so a tail cut pushed inside the specifier loop would make
		// it skip the NAME and leave the name inside the type. Clamping cannot
		// get that wrong, and it also closes a corner: for the abstract form
		// `void h(int = 2)` there is no name, `name_start` falls back to the node
		// end, and the specifier guard would then admit a `comment` child sitting
		// after the `=`.
		//
		// Clamping keeps everything LEFT of the `=`, so an array bound survives:
		// `int arr[3] = nullptr` reads `int[3]`, matching the convention above.
		//
		// This is a no-op for the other two callers. node-types.json gives
		// neither parameter_declaration nor variadic_parameter_declaration a
		// `default_value` field, so the lookup returns a null node.
		TSNode default_value = ts_node_child_by_field_name(decl, "default_value", 13);
		if (!ts_node_is_null(default_value)) {
			TSNode eq = ts_node_prev_sibling(default_value);
			if (!ts_node_is_null(eq)) {
				uint32_t eq_start = ts_node_start_byte(eq);
				if (eq_start > decl_start && eq_start < decl_end) {
					decl_end = eq_start;
				}
			}
		}

		if (decl_start >= content.length() || decl_end > content.length() || decl_end <= decl_start) {
			return string();
		}

		// Byte ranges to drop, built in source order: the specifiers sit in
		// `_declaration_specifiers`, which precedes the declarator field.
		vector<std::pair<uint32_t, uint32_t>> cuts;
		bool have_name = !ts_node_is_null(name_id);
		uint32_t name_start = have_name ? ts_node_start_byte(name_id) : decl_end;
		uint32_t child_count = ts_node_child_count(decl);
		for (uint32_t i = 0; i < child_count && i < 100; i++) {
			TSNode child = ts_node_child(decl, i);
			if (ts_node_is_null(child)) {
				continue;
			}
			const char *child_type = ts_node_type(child);
			if (!child_type) {
				continue;
			}
			if (strcmp(child_type, "storage_class_specifier") == 0 ||
			    strcmp(child_type, "attribute_declaration") == 0 || strcmp(child_type, "attribute_specifier") == 0 ||
			    strcmp(child_type, "ms_declspec_modifier") == 0 || strcmp(child_type, "comment") == 0) {
				uint32_t child_start = ts_node_start_byte(child);
				uint32_t child_end = ts_node_end_byte(child);
				if (child_end <= name_start && child_end > child_start) {
					cuts.push_back(std::make_pair(child_start, child_end));
				}
			}
		}
		if (have_name) {
			cuts.push_back(std::make_pair(name_start, ts_node_end_byte(name_id)));
		}

		string kept;
		uint32_t cursor = decl_start;
		for (idx_t c = 0; c < cuts.size(); c++) {
			uint32_t cut_start = cuts[c].first;
			uint32_t cut_end = cuts[c].second;
			if (cut_start < cursor || cut_end > decl_end) {
				continue;
			}
			kept += content.substr(cursor, cut_start - cursor);
			cursor = cut_end;
		}
		if (cursor < decl_end) {
			kept += content.substr(cursor, decl_end - cursor);
		}

		return NormalizeDeclaredType(kept);
	}

	// Pulls the name and type out of a C++ variadic parameter. The pack is not a
	// special case any more: the same declarator walk and the same type rule
	// serve it and every ordinary parameter.
	//
	// grammars/tree-sitter-cpp/grammar.js:463-478 bounds the shape:
	//
	//     variadic_parameter_declaration: seq(
	//       _declaration_specifiers,
	//       field('declarator', choice(
	//         variadic_declarator,                                // "... args"
	//         alias(variadic_reference_declarator, reference_declarator))))
	//     variadic_declarator: seq('...', optional(identifier))
	//     variadic_reference_declarator: seq(choice('&&','&'), variadic_declarator)
	//
	// So the declarator is one of exactly two shapes and the identifier sits at
	// depth 1 or 2, never deeper. The previous revision read the type from the
	// FIRST non-declarator child, so `const Args&...` stopped at the `const` and
	// never reached `Args`; and it looked for the variadic_declarator among
	// DIRECT children only, so the `&` and `&&` forms, where it sits inside a
	// reference_declarator, lost the name entirely.
	//
	// The `...` needs no concatenation here. It is in the source text already, so
	// excising the name leaves it in place.
	static ParameterInfo ExtractCppVariadicParameterDeclaration(TSNode node, const string &content) {
		ParameterInfo param;
		param.is_variadic = true;

		TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
		TSNode name_id = FindDeclaratorId(declarator);
		param.name = NodeText(name_id, content);
		param.type = DeclaredTypeText(node, name_id, content);

		return param;
	}

	static ParameterInfo ExtractParameterDeclaration(TSNode node, const string &content) {
		ParameterInfo param;

		// Safety check: ensure node is valid
		if (ts_node_is_null(node)) {
			return param;
		}

		// #283. One rule for every C++ parameter: the name is the declarator-id
		// at the end of the declarator chain, and the type is the declaration
		// with that name removed.
		//
		// What this replaced lost two things. The type came from a list of type
		// node names, so `const int& a` read as `int` -- the qualifier and the
		// reference were both dropped. The name came from a ONE-level look
		// inside a declarator, so `int** pp`, `void (*fp)(int)`,
		// `int (&arr)[3]` and `const char* const* argv` all reported an empty
		// name, because their declarator-id is two or three levels down.
		TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
		TSNode name_id = FindDeclaratorId(declarator);
		param.name = NodeText(name_id, content);
		param.type = DeclaredTypeText(node, name_id, content);

		uint32_t child_count = ts_node_child_count(node);

		// Safety check: reasonable limit on child count to prevent infinite loops
		if (child_count > 1000) {
			return param;
		}

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);

			// Safety check: ensure child node is valid
			if (ts_node_is_null(child)) {
				continue;
			}

			const char *child_type = ts_node_type(child);

			// Safety check: ensure child_type is not null
			if (!child_type) {
				continue;
			}

			// #315. A branch for `default_parameter_declaration` stood here, and it
			// had never run: that name occurs 0 times in node-types.json,
			// grammar.json and grammar.js. The real node is
			// optional_parameter_declaration, which is a parameter node in its own
			// right rather than a child of this one, and it is handled by
			// ExtractOptionalParameterDeclaration below.
			if (strcmp(child_type, "storage_class_specifier") == 0 || strcmp(child_type, "type_qualifier") == 0) {
				// Parameter qualifiers (const, volatile, etc.)
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					if (!param.annotations.empty()) {
						param.annotations += " ";
					}
					param.annotations += content.substr(start, end - start);
				}
			}
		}

		return param;
	}

	// #315. A C++ default argument makes the parameter a DIFFERENT node:
	// `optional_parameter_declaration`, not `parameter_declaration`. Measured
	// from grammars/tree-sitter-cpp/grammar.js:456:
	//
	//     optional_parameter_declaration: seq(
	//       _declaration_specifiers,
	//       field('declarator', optional(choice(_declarator, abstract_reference_declarator))),
	//       '=',
	//       field('default_value', expression))
	//
	// The declarator is OPTIONAL, so `void h(int = 2)` is a legal parameter with
	// no name at all. Reading the `declarator` field keeps the name empty there.
	// Scanning children for an identifier instead would report the DEFAULT VALUE
	// as the name -- `int = x` would read `x` -- which is exactly what the AST
	// row's FIND_IDENTIFIER strategy does today, and that is #318, not this.
	//
	// The name and the type come from the same two helpers #283 added, so a
	// defaulted parameter follows one rule with every other parameter. The
	// default is kept out of the type by DeclaredTypeText's window clamp, not by
	// trimming its result, because a type can legitimately contain an `=` in
	// forms such as `decltype(a = b)`.
	static ParameterInfo ExtractOptionalParameterDeclaration(TSNode node, const string &content) {
		ParameterInfo param;

		if (ts_node_is_null(node)) {
			return param;
		}

		TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
		TSNode name_id = FindDeclaratorId(declarator);
		param.name = NodeText(name_id, content);
		param.type = DeclaredTypeText(node, name_id, content);

		// `is_optional` and `default_value` are recorded because ParameterInfo
		// has the fields. Neither reaches SQL: `parameters` is exposed as
		// STRUCT("name" VARCHAR, "type" VARCHAR)[], exactly as `is_variadic` is
		// invisible. Exposing them is #255's territory.
		TSNode default_value = ts_node_child_by_field_name(node, "default_value", 13);
		if (!ts_node_is_null(default_value)) {
			param.default_value = NodeText(default_value, content);
			param.is_optional = true;
		}

		return param;
	}

	static vector<ParameterInfo> ExtractCppParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Safety check: ensure node is valid
		if (ts_node_is_null(node)) {
			return params;
		}

		// #315. Three node types reach here that the scan below cannot serve,
		// because it looks for a function_declarator CHILD and then a
		// parameter_list inside it. A node that IS one of those found no such
		// child and returned an empty list, so a prototype reported no
		// parameters at all and a lambda reported none either -- defaulted or
		// not. All three declare FUNCTION_WITH_PARAMS in
		// languages/cpp/cpp_types.def (:78, :98, :774), so they were declared,
		// dispatched and silently empty.
		//
		// node-types.json gives every step a FIELD, so none of this needs a
		// scan:
		//
		//     function_declarator   field `parameters` -> parameter_list
		//     lambda_expression     field `declarator` -> lambda_declarator
		//     lambda_declarator     field `parameters` -> parameter_list
		//
		// `argument_list` also declares the strategy (:779) and is deliberately
		// NOT handled here: a call's arguments are not parameters, and they
		// already reach SQL through the FUNCTION_CALL extractor.
		const char *node_type = ts_node_type(node);
		if (node_type) {
			if (strcmp(node_type, "parameter_list") == 0) {
				return ExtractCppParametersDirect(node, content);
			}
			if (strcmp(node_type, "function_declarator") == 0) {
				TSNode list = ts_node_child_by_field_name(node, "parameters", 10);
				if (!ts_node_is_null(list)) {
					return ExtractCppParametersDirect(list, content);
				}
				return params;
			}
			if (strcmp(node_type, "lambda_expression") == 0) {
				TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
				if (!ts_node_is_null(declarator)) {
					TSNode list = ts_node_child_by_field_name(declarator, "parameters", 10);
					if (!ts_node_is_null(list)) {
						return ExtractCppParametersDirect(list, content);
					}
				}
				return params;
			}
		}

		// For C++, parameters are nested: function_definition -> function_declarator -> parameter_list
		// First find the function_declarator
		uint32_t child_count = ts_node_child_count(node);

		// Safety check: reasonable limit on child count
		if (child_count > 1000) {
			return params;
		}

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);

			// Safety check: ensure child node is valid
			if (ts_node_is_null(child)) {
				continue;
			}

			const char *child_type = ts_node_type(child);

			// Safety check: ensure child_type is not null
			if (!child_type) {
				continue;
			}

			if (strcmp(child_type, "function_declarator") == 0) {
				// Now look for parameter_list within the function_declarator
				uint32_t declarator_child_count = ts_node_child_count(child);

				// Safety check: reasonable limit on child count
				if (declarator_child_count > 1000) {
					continue;
				}

				for (uint32_t j = 0; j < declarator_child_count; j++) {
					TSNode param_child = ts_node_child(child, j);

					// Safety check: ensure child node is valid
					if (ts_node_is_null(param_child)) {
						continue;
					}

					const char *param_child_type = ts_node_type(param_child);

					// Safety check: ensure child_type is not null
					if (!param_child_type) {
						continue;
					}

					if (strcmp(param_child_type, "parameter_list") == 0) {
						params = ExtractCppParametersDirect(param_child, content);
						return params; // Found parameters, return immediately
					}
				}
			}
		}

		return params;
	}

private:
	static string ExtractCppReturnType(TSNode node, const string &content) {
		// In C++, the return type appears as a direct child before the function_declarator
		// For function_definition nodes, look for type nodes before function_declarator

		const char *node_type = ts_node_type(node);

		// If this IS a function_declarator, we need to look at the parent for return type
		// This happens when extracting from standalone declarations like:
		//   void foo();  -- declaration node contains: type + function_declarator
		if (strcmp(node_type, "function_declarator") == 0) {
			TSNode parent = ts_node_parent(node);
			if (!ts_node_is_null(parent)) {
				string parent_return_type = ExtractReturnTypeFromParent(parent, node, content);
				if (!parent_return_type.empty()) {
					return parent_return_type;
				}
			}
			// Fall through to check for constructor/destructor patterns
			string function_name = ExtractFunctionName(node, content);
			return CheckConstructorDestructor(function_name);
		}

		uint32_t child_count = ts_node_child_count(node);

		// Debug: Track what we find
		string function_name;

		// First pass: find function name and look for explicit return type
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			// Stop when we reach the function_declarator - return type comes before it
			if (strcmp(child_type, "function_declarator") == 0) {
				// Extract function name for constructor detection
				function_name = ExtractFunctionName(child, content);
				break;
			}

			// Look for various type constructs
			if (IsTypeNode(child_type)) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);

				if (start < content.length() && end <= content.length() && end > start) {
					string type_text = content.substr(start, end - start);
					// Skip modifiers like "static", "const" that are not the actual return type
					if (!IsModifierKeyword(type_text)) {
						return type_text;
					}
				}
			}
		}

		// If no explicit return type found, check if this is a constructor or destructor
		return CheckConstructorDestructor(function_name);
	}

	// Check if a node type represents a type construct
	static bool IsTypeNode(const char *child_type) {
		return strcmp(child_type, "primitive_type") == 0 || strcmp(child_type, "type_identifier") == 0 ||
		       strcmp(child_type, "template_type") == 0 || strcmp(child_type, "qualified_identifier") == 0 ||
		       strcmp(child_type, "pointer_type") == 0 || strcmp(child_type, "reference_type") == 0 ||
		       strcmp(child_type, "auto") == 0 || strcmp(child_type, "sized_type_specifier") == 0 ||
		       strcmp(child_type, "decltype") == 0;
	}

	// Check if a type text is actually a modifier keyword
	static bool IsModifierKeyword(const string &type_text) {
		return type_text == "static" || type_text == "const" || type_text == "inline" || type_text == "virtual" ||
		       type_text == "extern" || type_text == "constexpr" || type_text == "explicit" || type_text == "friend";
	}

	// Extract return type from parent node (for function_declarator nodes)
	static string ExtractReturnTypeFromParent(TSNode parent, TSNode declarator_node, const string &content) {
		const char *parent_type = ts_node_type(parent);

		// Parent could be: declaration, function_definition, field_declaration, etc.
		uint32_t child_count = ts_node_child_count(parent);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(parent, i);

			// Stop when we reach our function_declarator - type comes before it
			if (ts_node_eq(child, declarator_node)) {
				break;
			}

			const char *child_type = ts_node_type(child);

			if (IsTypeNode(child_type)) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);

				if (start < content.length() && end <= content.length() && end > start) {
					string type_text = content.substr(start, end - start);
					if (!IsModifierKeyword(type_text)) {
						return type_text;
					}
				}
			}
		}

		return "";
	}

	// Check if function name indicates constructor/destructor
	static string CheckConstructorDestructor(const string &function_name) {
		if (function_name.empty()) {
			return "";
		}

		// Check for destructor (starts with ~)
		if (function_name.length() > 1 && function_name[0] == '~') {
			return "void"; // Destructors return void
		}

		// For qualified names like "ClassName::MethodName", extract just the method name
		string method_name = function_name;
		string class_prefix;
		size_t scope_pos = function_name.find("::");
		if (scope_pos != string::npos) {
			class_prefix = function_name.substr(0, scope_pos);
			method_name = function_name.substr(scope_pos + 2);

			// Only treat as constructor if MethodName == ClassName
			// E.g., "ASTType::ASTType" is a constructor, but "CatalogSet::EntryLookup" is not
			if (method_name == class_prefix) {
				return class_prefix; // Constructor returns instance of the class
			}
		}
		// For unqualified names, be very conservative - many are legitimately constructors
		// Don't try to detect class context as it's unreliable

		// If we can't determine the return type, return empty string (will show as NULL)
		// This is better than guessing wrong
		return "";
	}

	static string ExtractFunctionName(TSNode function_declarator, const string &content) {
		uint32_t child_count = ts_node_child_count(function_declarator);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(function_declarator, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			} else if (strcmp(child_type, "destructor_name") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}
		return "";
	}

	static string ExtractContainingClassName(TSNode node, const string &content) {
		// Walk up the AST to find the containing class
		TSNode current = ts_node_parent(node);
		while (!ts_node_is_null(current)) {
			const char *node_type = ts_node_type(current);
			if (strcmp(node_type, "class_specifier") == 0 || strcmp(node_type, "struct_specifier") == 0) {
				// Found containing class, extract its name
				uint32_t child_count = ts_node_child_count(current);
				for (uint32_t i = 0; i < child_count; i++) {
					TSNode child = ts_node_child(current, i);
					const char *child_type = ts_node_type(child);
					if (strcmp(child_type, "type_identifier") == 0) {
						uint32_t start = ts_node_start_byte(child);
						uint32_t end = ts_node_end_byte(child);
						if (start < content.length() && end <= content.length()) {
							return content.substr(start, end - start);
						}
					}
				}
			}
			current = ts_node_parent(current);
		}
		return "";
	}

	static vector<ParameterInfo> ExtractCppParametersDirect(TSNode params_node, const string &content) {
		vector<ParameterInfo> parameters;

		// Safety check: ensure node is valid
		if (ts_node_is_null(params_node)) {
			return parameters;
		}

		uint32_t child_count = ts_node_child_count(params_node);

		// Safety check: reasonable limit on parameter count
		if (child_count > 1000) {
			return parameters;
		}

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(params_node, i);

			// Safety check: ensure child node is valid
			if (ts_node_is_null(child)) {
				continue;
			}

			const char *child_type = ts_node_type(child);

			// Safety check: ensure child_type is not null
			if (!child_type) {
				continue;
			}

			ParameterInfo param;
			bool is_valid_param = false;

			if (strcmp(child_type, "parameter_declaration") == 0) {
				// Standard parameter: (Type param) or (Type param = default)
				param = ExtractParameterDeclaration(child, content);
				is_valid_param = !param.name.empty() || !param.type.empty();
			} else if (strcmp(child_type, "variadic_parameter_declaration") == 0) {
				// `Args... args` parses as
				//   variadic_parameter_declaration
				//     type_identifier        "Args"
				//     variadic_declarator    "... args"
				//       "..." , identifier   "args"
				//
				// The previous revision dispatched on "variadic_parameter", which is
				// not in the tree-sitter-cpp symbol table at all, so this branch had
				// never run; it also filled in the placeholders name="..." and
				// type="variadic" rather than the real ones.
				param = ExtractCppVariadicParameterDeclaration(child, content);
				is_valid_param = !param.name.empty() || !param.type.empty();
			} else if (strcmp(child_type, "optional_parameter_declaration") == 0) {
				// #315. A parameter with a default argument. Dispatching on
				// `parameter_declaration` alone skipped the child entirely, so the
				// whole parameter was absent from the list and the reported arity
				// was silently short -- `void f(int a, int b = 2)` read as one
				// parameter, with nothing in the row saying one was missing.
				param = ExtractOptionalParameterDeclaration(child, content);
				is_valid_param = !param.name.empty() || !param.type.empty();
			}

			if (is_valid_param) {
				parameters.push_back(param);
			}
		}

		return parameters;
	}

	static vector<string> ExtractCppModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// C++ function modifiers appear as siblings to the function_declarator within function_definition
		// Look for modifier siblings in the current node's children
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			if (ts_node_is_null(child))
				continue;

			const char *child_type = ts_node_type(child);
			if (!child_type)
				continue;

			// Function specifiers and qualifiers that appear before function_declarator
			if (strcmp(child_type, "storage_class_specifier") == 0 || strcmp(child_type, "type_qualifier") == 0 ||
			    strcmp(child_type, "function_specifier") == 0 || strcmp(child_type, "virtual_specifier") == 0 ||
			    strcmp(child_type, "explicit_function_specifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length() && end > start) {
					modifiers.push_back(content.substr(start, end - start));
				}
			} else if (strcmp(child_type, "function_declarator") == 0) {
				// Look for trailing modifiers in the function_declarator
				uint32_t declarator_child_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < declarator_child_count; j++) {
					TSNode declarator_child = ts_node_child(child, j);
					if (ts_node_is_null(declarator_child))
						continue;

					const char *declarator_child_type = ts_node_type(declarator_child);
					if (!declarator_child_type)
						continue;

					if (strcmp(declarator_child_type, "type_qualifier") == 0 ||
					    strcmp(declarator_child_type, "noexcept") == 0 ||
					    strcmp(declarator_child_type, "override") == 0 || strcmp(declarator_child_type, "final") == 0) {
						uint32_t start = ts_node_start_byte(declarator_child);
						uint32_t end = ts_node_end_byte(declarator_child);
						if (start < content.length() && end <= content.length() && end > start) {
							modifiers.push_back(content.substr(start, end - start));
						}
					}
				}
			}
		}

		return modifiers;
	}
};

// Specialization for CLASS_WITH_METHODS and CLASS_WITH_INHERITANCE
template <>
struct CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		bool has_inheritance = false;
		context.signature_type = ExtractClassType(node, content);
		context.parameters = ExtractBaseClasses(node, content, has_inheritance);
		context.modifiers = ExtractCppClassModifiers(node, content, has_inheritance);

		return context;
	}

private:
	// Determine class type (class vs struct)
	static string ExtractClassType(TSNode node, const string &content) {
		string node_type = ts_node_type(node);
		if (node_type == "struct_specifier") {
			return "struct";
		}
		return "class";
	}

	// Extract base classes into parameters
	static vector<ParameterInfo> ExtractBaseClasses(TSNode node, const string &content, bool &has_inheritance) {
		vector<ParameterInfo> parents;
		has_inheritance = false;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "base_class_clause") == 0) {
				has_inheritance = true;
				// Extract each base class from the clause
				uint32_t base_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < base_count; j++) {
					TSNode base_child = ts_node_child(child, j);
					const char *base_type = ts_node_type(base_child);

					// Skip punctuation and access specifiers
					if (strcmp(base_type, ":") == 0 || strcmp(base_type, ",") == 0 ||
					    strcmp(base_type, "access_specifier") == 0 || strcmp(base_type, "public") == 0 ||
					    strcmp(base_type, "protected") == 0 || strcmp(base_type, "private") == 0 ||
					    strcmp(base_type, "virtual") == 0) {
						continue;
					}

					// Extract type identifier
					if (strcmp(base_type, "type_identifier") == 0 || strcmp(base_type, "qualified_identifier") == 0 ||
					    strcmp(base_type, "template_type") == 0 || strcmp(base_type, "dependent_type") == 0) {
						string type_name = ExtractNodeText(base_child, content);
						if (!type_name.empty()) {
							parents.push_back({type_name, ""});
						}
					}
				}
				break;
			}
		}

		return parents;
	}

	static vector<string> ExtractCppClassModifiers(TSNode node, const string &content, bool has_inheritance) {
		vector<string> modifiers;

		// Add extends keyword if class has base classes
		if (has_inheritance) {
			modifiers.push_back("extends");
		}

		// Check for template
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			const char *parent_type = ts_node_type(parent);
			if (strcmp(parent_type, "template_declaration") == 0) {
				modifiers.push_back("template");
			}
		}

		// Check for final specifier
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "virtual_specifier") == 0 || strcmp(child_type, "final") == 0) {
				string text = ExtractNodeText(child, content);
				if (text == "final") {
					modifiers.push_back("final");
				}
			}
		}

		return modifiers;
	}

	static string ExtractNodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node)) {
			return "";
		}

		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start < content.length() && end <= content.length() && end > start) {
			return content.substr(start, end - start);
		}

		return "";
	}
};

// Reuse CLASS_WITH_METHODS for CLASS_WITH_INHERITANCE
template <>
struct CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
	static NativeContext Extract(TSNode node, const string &content) {
		return CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
	}
};

// Specialization for ARROW_FUNCTION (C++ lambda expressions)
template <>
struct CppNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract lambda return type (often inferred, may be empty)
		context.signature_type = ExtractLambdaReturnType(node, content);

		// Extract lambda parameters (if present)
		context.parameters = ExtractLambdaParameters(node, content);

		// Extract lambda capture list and modifiers
		auto modifiers = ExtractLambdaModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

private:
	static string ExtractLambdaReturnType(TSNode node, const string &content) {
		// Lambda return types are often inferred in C++, but may be explicitly specified
		// Look for trailing return type syntax: []() -> ReturnType
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);

			if (ts_node_is_null(child)) {
				continue;
			}

			const char *child_type = ts_node_type(child);

			if (!child_type) {
				continue;
			}

			if (strcmp(child_type, "trailing_return_type") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}

		// Most lambdas have inferred return types
		return "";
	}

	// #315. This was a SECOND copy of the parameter dispatch, and it was dead:
	// it looked for a parameter_list among the lambda's DIRECT children, while
	// node-types.json puts it one level down, under the `declarator` field's
	// lambda_declarator. So a lambda reported no parameters at all, defaulted or
	// not -- which is why the no-default case is the control in the test.
	//
	// Delegating means the two field hops live in one place, and the
	// optional-parameter arm never has to exist twice.
	static vector<ParameterInfo> ExtractLambdaParameters(TSNode node, const string &content) {
		return CppNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractCppParameters(node, content);
	}

	static vector<string> ExtractLambdaModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Look for lambda capture list and other lambda-specific modifiers
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);

			if (ts_node_is_null(child)) {
				continue;
			}

			const char *child_type = ts_node_type(child);

			if (!child_type) {
				continue;
			}

			if (strcmp(child_type, "lambda_capture_specifier") == 0 ||
			    strcmp(child_type, "lambda_default_capture") == 0) {
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

// Specialization for VARIABLE_WITH_TYPE (C++ variable declarations)
template <>
struct CppNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract C++ variable type
		context.signature_type = ExtractCppVariableType(node, content);

		// Extract variable specifiers and qualifiers
		auto modifiers = ExtractCppVariableModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

private:
	static string ExtractCppVariableType(TSNode node, const string &content) {
		// Look for type in variable declaration
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "primitive_type") == 0 || strcmp(child_type, "type_identifier") == 0 ||
			    strcmp(child_type, "template_type") == 0 || strcmp(child_type, "qualified_identifier") == 0 ||
			    strcmp(child_type, "pointer_type") == 0 || strcmp(child_type, "reference_type") == 0 ||
			    strcmp(child_type, "auto") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}

		return "";
	}

	static vector<string> ExtractCppVariableModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check parent for variable specifiers
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			uint32_t parent_count = ts_node_child_count(parent);
			for (uint32_t i = 0; i < parent_count; i++) {
				TSNode child = ts_node_child(parent, i);
				const char *child_type = ts_node_type(child);

				if (strcmp(child_type, "storage_class_specifier") == 0 || strcmp(child_type, "type_qualifier") == 0 ||
				    strcmp(child_type, "constexpr") == 0 || strcmp(child_type, "thread_local") == 0) {
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

// Specialization for FUNCTION_CALL (C++ function calls and expressions)
template <>
struct CppNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		return UnifiedFunctionCallExtractor<CppLanguageTag>::Extract(node, content);
	}
};

// Specialization for CUSTOM (C++ function calls and expressions) - DEPRECATED: Use FUNCTION_CALL
template <>
struct CppNativeExtractor<NativeExtractionStrategy::CUSTOM> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		try {
			const char *node_type = ts_node_type(node);

			if (strcmp(node_type, "call_expression") == 0) {
				context = ExtractCallExpression(node, content);
			} else if (strcmp(node_type, "new_expression") == 0) {
				context = ExtractNewExpression(node, content);
			} else if (strcmp(node_type, "delete_expression") == 0) {
				context = ExtractDeleteExpression(node, content);
			} else {
				// Unknown node type for CUSTOM strategy
				context.signature_type = "";
			}

		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}

		return context;
	}

private:
	static NativeContext ExtractCallExpression(TSNode node, const string &content) {
		NativeContext context;

		// For call_expression: extract function name and arguments
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			// First child is usually the function identifier or member expression
			if (i == 0) {
				if (strcmp(child_type, "identifier") == 0 || strcmp(child_type, "qualified_identifier") == 0 ||
				    strcmp(child_type, "field_expression") == 0) {
					uint32_t start = ts_node_start_byte(child);
					uint32_t end = ts_node_end_byte(child);

					if (start < content.length() && end <= content.length() && end > start) {
						context.signature_type = content.substr(start, end - start);
					}
				}
			}

			// Extract arguments from argument_list
			if (strcmp(child_type, "argument_list") == 0) {
				context.parameters = ExtractCallArguments(child, content);
			}
		}

		if (context.signature_type.empty()) {
			context.signature_type = "function_call"; // Fallback
		}

		return context;
	}

	static NativeContext ExtractNewExpression(TSNode node, const string &content) {
		NativeContext context;

		// For new_expression: extract class name and constructor arguments
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			// Skip "new" keyword, look for type identifier
			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "identifier") == 0 ||
			    strcmp(child_type, "qualified_identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);

				if (start < content.length() && end <= content.length() && end > start) {
					context.signature_type = content.substr(start, end - start);
				}
			}

			// Extract constructor arguments from argument_list
			if (strcmp(child_type, "argument_list") == 0) {
				context.parameters = ExtractCallArguments(child, content);
			}
		}

		if (context.signature_type.empty()) {
			context.signature_type = "constructor_call"; // Fallback
		}

		return context;
	}

	static NativeContext ExtractDeleteExpression(TSNode node, const string &content) {
		NativeContext context;

		// For delete_expression: extract object identifier
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			// Skip "delete" keyword, look for identifier
			if (strcmp(child_type, "identifier") == 0 || strcmp(child_type, "qualified_identifier") == 0 ||
			    strcmp(child_type, "field_expression") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);

				if (start < content.length() && end <= content.length() && end > start) {
					context.signature_type = content.substr(start, end - start);
				}
			}
		}

		if (context.signature_type.empty()) {
			context.signature_type = "delete_call"; // Fallback
		}

		return context;
	}

	static vector<ParameterInfo> ExtractCallArguments(TSNode args_node, const string &content) {
		vector<ParameterInfo> arguments;

		uint32_t child_count = ts_node_child_count(args_node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(args_node, i);
			const char *child_type = ts_node_type(child);

			// Skip punctuation like "," and "(" and ")"
			if (strcmp(child_type, ",") == 0 || strcmp(child_type, "(") == 0 || strcmp(child_type, ")") == 0) {
				continue;
			}

			ParameterInfo arg;

			// Extract argument text as the "type" field
			uint32_t start = ts_node_start_byte(child);
			uint32_t end = ts_node_end_byte(child);

			if (start < content.length() && end <= content.length() && end > start) {
				arg.type = content.substr(start, end - start);
				arg.name = ""; // Arguments don't typically have names in calls
				arguments.push_back(arg);
			}
		}

		return arguments;
	}
};

} // namespace duckdb
