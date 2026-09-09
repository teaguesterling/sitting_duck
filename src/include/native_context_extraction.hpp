#pragma once

#include "duckdb.hpp"
#include "node_config.hpp"
#include "ast_type.hpp" // For ParameterInfo and NativeContext definitions
#include <tree_sitter/api.h>
#include <vector>
#include <type_traits>
#include <cstring>

// Language-specific extractors will be included at the END to avoid circular dependencies

namespace duckdb {

// Note: ParameterInfo and NativeContext are now defined in ast_type.hpp

//==============================================================================
// Template-Based Native Context Extraction Framework
//==============================================================================

// Base template for native context extraction - default returns empty context
template <NativeExtractionStrategy Strategy>
struct NativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for NONE strategy - explicit no-op
template <>
struct NativeExtractor<NativeExtractionStrategy::NONE> {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Explicitly no extraction
	}
};

//==============================================================================
// Language-Specific Template Specialization Framework
//==============================================================================

// Forward declarations for language-specific extractors
template <NativeExtractionStrategy Strategy>
struct PythonNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct JavaScriptNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct JavaNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct TypeScriptNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct RustNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct CppNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct GoNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct CNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct PHPNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct RubyNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct SwiftNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct KotlinNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct RNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct BashNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct SQLNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct CSSNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct HTMLNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct DartNativeExtractor;
template <NativeExtractionStrategy Strategy>
struct CSharpNativeExtractor;

// Language adapter traits - each adapter defines its extractor type
template <typename AdapterType>
struct NativeExtractionTraits {
	// Default: use generic extractor
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = NativeExtractor<Strategy>;
};

// Forward declare language adapters for traits
class PythonAdapter;
class JavaScriptAdapter;
class TypeScriptAdapter;
class JavaAdapter;
class CPPAdapter;
class RustAdapter;
class GoAdapter;
class CAdapter;
class PHPAdapter;
class RubyAdapter;
class SwiftAdapter;
class KotlinAdapter;
class CSharpAdapter;
class RAdapter;
class BashAdapter;
class SQLAdapter;
class CSSAdapter;
class HTMLAdapter;
class DartAdapter;
class LuaAdapter;

// #64: per-adapter "bare name-binding" contexts. A plain identifier is generically a
// NAME_REFERENCE, but in some parent contexts (e.g. a bare parameter — an identifier
// directly in a parameter list) it introduces a name and should be NAME_DEFINITION.
// Such positions have no distinct wrapper node, so the flag cannot come from the .def
// file; the walker consults this per-grammar rule after applying the .def flags.
// Default: the language declares no bare binding contexts.
template <class AdapterType>
inline bool IsBareNameDefinitionParent(const char *parent_type) {
	return false;
}
// Python: an identifier directly inside a parameter list is a parameter definition.
// Typed/default parameters live in their own already-NAME_DEFINITION wrapper nodes, so
// they are unaffected; this only upgrades the bare `def f(self, x)` / `lambda p:` names.
template <>
inline bool IsBareNameDefinitionParent<PythonAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "parameters") == 0 || std::strcmp(parent_type, "lambda_parameters") == 0;
}
// #64 Phase 3 — other list-based languages (bare identifier directly in the param list;
// typed/defaulted params live in their own wrapper nodes, verified not directly in the list).
// JavaScript: formal_parameters. Ruby: method_parameters. Lua: parameters.
template <>
inline bool IsBareNameDefinitionParent<JavaScriptAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "formal_parameters") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<RubyAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "method_parameters") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<LuaAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "parameters") == 0;
}
// #64 Phase 3 — wrapper-based languages: the param NAME identifier's direct parent is
// the per-param wrapper node. Safe (verified) because the type is a non-identifier node
// and any default VALUE lives under a different node (optional_parameter_declaration,
// the param list, the function decl, optional_formal_parameters), so it is not flagged.
// typescript/c#/r are deliberately EXCLUDED here: their default value shares the wrapper
// node with the name, so the plain rule would mis-flag it — those need first-child/field
// precision (deferred, tracker/039).
template <>
inline bool IsBareNameDefinitionParent<CPPAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "parameter_declaration") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<CAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "parameter_declaration") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<GoAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "parameter_declaration") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<RustAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "parameter") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<JavaAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "formal_parameter") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<KotlinAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "parameter") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<SwiftAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "parameter") == 0;
}
template <>
inline bool IsBareNameDefinitionParent<DartAdapter>(const char *parent_type) {
	return std::strcmp(parent_type, "formal_parameter") == 0;
}

// #64 Phase 3 — FIELD-precise binding contexts. For languages where a defaulted param's
// VALUE shares the wrapper node with the NAME (so a plain parent-type rule would mis-flag
// the value), key on the tree-sitter FIELD: flag the node only when it is the wrapper's
// name/pattern field. Consulted in addition to IsBareNameDefinitionParent; default no-op
// (only TypeScript/C#/R specialize it — everything else pays nothing).
// ts_node_child_by_field_name returns a null node for an absent field, and
// ts_node_eq(self, null) is false, so this is safe when a param has no default/name field.
template <class AdapterType>
inline bool IsBareNameDefinitionField(TSNode /*self*/, TSNode /*parent*/, const char * /*parent_type*/) {
	return false;
}
// TypeScript: required_parameter/optional_parameter — name is field `pattern` (identifier
// binding) or `name`; the default is field `value` (excluded).
template <>
inline bool IsBareNameDefinitionField<TypeScriptAdapter>(TSNode self, TSNode parent, const char *parent_type) {
	if (std::strcmp(parent_type, "required_parameter") != 0 && std::strcmp(parent_type, "optional_parameter") != 0) {
		return false;
	}
	return ts_node_eq(self, ts_node_child_by_field_name(parent, "pattern", 7)) ||
	       ts_node_eq(self, ts_node_child_by_field_name(parent, "name", 4));
}
// C#: parameter — name is field `name` (the default value is not that field).
template <>
inline bool IsBareNameDefinitionField<CSharpAdapter>(TSNode self, TSNode parent, const char *parent_type) {
	return std::strcmp(parent_type, "parameter") == 0 && ts_node_eq(self, ts_node_child_by_field_name(parent, "name", 4));
}
// R: parameter — name is field `name`; the default is field `default` (excluded).
template <>
inline bool IsBareNameDefinitionField<RAdapter>(TSNode self, TSNode parent, const char *parent_type) {
	return std::strcmp(parent_type, "parameter") == 0 && ts_node_eq(self, ts_node_child_by_field_name(parent, "name", 4));
}

// Specializations for each language adapter
template <>
struct NativeExtractionTraits<PythonAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = PythonNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<JavaScriptAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = JavaScriptNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<TypeScriptAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = TypeScriptNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<JavaAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = JavaNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<CPPAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = CppNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<RustAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = RustNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<GoAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = GoNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<CAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = CNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<PHPAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = PHPNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<RubyAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = RubyNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<SwiftAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = SwiftNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<KotlinAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = KotlinNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<RAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = RNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<BashAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = BashNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<SQLAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = SQLNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<CSSAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = CSSNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<HTMLAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = HTMLNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<DartAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = DartNativeExtractor<Strategy>;
};

template <>
struct NativeExtractionTraits<CSharpAdapter> {
	template <NativeExtractionStrategy Strategy>
	using ExtractorType = CSharpNativeExtractor<Strategy>;
};

//==============================================================================
// Main Template Dispatch Function (Zero Virtual Calls)
//==============================================================================

// Dynamic strategy dispatch function - called from hot loop
template <typename AdapterType>
NativeContext ExtractNativeContextTemplated(TSNode node, const string &content, NativeExtractionStrategy strategy) {
	// Runtime dispatch to language-specific extractors
	// Uses template traits to get the correct extractor type for each language
	switch (strategy) {
	case NativeExtractionStrategy::FUNCTION_WITH_PARAMS:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
	case NativeExtractionStrategy::ASYNC_FUNCTION:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::ASYNC_FUNCTION>::Extract(node, content);
	case NativeExtractionStrategy::CLASS_WITH_METHODS:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
	case NativeExtractionStrategy::VARIABLE_WITH_TYPE:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::VARIABLE_WITH_TYPE>::Extract(node, content);
	case NativeExtractionStrategy::ARROW_FUNCTION:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::ARROW_FUNCTION>::Extract(node, content);
	case NativeExtractionStrategy::CLASS_WITH_INHERITANCE:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::CLASS_WITH_INHERITANCE>::Extract(node, content);
	case NativeExtractionStrategy::FUNCTION_WITH_DECORATORS:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::FUNCTION_WITH_DECORATORS>::Extract(node, content);
	case NativeExtractionStrategy::FUNCTION_CALL:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::FUNCTION_CALL>::Extract(node, content);
	case NativeExtractionStrategy::CONSTRUCTOR_DEFINITION:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<
		    NativeExtractionStrategy::CONSTRUCTOR_DEFINITION>::Extract(node, content);
	case NativeExtractionStrategy::CUSTOM:
		return NativeExtractionTraits<AdapterType>::template ExtractorType<NativeExtractionStrategy::CUSTOM>::Extract(
		    node, content);
	case NativeExtractionStrategy::NONE:
	default:
		return NativeContext(); // Return empty context for NONE or unknown strategies
	}
}

//==============================================================================
// Helper Functions for Common Extraction Patterns
//==============================================================================

// Helper function to extract text from a specific child by type
string ExtractChildTextByType(TSNode node, const string &content, const string &child_type);

// Helper function to extract all children of a specific type
vector<TSNode> FindChildrenByType(TSNode node, const string &child_type);

// Helper function to extract parameter list from common patterns
vector<ParameterInfo> ExtractParameterList(TSNode params_node, const string &content);

// Helper function to extract modifiers from various patterns
vector<string> ExtractModifiersFromNode(TSNode node, const string &content);

// Ensure a modifier is present in the list (dedup). Prepends if prepend=true.
inline void EnsureModifier(vector<string> &modifiers, const string &mod, bool prepend = false) {
	for (const auto &m : modifiers) {
		if (m == mod)
			return;
	}
	if (prepend) {
		modifiers.insert(modifiers.begin(), mod);
	} else {
		modifiers.push_back(mod);
	}
}

// Contract A: annotations/decorators are always '@'-prefixed in extracted text.
// Split them out of a mixed modifier list into the dedicated `annotations` string
// (joined by ", "), leaving `modifiers` keyword-only. No-op when there are none.
inline void SplitAnnotations(vector<string> &modifiers, string &annotations) {
	vector<string> keyword_only;
	string annots;
	for (const auto &m : modifiers) {
		if (!m.empty() && m[0] == '@') {
			if (!annots.empty()) {
				annots += ", ";
			}
			annots += m;
		} else {
			keyword_only.push_back(m);
		}
	}
	if (!annots.empty()) {
		annotations = annots;
	}
	modifiers = std::move(keyword_only);
}

// Collect '@decorator' text from a declaration node's children and its preceding
// siblings (decorators sit before/inside the declaration in TS/Python-style grammars).
inline vector<string> ExtractDecoratorTexts(TSNode node, const string &content) {
	vector<string> out;
	auto grab = [&](TSNode n) {
		if (string(ts_node_type(n)) == "decorator") {
			uint32_t s = ts_node_start_byte(n);
			uint32_t e = ts_node_end_byte(n);
			if (s < content.size() && e <= content.size() && e > s) {
				out.push_back(content.substr(s, e - s));
			}
		}
	};
	uint32_t child_count = ts_node_child_count(node);
	for (uint32_t i = 0; i < child_count; i++) {
		grab(ts_node_child(node, i));
	}
	// Preceding siblings: only the CONTIGUOUS run of decorators immediately before
	// the node belong to it. A non-decorator sibling (e.g. a prior method) ends the
	// run, so an earlier declaration's decorators don't bleed onto this one.
	TSNode parent = ts_node_parent(node);
	if (!ts_node_is_null(parent)) {
		uint32_t parent_count = ts_node_child_count(parent);
		uint32_t self_idx = parent_count;
		for (uint32_t i = 0; i < parent_count; i++) {
			if (ts_node_eq(ts_node_child(parent, i), node)) {
				self_idx = i;
				break;
			}
		}
		vector<string> preceding;
		for (uint32_t i = self_idx; i-- > 0;) {
			TSNode sib = ts_node_child(parent, i);
			if (string(ts_node_type(sib)) != "decorator") {
				break; // run ends at the first non-decorator
			}
			uint32_t s = ts_node_start_byte(sib);
			uint32_t e = ts_node_end_byte(sib);
			if (s < content.size() && e <= content.size() && e > s) {
				preceding.push_back(content.substr(s, e - s));
			}
		}
		// preceding was collected nearest-first; restore source order
		out.insert(out.end(), preceding.rbegin(), preceding.rend());
	}
	return out;
}

// Helper function to build qualified name from context
string BuildQualifiedName(TSNode node, const string &content, const string &base_name);

// Helper function to extract node text content
string ExtractNodeText(TSNode node, const string &content);

} // namespace duckdb

// Include language-specific extractors AFTER all declarations to prevent circular dependencies
#include "python_native_extractors.hpp"
#include "javascript_native_extractors.hpp"
#include "typescript_native_extractors.hpp"
#include "java_native_extractors.hpp"
#include "cpp_native_extractors.hpp"
#include "rust_native_extractors.hpp"
#include "go_native_extractors.hpp"
#include "c_native_extractors.hpp"
#include "php_native_extractors.hpp"
#include "ruby_native_extractors.hpp"
#include "swift_native_extractors.hpp"
#include "kotlin_native_extractors.hpp"
#include "r_native_extractors.hpp"
#include "bash_native_extractors.hpp"
#include "sql_native_extractors.hpp"
#include "css_native_extractors.hpp"
#include "html_native_extractors.hpp"
#include "dart_native_extractors.hpp"
#include "csharp_native_extractors.hpp"
