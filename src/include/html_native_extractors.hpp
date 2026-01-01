#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>
#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;

namespace duckdb {

// Forward declaration
class HTMLAdapter;

//==============================================================================
// HTML Native Context Extraction
//==============================================================================

template <NativeExtractionStrategy Strategy>
struct HTMLNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content);
};

//==============================================================================
// HTML Element/Tag Extraction (for elements, script/style tags, forms)
//==============================================================================

template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		try {
			string node_type = ts_node_type(node);

			if (node_type == "element") {
				context.signature_type = "ELEMENT";
				context.parameters = ExtractElementAttributes(node, content);
				context.modifiers = ExtractElementModifiers(node, content);
			} else if (node_type == "script_element") {
				context.signature_type = "SCRIPT";
				context.parameters = ExtractScriptAttributes(node, content);
				context.modifiers = ExtractScriptModifiers(node, content);
			} else if (node_type == "style_element") {
				context.signature_type = "STYLE";
				context.parameters = ExtractStyleAttributes(node, content);
				context.modifiers = ExtractStyleModifiers(node, content);
			} else if (node_type == "start_tag") {
				context.signature_type = "START_TAG";
				context.parameters = ExtractTagAttributes(node, content);
				context.modifiers = ExtractTagModifiers(node, content);
			} else if (node_type == "self_closing_tag") {
				context.signature_type = "SELF_CLOSING_TAG";
				context.parameters = ExtractTagAttributes(node, content);
				context.modifiers = ExtractSelfClosingModifiers(node, content);
			} else {
				// Generic HTML construct
				context.signature_type = "HTML";
				context.parameters.clear();
				context.modifiers.clear();
			}
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}
		return context;
	}

	// Public static methods for HTML element extraction
	static vector<ParameterInfo> ExtractElementAttributes(TSNode node, const string &content) {
		vector<ParameterInfo> attributes;

		// Find start_tag in element
		TSNode start_tag = FindChildByType(node, "start_tag");
		if (!ts_node_is_null(start_tag)) {
			attributes = ExtractTagAttributes(start_tag, content);
		}

		return attributes;
	}

	static vector<ParameterInfo> ExtractTagAttributes(TSNode node, const string &content) {
		vector<ParameterInfo> attributes;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "attribute") {
				ParameterInfo attr = ExtractAttributeInfo(child, content);
				if (!attr.name.empty()) {
					attributes.push_back(attr);
				}
			}
		}

		return attributes;
	}

	static ParameterInfo ExtractAttributeInfo(TSNode attr_node, const string &content) {
		ParameterInfo info;
		info.is_optional = false;
		info.is_variadic = false;

		uint32_t child_count = ts_node_child_count(attr_node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(attr_node, i);
			string child_type = ts_node_type(child);

			if (child_type == "attribute_name") {
				info.name = ExtractNodeText(child, content);
			} else if (child_type == "attribute_value" || child_type == "quoted_attribute_value") {
				info.type = ExtractNodeText(child, content);
				// Remove quotes from quoted values
				if (!info.type.empty() && (info.type.front() == '"' || info.type.front() == '\'')) {
					info.type = info.type.substr(1, info.type.length() - 2);
				}
			}
		}

		return info;
	}

	static vector<ParameterInfo> ExtractScriptAttributes(TSNode node, const string &content) {
		vector<ParameterInfo> attributes;

		// Scripts have start_tag with attributes
		TSNode start_tag = FindChildByType(node, "start_tag");
		if (!ts_node_is_null(start_tag)) {
			attributes = ExtractTagAttributes(start_tag, content);
		}

		// Add script content as special parameter
		TSNode raw_text = FindChildByType(node, "raw_text");
		if (!ts_node_is_null(raw_text)) {
			ParameterInfo content_param;
			content_param.name = "script_content";
			content_param.type = "JAVASCRIPT";
			content_param.is_optional = false;
			content_param.is_variadic = false;
			attributes.push_back(content_param);
		}

		return attributes;
	}

	static vector<ParameterInfo> ExtractStyleAttributes(TSNode node, const string &content) {
		vector<ParameterInfo> attributes;

		// Styles have start_tag with attributes
		TSNode start_tag = FindChildByType(node, "start_tag");
		if (!ts_node_is_null(start_tag)) {
			attributes = ExtractTagAttributes(start_tag, content);
		}

		// Add style content as special parameter
		TSNode raw_text = FindChildByType(node, "raw_text");
		if (!ts_node_is_null(raw_text)) {
			ParameterInfo content_param;
			content_param.name = "style_content";
			content_param.type = "CSS";
			content_param.is_optional = false;
			content_param.is_variadic = false;
			attributes.push_back(content_param);
		}

		return attributes;
	}

	static vector<string> ExtractElementModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Extract tag name as modifier
		string tag_name = ExtractTagName(node);
		if (!tag_name.empty()) {
			modifiers.push_back("TAG_" + StringUtil::Upper(tag_name));
		}

		// Check for semantic HTML5 elements
		if (IsSemanticElement(tag_name)) {
			modifiers.push_back("SEMANTIC");
		}

		// Check for form elements
		if (IsFormElement(tag_name)) {
			modifiers.push_back("FORM_ELEMENT");
		}

		// Check for interactive elements
		if (IsInteractiveElement(tag_name)) {
			modifiers.push_back("INTERACTIVE");
		}

		return modifiers;
	}

	static vector<string> ExtractScriptModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("SCRIPT");
		modifiers.push_back("EXECUTABLE");

		// Check for script type in attributes
		vector<ParameterInfo> attrs = ExtractScriptAttributes(node, content);
		for (const auto &attr : attrs) {
			if (attr.name == "type") {
				if (attr.type.find("module") != string::npos) {
					modifiers.push_back("MODULE");
				}
				break;
			}
		}

		return modifiers;
	}

	static vector<string> ExtractStyleModifiers(TSNode node, const string &content) {
		vector<string> modifiers;
		modifiers.push_back("STYLE");
		modifiers.push_back("STYLESHEET");

		return modifiers;
	}

	static vector<string> ExtractTagModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Extract tag name and categorize
		string tag_name = ExtractTagNameFromTag(node, content);
		if (!tag_name.empty()) {
			modifiers.push_back("TAG_" + StringUtil::Upper(tag_name));

			if (IsSemanticElement(tag_name)) {
				modifiers.push_back("SEMANTIC");
			}

			if (IsFormElement(tag_name)) {
				modifiers.push_back("FORM_ELEMENT");
			}
		}

		return modifiers;
	}

	static vector<string> ExtractSelfClosingModifiers(TSNode node, const string &content) {
		vector<string> modifiers = ExtractTagModifiers(node, content);
		modifiers.push_back("SELF_CLOSING");

		return modifiers;
	}

	static string ExtractTagName(TSNode element_node) {
		// For element nodes, find start_tag -> tag_name
		TSNode start_tag = FindChildByType(element_node, "start_tag");
		if (!ts_node_is_null(start_tag)) {
			TSNode tag_name = FindChildByType(start_tag, "tag_name");
			if (!ts_node_is_null(tag_name)) {
				return ExtractNodeText(tag_name, "");
			}
		}
		return "";
	}

	static string ExtractTagNameFromTag(TSNode tag_node, const string &content) {
		// For start_tag or self_closing_tag nodes
		TSNode tag_name = FindChildByType(tag_node, "tag_name");
		if (!ts_node_is_null(tag_name)) {
			return ExtractNodeText(tag_name, content);
		}
		return "";
	}

	static bool IsSemanticElement(const string &tag_name) {
		static const set<string> semantic_tags = {"header", "nav",    "main",       "section", "article", "aside",
		                                          "footer", "figure", "figcaption", "time",    "mark",    "address"};
		return semantic_tags.find(tag_name) != semantic_tags.end();
	}

	static bool IsFormElement(const string &tag_name) {
		static const set<string> form_tags = {"form",     "input",  "textarea", "select", "option",   "button", "label",
		                                      "fieldset", "legend", "datalist", "output", "progress", "meter"};
		return form_tags.find(tag_name) != form_tags.end();
	}

	static bool IsInteractiveElement(const string &tag_name) {
		static const set<string> interactive_tags = {"a",       "button",  "input",  "select", "textarea",
		                                             "details", "summary", "dialog", "embed",  "iframe",
		                                             "img",     "audio",   "video"};
		return interactive_tags.find(tag_name) != interactive_tags.end();
	}

	static TSNode FindChildByType(TSNode parent, const string &type) {
		uint32_t child_count = ts_node_child_count(parent);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(parent, i);
			if (string(ts_node_type(child)) == type) {
				return child;
			}
		}
		return {0};
	}

	static string ExtractNodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node)) {
			return "";
		}

		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start >= content.length() || end > content.length() || start >= end) {
			return "";
		}

		return content.substr(start, end - start);
	}
};

//==============================================================================
// HTML Attribute/Value Extraction
//==============================================================================

template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		try {
			string node_type = ts_node_type(node);

			if (node_type == "attribute") {
				context.signature_type = ExtractAttributeType(node, content);
				context.parameters.clear();
				context.modifiers = ExtractAttributeModifiers(node, content);
			} else if (node_type == "attribute_name") {
				context.signature_type = "ATTRIBUTE_NAME";
				context.parameters.clear();
				context.modifiers = ExtractAttributeNameModifiers(node, content);
			} else if (node_type == "attribute_value" || node_type == "quoted_attribute_value") {
				context.signature_type = "ATTRIBUTE_VALUE";
				context.parameters.clear();
				context.modifiers = ExtractAttributeValueModifiers(node, content);
			} else if (node_type == "tag_name") {
				context.signature_type = "TAG_NAME";
				context.parameters.clear();
				context.modifiers = ExtractTagNameModifiers(node, content);
			} else {
				context.signature_type = "";
				context.parameters.clear();
				context.modifiers.clear();
			}
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}
		return context;
	}

	// Public static methods for HTML attribute extraction
	static string ExtractAttributeType(TSNode node, const string &content) {
		// Get attribute name to determine type
		TSNode attr_name = FindChildByType(node, "attribute_name");
		if (!ts_node_is_null(attr_name)) {
			string name = ExtractNodeText(attr_name, content);
			return CategorizeAttribute(name);
		}
		return "ATTRIBUTE";
	}

	static vector<string> ExtractAttributeModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		TSNode attr_name = FindChildByType(node, "attribute_name");
		if (!ts_node_is_null(attr_name)) {
			string name = ExtractNodeText(attr_name, content);

			// Categorize attribute by function
			if (IsIdAttribute(name)) {
				modifiers.push_back("IDENTIFIER");
			} else if (IsClassAttribute(name)) {
				modifiers.push_back("STYLING");
			} else if (IsEventAttribute(name)) {
				modifiers.push_back("EVENT_HANDLER");
			} else if (IsDataAttribute(name)) {
				modifiers.push_back("DATA_ATTRIBUTE");
			} else if (IsAriaAttribute(name)) {
				modifiers.push_back("ACCESSIBILITY");
			} else if (IsUrlAttribute(name)) {
				modifiers.push_back("URL_REFERENCE");
			} else if (IsFormAttribute(name)) {
				modifiers.push_back("FORM_CONTROL");
			} else if (IsMediaAttribute(name)) {
				modifiers.push_back("MEDIA_CONTROL");
			}
		}

		return modifiers;
	}

	static vector<string> ExtractAttributeNameModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		string name = ExtractNodeText(node, content);
		if (IsStandardAttribute(name)) {
			modifiers.push_back("STANDARD");
		} else if (IsCustomAttribute(name)) {
			modifiers.push_back("CUSTOM");
		}

		return modifiers;
	}

	static vector<string> ExtractAttributeValueModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		string value = ExtractNodeText(node, content);

		// Remove quotes if present
		if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
			value = value.substr(1, value.length() - 2);
		}

		// Categorize value type
		if (IsUrl(value)) {
			modifiers.push_back("URL");
		} else if (IsColor(value)) {
			modifiers.push_back("COLOR");
		} else if (IsNumber(value)) {
			modifiers.push_back("NUMBER");
		} else if (IsBooleanValue(value)) {
			modifiers.push_back("BOOLEAN");
		} else if (IsClassList(value)) {
			modifiers.push_back("CLASS_LIST");
		} else {
			modifiers.push_back("TEXT");
		}

		return modifiers;
	}

	static vector<string> ExtractTagNameModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		string tag_name = ExtractNodeText(node, content);

		if (IsBlockElement(tag_name)) {
			modifiers.push_back("BLOCK");
		} else if (IsInlineElement(tag_name)) {
			modifiers.push_back("INLINE");
		}

		if (IsVoidElement(tag_name)) {
			modifiers.push_back("VOID");
		}

		return modifiers;
	}

	// Utility methods for attribute categorization
	static string CategorizeAttribute(const string &name) {
		if (IsIdAttribute(name))
			return "ID_ATTRIBUTE";
		if (IsClassAttribute(name))
			return "CLASS_ATTRIBUTE";
		if (IsEventAttribute(name))
			return "EVENT_ATTRIBUTE";
		if (IsDataAttribute(name))
			return "DATA_ATTRIBUTE";
		if (IsAriaAttribute(name))
			return "ARIA_ATTRIBUTE";
		if (IsUrlAttribute(name))
			return "URL_ATTRIBUTE";
		if (IsFormAttribute(name))
			return "FORM_ATTRIBUTE";
		if (IsMediaAttribute(name))
			return "MEDIA_ATTRIBUTE";
		return "GENERIC_ATTRIBUTE";
	}

	static bool IsIdAttribute(const string &name) {
		return name == "id";
	}

	static bool IsClassAttribute(const string &name) {
		return name == "class";
	}

	static bool IsEventAttribute(const string &name) {
		return name.substr(0, 2) == "on"; // onclick, onload, etc.
	}

	static bool IsDataAttribute(const string &name) {
		return name.substr(0, 5) == "data-";
	}

	static bool IsAriaAttribute(const string &name) {
		return name.substr(0, 5) == "aria-";
	}

	static bool IsUrlAttribute(const string &name) {
		static const set<string> url_attrs = {"href", "src", "action", "cite", "formaction"};
		return url_attrs.find(name) != url_attrs.end();
	}

	static bool IsFormAttribute(const string &name) {
		static const set<string> form_attrs = {"name",     "value",    "type",    "placeholder", "required",
		                                       "disabled", "readonly", "checked", "selected",    "multiple",
		                                       "pattern",  "min",      "max"};
		return form_attrs.find(name) != form_attrs.end();
	}

	static bool IsMediaAttribute(const string &name) {
		static const set<string> media_attrs = {"width",    "height", "alt",   "controls",
		                                        "autoplay", "loop",   "muted", "poster"};
		return media_attrs.find(name) != media_attrs.end();
	}

	static bool IsStandardAttribute(const string &name) {
		static const set<string> standard_attrs = {
		    "id",       "class",     "style",           "title",     "lang",       "dir",      "hidden",
		    "tabindex", "accesskey", "contenteditable", "draggable", "spellcheck", "translate"};
		return standard_attrs.find(name) != standard_attrs.end();
	}

	static bool IsCustomAttribute(const string &name) {
		return name.substr(0, 5) == "data-" || name.find('-') != string::npos;
	}

	static bool IsUrl(const string &value) {
		return value.substr(0, 4) == "http" || value.substr(0, 2) == "//" || value.substr(0, 1) == "/" ||
		       value.substr(0, 1) == "#";
	}

	static bool IsColor(const string &value) {
		return value.substr(0, 1) == "#" || value.substr(0, 3) == "rgb" || value.substr(0, 3) == "hsl";
	}

	static bool IsNumber(const string &value) {
		if (value.empty())
			return false;
		for (char c : value) {
			if (!isdigit(c) && c != '.' && c != '-')
				return false;
		}
		return true;
	}

	static bool IsBooleanValue(const string &value) {
		return value == "true" || value == "false" || value == "checked" || value == "selected" || value == "disabled";
	}

	static bool IsClassList(const string &value) {
		return value.find(' ') != string::npos; // Multiple classes
	}

	static bool IsBlockElement(const string &tag) {
		static const set<string> block_elements = {"div",   "p",       "h1",      "h2",     "h3",     "h4",  "h5",
		                                           "h6",    "section", "article", "header", "footer", "nav", "main",
		                                           "aside", "form",    "table",   "ul",     "ol"};
		return block_elements.find(tag) != block_elements.end();
	}

	static bool IsInlineElement(const string &tag) {
		static const set<string> inline_elements = {"span",  "a",    "strong", "em", "code",
		                                            "small", "mark", "time",   "b",  "i"};
		return inline_elements.find(tag) != inline_elements.end();
	}

	static bool IsVoidElement(const string &tag) {
		static const set<string> void_elements = {"area",  "base", "br",   "col",   "embed",  "hr",    "img",
		                                          "input", "link", "meta", "param", "source", "track", "wbr"};
		return void_elements.find(tag) != void_elements.end();
	}

	static TSNode FindChildByType(TSNode parent, const string &type) {
		uint32_t child_count = ts_node_child_count(parent);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(parent, i);
			if (string(ts_node_type(child)) == type) {
				return child;
			}
		}
		return {0};
	}

	static string ExtractNodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node)) {
			return "";
		}

		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start >= content.length() || end > content.length() || start >= end) {
			return "";
		}

		return content.substr(start, end - start);
	}
};

//==============================================================================
// HTML Class/Component Extraction (for complex HTML structures)
//==============================================================================

template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		// HTML doesn't have classes in the traditional sense
		// This could be used for component-like structures
		return NativeContext();
	}
};

//==============================================================================
// HTML Async/Dynamic Content Extraction
//==============================================================================

template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		// HTML doesn't have async functions
		return NativeContext();
	}
};

//==============================================================================
// HTML Arrow Function Extraction
//==============================================================================

template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		// HTML doesn't have arrow functions
		return NativeContext();
	}
};

//==============================================================================
// HTML Inheritance/Extension Extraction
//==============================================================================

template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
	static NativeContext Extract(TSNode node, const string &content) {
		// HTML doesn't have inheritance
		return NativeContext();
	}
};

//==============================================================================
// HTML Decorator/Annotation Extraction
//==============================================================================

template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_DECORATORS> {
	static NativeContext Extract(TSNode node, const string &content) {
		// HTML doesn't have decorators
		return NativeContext();
	}
};

// Specialization for FUNCTION_CALL (HTML function calls)
template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "html_function_call"; // Placeholder
		return context;
	}
};

// Specialization for CUSTOM (HTML custom extraction)
template <>
struct HTMLNativeExtractor<NativeExtractionStrategy::CUSTOM> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "html_custom"; // Placeholder
		return context;
	}
};

} // namespace duckdb
