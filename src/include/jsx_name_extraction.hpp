#pragma once

#include <tree_sitter/api.h>
#include <cstdint>
#include <cstring>
#include <string>

namespace duckdb {

//==============================================================================
// JSX names, shared by JavaScriptAdapter and TypeScriptAdapter
//==============================================================================
//
// WHY A SHARED HEADER AND NOT A METHOD ON EITHER ADAPTER. JSX reaches three
// languages. `.jsx` files route to javascript and therefore to
// JavaScriptAdapter::ExtractNodeName; `.tsx` files route to tsx, and TSXAdapter
// derives from TypeScriptAdapter, so they take TypeScriptAdapter's method. The
// two adapters are SIBLINGS -- both derive straight from LanguageAdapter -- and
// share no name extraction. Worse, the failure is silent: the generic CUSTOM
// case in LanguageAdapter::ExtractByStrategy returns the empty string. A helper
// wired into one adapter and not the other gives the two file extensions
// different answers and no error. PR #329 shipped that class of problem four
// times over, so the logic lives in one place both call.
//
// WHY NOT A GENERIC ExtractionStrategy. The grammar gives a JSX tag one of
// three node types:
//
//     <div>         jsx_opening_element > identifier
//     <Foo.Bar>     jsx_opening_element > member_expression
//     <svg:circle/> jsx_self_closing_element > jsx_namespace_name
//
// FIND_IDENTIFIER descends two wrapper levels, so it returns `Foo` for
// <Foo.Bar> and `svg` for <svg:circle>. Both look right and are wrong.
// FIND_PROPERTY cannot see a namespaced attribute name (xlink:href) at all.
// See tracker/features/058-jsx-node-types.md.

//! The node types that can carry a JSX tag name.
inline bool IsJSXTagNode(const char *node_type) {
	return strcmp(node_type, "identifier") == 0 || strcmp(node_type, "member_expression") == 0 ||
	       strcmp(node_type, "jsx_namespace_name") == 0;
}

//! A node's source text, verbatim. `Foo.Bar` and `svg:circle` are spelled by
//! their own text; no child of theirs is the whole name.
inline std::string JSXNodeText(TSNode node, const std::string &content) {
	const uint32_t start = ts_node_start_byte(node);
	const uint32_t end = ts_node_end_byte(node);
	if (start >= end || end > content.size()) {
		return "";
	}
	return content.substr(start, end - start);
}

//! The JSX name for the five node types that declare a CUSTOM strategy.
//! Returns the empty string for any other type, and for a FRAGMENT: `<>...</>`
//! is a jsx_element whose jsx_opening_element holds only the two angle
//! brackets, so it names nothing. Inventing a name there would be worse.
inline std::string ExtractJSXName(TSNode node, const std::string &content) {
	const char *node_type = ts_node_type(node);

	// An attribute is named by its own key. A plain prop puts that key in a
	// property_identifier; a namespaced prop (xlink:href) puts it in a
	// jsx_namespace_name, which is why FIND_PROPERTY is not enough.
	if (strcmp(node_type, "jsx_attribute") == 0) {
		const uint32_t child_count = ts_node_named_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_named_child(node, i);
			const char *child_type = ts_node_type(child);
			if (strcmp(child_type, "property_identifier") == 0 || strcmp(child_type, "jsx_namespace_name") == 0) {
				return JSXNodeText(child, content);
			}
		}
		return "";
	}

	// An element is named by its tag, which sits one level down on the opening
	// element. This is HTML's shape too: html_adapter.cpp reads an element's
	// name from its start_tag's tag_name.
	if (strcmp(node_type, "jsx_element") == 0) {
		const uint32_t child_count = ts_node_named_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_named_child(node, i);
			if (strcmp(ts_node_type(child), "jsx_opening_element") == 0) {
				return ExtractJSXName(child, content);
			}
		}
		return "";
	}

	// The three tag-bearing types carry the tag as their first named child that
	// can be one. A fragment has no such child and reports no name.
	if (strcmp(node_type, "jsx_opening_element") == 0 || strcmp(node_type, "jsx_closing_element") == 0 ||
	    strcmp(node_type, "jsx_self_closing_element") == 0) {
		const uint32_t child_count = ts_node_named_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_named_child(node, i);
			if (IsJSXTagNode(ts_node_type(child))) {
				return JSXNodeText(child, content);
			}
		}
		return "";
	}

	return "";
}

} // namespace duckdb
