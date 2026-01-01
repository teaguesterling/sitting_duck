#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Java-Specific Native Context Extractors
//==============================================================================

// Base template for Java extractors - default returns empty context
template <NativeExtractionStrategy Strategy>
struct JavaNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content) {
		return NativeContext(); // Default: no extraction
	}
};

// Specialization for FUNCTION_WITH_PARAMS (Java methods)
template <>
struct JavaNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		// Extract return type (Java methods have explicit return types)
		context.signature_type = ExtractJavaReturnType(node, content);

		// Extract method parameters with Java type annotations
		context.parameters = ExtractJavaParameters(node, content);

		// Extract access modifiers and other modifiers
		auto modifiers = ExtractJavaModifiers(node, content);
		context.modifiers = modifiers;

		return context;
	}

private:
	static string ExtractJavaReturnType(TSNode node, const string &content) {
		// In Java, return type comes before method name
		// Structure: [modifiers] returnType methodName(params) { }
		uint32_t child_count = ts_node_child_count(node);

		// Look for return type node (can be anywhere in the method declaration)
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			// Look for any return type (primitive types like int, or reference types like String)
			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "primitive_type") == 0 ||
			    strcmp(child_type, "generic_type") == 0 || strcmp(child_type, "array_type") == 0 ||
			    strcmp(child_type, "void_type") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length() && end > start) {
					return content.substr(start, end - start);
				}
			}
		}

		// If not found, check parent for return type (method_declaration structure)
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			uint32_t parent_count = ts_node_child_count(parent);

			for (uint32_t i = 0; i < parent_count && i < 20; i++) { // Limit search
				TSNode child = ts_node_child(parent, i);
				const char *child_type = ts_node_type(child);

				// Look for any return type in parent
				if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "primitive_type") == 0 ||
				    strcmp(child_type, "generic_type") == 0 || strcmp(child_type, "array_type") == 0 ||
				    strcmp(child_type, "void_type") == 0) {
					uint32_t start = ts_node_start_byte(child);
					uint32_t end = ts_node_end_byte(child);
					if (start < content.length() && end <= content.length() && end > start) {
						return content.substr(start, end - start);
					}
				}
			}
		}

		return "";
	}

	static vector<ParameterInfo> ExtractJavaParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find formal_parameters node
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "formal_parameters") == 0) {
				params = ExtractJavaParametersDirect(child, content);
				break;
			}
		}

		return params;
	}

	static vector<ParameterInfo> ExtractJavaParametersDirect(TSNode params_node, const string &content) {
		vector<ParameterInfo> parameters;

		uint32_t child_count = ts_node_child_count(params_node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(params_node, i);
			const char *child_type = ts_node_type(child);

			ParameterInfo param;
			bool is_valid_param = false;

			if (strcmp(child_type, "formal_parameter") == 0) {
				// Standard parameter: (Type param)
				param = ExtractFormalParameter(child, content);
				is_valid_param = !param.name.empty();
			} else if (strcmp(child_type, "spread_parameter") == 0) {
				// Varargs parameter: (Type... args)
				param = ExtractSpreadParameter(child, content);
				is_valid_param = !param.name.empty();
			}

			if (is_valid_param) {
				parameters.push_back(param);
			}
		}

		return parameters;
	}

	static ParameterInfo ExtractFormalParameter(TSNode node, const string &content) {
		ParameterInfo param;
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "primitive_type") == 0 ||
			    strcmp(child_type, "generic_type") == 0 || strcmp(child_type, "array_type") == 0) {
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
			} else if (strcmp(child_type, "modifiers") == 0) {
				// Parameter modifiers (final, etc.)
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.annotations = content.substr(start, end - start);
				}
			}
		}

		return param;
	}

	static ParameterInfo ExtractSpreadParameter(TSNode node, const string &content) {
		ParameterInfo param;
		param.is_variadic = true;
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "primitive_type") == 0 ||
			    strcmp(child_type, "generic_type") == 0) {
				// Varargs type (without the ...)
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.type = content.substr(start, end - start) + "[]";
				}
			} else if (strcmp(child_type, "identifier") == 0) {
				// Parameter name
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					param.name = content.substr(start, end - start);
				}
			}
		}

		return param;
	}

	static vector<string> ExtractJavaModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// First check within the node itself for modifiers
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				// Extract individual modifiers
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					const char *modifier_type = ts_node_type(modifier);
					uint32_t start = ts_node_start_byte(modifier);
					uint32_t end = ts_node_end_byte(modifier);
					if (start < content.length() && end <= content.length() && end > start) {
						modifiers.push_back(content.substr(start, end - start));
					}
				}
			} else if (strcmp(child_type, "annotation") == 0) {
				// Java annotations
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length() && end > start) {
					modifiers.push_back(content.substr(start, end - start));
				}
			}
		}

		// If no modifiers found, check parent (but avoid ts_node_parent() reliability issues)
		if (modifiers.empty()) {
			TSNode parent = ts_node_parent(node);
			if (!ts_node_is_null(parent)) {
				uint32_t parent_count = ts_node_child_count(parent);
				for (uint32_t i = 0; i < parent_count && i < 10; i++) { // Limit search
					TSNode sibling = ts_node_child(parent, i);
					const char *sibling_type = ts_node_type(sibling);

					if (strcmp(sibling_type, "modifiers") == 0) {
						// Extract individual modifiers
						uint32_t mod_count = ts_node_child_count(sibling);
						for (uint32_t j = 0; j < mod_count && j < 20; j++) { // Limit search
							TSNode modifier = ts_node_child(sibling, j);
							uint32_t start = ts_node_start_byte(modifier);
							uint32_t end = ts_node_end_byte(modifier);
							if (start < content.length() && end <= content.length() && end > start) {
								modifiers.push_back(content.substr(start, end - start));
							}
						}
					} else if (strcmp(sibling_type, "annotation") == 0) {
						// Java annotations
						uint32_t start = ts_node_start_byte(sibling);
						uint32_t end = ts_node_end_byte(sibling);
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
struct JavaNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		try {
			string node_type = ts_node_type(node);

			if (node_type == "class_declaration") {
				context.signature_type = ExtractClassType(node, content);
				context.parameters = ExtractClassParents(node, content);
				context.modifiers = ExtractJavaClassModifiers(node, content);
			} else if (node_type == "interface_declaration") {
				context.signature_type = ExtractInterfaceType(node, content);
				context.parameters = ExtractInterfaceParents(node, content);
				context.modifiers = ExtractInterfaceModifiers(node, content);
			} else if (node_type == "enum_declaration") {
				context.signature_type = ExtractEnumType(node, content);
				context.parameters = ExtractEnumInterfaces(node, content);
				context.modifiers = ExtractEnumModifiers(node, content);
			} else if (node_type == "annotation_type_declaration") {
				context.signature_type = ExtractAnnotationType(node, content);
				context.modifiers = ExtractAnnotationModifiers(node, content);
			} else {
				// Default class extraction
				context.signature_type = "class";
				context.parameters = ExtractClassParents(node, content);
				context.modifiers = ExtractJavaClassModifiers(node, content);
			}
		} catch (...) {
			context.signature_type = "class";
			context.modifiers.clear();
			context.parameters.clear();
		}

		return context;
	}

private:
	static string ExtractClassType(TSNode node, const string &content) {
		// Check for abstract class by looking at modifiers directly
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					uint32_t start = ts_node_start_byte(modifier);
					uint32_t end = ts_node_end_byte(modifier);
					if (start < content.length() && end <= content.length()) {
						string mod_text = content.substr(start, end - start);
						if (mod_text == "abstract") {
							return "abstract_class";
						}
					}
				}
			}
		}
		return "class";
	}

	static string ExtractInterfaceType(TSNode node, const string &content) {
		// Check for functional interface annotation directly
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					uint32_t start = ts_node_start_byte(modifier);
					uint32_t end = ts_node_end_byte(modifier);
					if (start < content.length() && end <= content.length()) {
						string mod_text = content.substr(start, end - start);
						if (mod_text.find("@FunctionalInterface") != string::npos) {
							return "functional_interface";
						}
					}
				}
			} else if (strcmp(child_type, "annotation") == 0 || strcmp(child_type, "marker_annotation") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					string annotation_text = content.substr(start, end - start);
					if (annotation_text.find("@FunctionalInterface") != string::npos) {
						return "functional_interface";
					}
				}
			}
		}

		// Check if interface has methods (not a marker interface)
		if (HasMethods(node, content)) {
			return "interface";
		} else {
			return "marker_interface";
		}
	}

	static string ExtractEnumType(TSNode node, const string &content) {
		// Check if enum has methods or just constants
		if (HasMethods(node, content)) {
			return "enum_with_methods";
		} else {
			return "enum";
		}
	}

	static string ExtractAnnotationType(TSNode node, const string &content) {
		return "annotation";
	}

	// Extract parent classes/interfaces as ParameterInfo (for classes)
	// Each parent has its inheritance kind stored in ParameterInfo.type ("extends" or "implements")
	static vector<ParameterInfo> ExtractClassParents(TSNode node, const string &content) {
		vector<ParameterInfo> parents;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "superclass") == 0) {
				// Extract extended class: "extends ClassName"
				ExtractTypeIdentifiers(child, content, parents, "extends");
			} else if (strcmp(child_type, "super_interfaces") == 0) {
				// Extract implemented interfaces: "implements Interface1, Interface2"
				ExtractTypeIdentifiers(child, content, parents, "implements");
			}
		}

		return parents;
	}

	// Extract parent interfaces for interface declarations
	// Interfaces extend other interfaces, so all parents have type="extends"
	static vector<ParameterInfo> ExtractInterfaceParents(TSNode node, const string &content) {
		vector<ParameterInfo> parents;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "extends_interfaces") == 0) {
				// Interface extends other interfaces
				ExtractTypeIdentifiers(child, content, parents, "extends");
			}
		}

		return parents;
	}

	// Extract implemented interfaces for enum declarations
	// Enums implement interfaces, so all parents have type="implements"
	static vector<ParameterInfo> ExtractEnumInterfaces(TSNode node, const string &content) {
		vector<ParameterInfo> parents;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "super_interfaces") == 0) {
				// Enum implements interfaces
				ExtractTypeIdentifiers(child, content, parents, "implements");
			}
		}

		return parents;
	}

	// Helper to extract type identifiers from inheritance clauses
	// inheritance_kind: "extends", "implements", etc. - stored in ParameterInfo.type
	static void ExtractTypeIdentifiers(TSNode clause_node, const string &content, vector<ParameterInfo> &parents,
	                                   const string &inheritance_kind) {
		uint32_t child_count = ts_node_child_count(clause_node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(clause_node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "generic_type") == 0 ||
			    strcmp(child_type, "scoped_type_identifier") == 0) {
				string type_name = ExtractNodeText(child, content);
				if (!type_name.empty()) {
					parents.push_back({type_name, inheritance_kind});
				}
			} else if (strcmp(child_type, "type_list") == 0) {
				// Handle type lists (for multiple interfaces)
				uint32_t list_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < list_count; j++) {
					TSNode list_child = ts_node_child(child, j);
					const char *list_child_type = ts_node_type(list_child);

					if (strcmp(list_child_type, "type_identifier") == 0 ||
					    strcmp(list_child_type, "generic_type") == 0 ||
					    strcmp(list_child_type, "scoped_type_identifier") == 0) {
						string type_name = ExtractNodeText(list_child, content);
						if (!type_name.empty()) {
							parents.push_back({type_name, inheritance_kind});
						}
					}
				}
			}
		}
	}

	static bool HasMethods(TSNode node, const string &content) {
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "class_body" || child_type == "interface_body" || child_type == "enum_body") {
				uint32_t body_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < body_count; j++) {
					TSNode body_child = ts_node_child(child, j);
					string body_child_type = ts_node_type(body_child);

					if (body_child_type == "method_declaration" || body_child_type == "constructor_declaration") {
						return true;
					}
				}
			}
		}
		return false;
	}

	// Extract class modifiers (public, abstract, final, etc.)
	// Note: inheritance keywords (extends/implements) are now in ParameterInfo.type, not here
	static vector<string> ExtractJavaClassModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "modifiers") == 0) {
				// Extract class modifiers (public, abstract, final, etc.)
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					uint32_t start = ts_node_start_byte(modifier);
					uint32_t end = ts_node_end_byte(modifier);
					if (start < content.length() && end <= content.length()) {
						modifiers.push_back(content.substr(start, end - start));
					}
				}
			} else if (strcmp(child_type, "annotation") == 0) {
				// Class annotations
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					modifiers.push_back(content.substr(start, end - start));
				}
			}
		}

		return modifiers;
	}

	// Extract interface modifiers (public, etc.)
	// Note: inheritance keywords are now in ParameterInfo.type, not here
	static vector<string> ExtractInterfaceModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("interface");

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "modifiers") {
				// Extract interface modifiers (public, etc.)
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					string modifier_text = ExtractNodeText(modifier, content);
					if (!modifier_text.empty()) {
						modifiers.push_back(modifier_text);
					}
				}
			} else if (child_type == "annotation") {
				// Interface annotations
				string annotation_text = ExtractNodeText(child, content);
				if (!annotation_text.empty()) {
					modifiers.push_back(annotation_text);
				}
			}
		}

		return modifiers;
	}

	// Extract enum modifiers (public, etc.)
	// Note: inheritance keywords are now in ParameterInfo.type, not here
	static vector<string> ExtractEnumModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("enum");

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "modifiers") {
				// Extract enum modifiers (public, etc.)
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					string modifier_text = ExtractNodeText(modifier, content);
					if (!modifier_text.empty()) {
						modifiers.push_back(modifier_text);
					}
				}
			} else if (child_type == "annotation") {
				// Enum annotations
				string annotation_text = ExtractNodeText(child, content);
				if (!annotation_text.empty()) {
					modifiers.push_back(annotation_text);
				}
			}
		}

		return modifiers;
	}

	static vector<string> ExtractAnnotationModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("annotation_type");

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "modifiers") {
				// Extract annotation modifiers (public, etc.)
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					string modifier_text = ExtractNodeText(modifier, content);
					if (!modifier_text.empty()) {
						modifiers.push_back(modifier_text);
					}
				}
			} else if (child_type == "annotation") {
				// Meta-annotations
				string annotation_text = ExtractNodeText(child, content);
				if (!annotation_text.empty()) {
					modifiers.push_back(annotation_text);
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
struct JavaNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
	static NativeContext Extract(TSNode node, const string &content) {
		return JavaNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
	}
};

// Specialization for VARIABLE_WITH_TYPE (Java field declarations)
template <>
struct JavaNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		try {
			string node_type = ts_node_type(node);

			if (node_type == "variable_declarator" || node_type == "field_declaration") {
				context.signature_type = ExtractJavaVariableType(node, content);
				context.modifiers = ExtractJavaVariableModifiers(node, content);
			} else if (node_type == "identifier") {
				context.signature_type = ExtractJavaIdentifierType(node, content);
				context.modifiers = ExtractJavaIdentifierModifiers(node, content);
			} else if (node_type == "type_identifier") {
				context.signature_type = ExtractJavaTypeIdentifierInfo(node, content);
				context.modifiers.push_back("type_reference");
			} else if (node_type == "primitive_type") {
				context.signature_type = ExtractNodeText(node, content);
				context.modifiers.push_back("primitive");
			} else if (node_type == "generic_type") {
				context.signature_type = ExtractNodeText(node, content);
				context.modifiers.push_back("generic");
			} else if (node_type == "array_type") {
				context.signature_type = ExtractNodeText(node, content);
				context.modifiers.push_back("array");
			} else if (node_type == "formal_parameter") {
				context.signature_type = ExtractJavaParameterType(node, content);
				context.modifiers = ExtractJavaParameterModifiers(node, content);
			} else if (node_type == "modifiers") {
				context.signature_type = "modifiers";
				context.modifiers = ExtractModifiersList(node, content);
			} else if (node_type == "field_access") {
				context.signature_type = ExtractFieldAccessType(node, content);
				context.modifiers = ExtractFieldAccessModifiers(node, content);
			} else if (node_type == "array_access") {
				context.signature_type = ExtractArrayAccessType(node, content);
				context.modifiers = ExtractArrayAccessModifiers(node, content);
			} else if (node_type == "method_invocation") {
				context.signature_type = ExtractMethodInvocationType(node, content);
				context.modifiers = ExtractMethodInvocationModifiers(node, content);
			} else {
				// Generic Java variable/type extraction
				context.signature_type = ExtractJavaVariableType(node, content);
				context.modifiers = ExtractJavaVariableModifiers(node, content);
			}
		} catch (...) {
			context.signature_type = "";
			context.modifiers.clear();
		}

		return context;
	}

private:
	static string ExtractJavaVariableType(TSNode node, const string &content) {
		// Look for type in variable declarator
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "primitive_type") == 0 ||
			    strcmp(child_type, "generic_type") == 0 || strcmp(child_type, "array_type") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.length() && end <= content.length()) {
					return content.substr(start, end - start);
				}
			}
		}

		return "";
	}

	static vector<string> ExtractJavaVariableModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check parent for field modifiers
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			uint32_t parent_count = ts_node_child_count(parent);
			for (uint32_t i = 0; i < parent_count; i++) {
				TSNode child = ts_node_child(parent, i);
				const char *child_type = ts_node_type(child);

				if (strcmp(child_type, "modifiers") == 0) {
					// Extract field modifiers (public, private, static, final, etc.)
					uint32_t mod_count = ts_node_child_count(child);
					for (uint32_t j = 0; j < mod_count; j++) {
						TSNode modifier = ts_node_child(child, j);
						uint32_t start = ts_node_start_byte(modifier);
						uint32_t end = ts_node_end_byte(modifier);
						if (start < content.length() && end <= content.length()) {
							modifiers.push_back(content.substr(start, end - start));
						}
					}
				} else if (strcmp(child_type, "annotation") == 0) {
					// Field annotations
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

	static string ExtractJavaIdentifierType(TSNode node, const string &content) {
		// For identifiers, check parent context to infer type
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			string parent_type = ts_node_type(parent);

			if (parent_type == "variable_declarator") {
				return ExtractJavaVariableType(parent, content);
			} else if (parent_type == "formal_parameter") {
				return ExtractJavaParameterType(parent, content);
			} else if (parent_type == "field_declaration") {
				return ExtractJavaVariableType(parent, content);
			} else if (parent_type == "method_invocation") {
				return "method_call";
			} else if (parent_type == "field_access") {
				return "field_access";
			}
		}

		return "identifier";
	}

	static vector<string> ExtractJavaIdentifierModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check if this identifier is in a specific context
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			string parent_type = ts_node_type(parent);
			modifiers.push_back("in_" + parent_type);

			// Check for additional context modifiers
			if (parent_type == "method_invocation") {
				modifiers.push_back("method_call");
			} else if (parent_type == "field_access") {
				modifiers.push_back("field_access");
			} else if (parent_type == "variable_declarator") {
				modifiers.push_back("variable_name");
			} else if (parent_type == "class_declaration") {
				modifiers.push_back("class_name");
			}
		}

		return modifiers;
	}

	static string ExtractJavaTypeIdentifierInfo(TSNode node, const string &content) {
		// Extract the actual type name
		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start < content.length() && end <= content.length() && end > start) {
			return content.substr(start, end - start);
		}

		return "type";
	}

	static string ExtractJavaParameterType(TSNode node, const string &content) {
		// Look for type in formal parameter
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char *child_type = ts_node_type(child);

			if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "primitive_type") == 0 ||
			    strcmp(child_type, "generic_type") == 0 || strcmp(child_type, "array_type") == 0) {
				return ExtractNodeText(child, content);
			}
		}

		return "parameter";
	}

	static vector<string> ExtractJavaParameterModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("method_parameter");

		// Check for parameter modifiers (final, annotations)
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "modifiers") {
				vector<string> param_mods = ExtractModifiersList(child, content);
				modifiers.insert(modifiers.end(), param_mods.begin(), param_mods.end());
			} else if (child_type == "annotation") {
				modifiers.push_back("annotated");
			}
		}

		return modifiers;
	}

	static vector<string> ExtractModifiersList(TSNode modifiers_node, const string &content) {
		vector<string> modifiers;

		uint32_t child_count = ts_node_child_count(modifiers_node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(modifiers_node, i);
			string child_type = ts_node_type(child);

			if (child_type == "public" || child_type == "private" || child_type == "protected" ||
			    child_type == "static" || child_type == "final" || child_type == "abstract" ||
			    child_type == "synchronized" || child_type == "volatile" || child_type == "transient") {
				modifiers.push_back(child_type);
			} else {
				// Extract text for other modifiers
				string modifier_text = ExtractNodeText(child, content);
				if (!modifier_text.empty()) {
					modifiers.push_back(modifier_text);
				}
			}
		}

		return modifiers;
	}

	static string ExtractFieldAccessType(TSNode node, const string &content) {
		// For field access expressions like obj.field
		return "field_access";
	}

	static vector<string> ExtractFieldAccessModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("field_access");

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (i == 0) {
				modifiers.push_back("object_" + child_type);
			} else if (child_type == "identifier") {
				modifiers.push_back("field_name");
			}
		}

		// Check if it's a static field access (ClassName.field)
		if (child_count >= 2) {
			TSNode first_child = ts_node_child(node, 0);
			string first_type = ts_node_type(first_child);

			if (first_type == "type_identifier") {
				modifiers.push_back("static_field");
			} else {
				modifiers.push_back("instance_field");
			}
		}

		return modifiers;
	}

	static string ExtractArrayAccessType(TSNode node, const string &content) {
		// For array access expressions like arr[index]
		return "array_access";
	}

	static vector<string> ExtractArrayAccessModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("array_access");
		modifiers.push_back("subscript_operation");

		// Analyze the index expression
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "decimal_integer_literal") {
				modifiers.push_back("constant_index");
			} else if (child_type == "identifier") {
				modifiers.push_back("variable_index");
			} else if (child_type == "method_invocation") {
				modifiers.push_back("computed_index");
			}
		}

		return modifiers;
	}

	static string ExtractMethodInvocationType(TSNode node, const string &content) {
		// For method invocations like obj.method() - when captured as variable context
		return "method_reference";
	}

	static vector<string> ExtractMethodInvocationModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("method_invocation");

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (i == 0) {
				if (child_type == "type_identifier") {
					modifiers.push_back("static_method");
				} else {
					modifiers.push_back("instance_method");
				}
				modifiers.push_back("object_" + child_type);
			} else if (child_type == "identifier") {
				modifiers.push_back("method_name");
			} else if (child_type == "argument_list") {
				modifiers.push_back("has_arguments");
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

// Specialization for FUNCTION_CALL (Java method invocations and object creation)
template <>
struct JavaNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		return UnifiedFunctionCallExtractor<JavaLanguageTag>::Extract(node, content);
	}
};

// Specialization for FUNCTION_WITH_DECORATORS (Java methods with annotations)
template <>
struct JavaNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_DECORATORS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;

		try {
			string node_type = ts_node_type(node);

			if (node_type == "method_declaration" || node_type == "constructor_declaration") {
				// Start with basic function extraction
				context = JavaNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);

				// Add annotations and advanced modifiers
				vector<string> annotations = ExtractJavaAnnotations(node, content);
				vector<string> advanced_modifiers = ExtractJavaAdvancedModifiers(node, content);

				// Combine existing modifiers with annotations and advanced modifiers
				context.modifiers.insert(context.modifiers.end(), annotations.begin(), annotations.end());
				context.modifiers.insert(context.modifiers.end(), advanced_modifiers.begin(), advanced_modifiers.end());

				// Enhance signature type with annotation info
				if (!annotations.empty()) {
					context.signature_type = "annotated_" + context.signature_type;
				}
			} else {
				// Fallback to basic function extraction
				context = JavaNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
			}
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}

		return context;
	}

private:
	static vector<string> ExtractJavaAnnotations(TSNode node, const string &content) {
		vector<string> annotations;

		// Check parent and siblings for annotations
		TSNode parent = ts_node_parent(node);
		if (!ts_node_is_null(parent)) {
			uint32_t parent_count = ts_node_child_count(parent);
			for (uint32_t i = 0; i < parent_count; i++) {
				TSNode sibling = ts_node_child(parent, i);
				string sibling_type = ts_node_type(sibling);

				if (sibling_type == "annotation") {
					string annotation_text = ExtractNodeText(sibling, content);
					if (!annotation_text.empty()) {
						annotations.push_back(annotation_text);
					}
				} else if (sibling_type == "marker_annotation") {
					string annotation_text = ExtractNodeText(sibling, content);
					if (!annotation_text.empty()) {
						annotations.push_back(annotation_text);
					}
				}
			}
		}

		// Check within the node itself
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "annotation" || child_type == "marker_annotation") {
				string annotation_text = ExtractNodeText(child, content);
				if (!annotation_text.empty()) {
					annotations.push_back(annotation_text);
				}
			}
		}

		return annotations;
	}

	static vector<string> ExtractJavaAdvancedModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "modifiers") {
				// Extract individual Java modifiers
				uint32_t mod_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < mod_count; j++) {
					TSNode modifier = ts_node_child(child, j);
					string modifier_type = ts_node_type(modifier);

					if (modifier_type == "synchronized") {
						modifiers.push_back("synchronized");
					} else if (modifier_type == "native") {
						modifiers.push_back("native");
					} else if (modifier_type == "strictfp") {
						modifiers.push_back("strictfp");
					} else if (modifier_type == "transient") {
						modifiers.push_back("transient");
					} else if (modifier_type == "volatile") {
						modifiers.push_back("volatile");
					} else {
						// Extract text for other modifiers
						string modifier_text = ExtractNodeText(modifier, content);
						if (!modifier_text.empty()) {
							modifiers.push_back(modifier_text);
						}
					}
				}
			}
		}

		// Check for generic type parameters
		bool has_generics = false;
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "type_parameters") {
				has_generics = true;
				modifiers.push_back("generic");
				break;
			}
		}

		// Check for throws clause
		bool has_throws = false;
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "throws") {
				has_throws = true;
				modifiers.push_back("throws_exceptions");

				// Extract the exception types
				string throws_text = ExtractNodeText(child, content);
				if (!throws_text.empty()) {
					modifiers.push_back(throws_text);
				}
				break;
			}
		}

		// Check for varargs parameters
		bool has_varargs = false;
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "formal_parameters") {
				uint32_t param_count = ts_node_child_count(child);
				for (uint32_t j = 0; j < param_count; j++) {
					TSNode param = ts_node_child(child, j);
					string param_type = ts_node_type(param);

					if (param_type == "spread_parameter") {
						has_varargs = true;
						modifiers.push_back("varargs");
						break;
					}
				}
				if (has_varargs)
					break;
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

} // namespace duckdb
