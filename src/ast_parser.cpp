#include "ast_parser.hpp"
#include "grammars.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include <cstring>

// Tree-sitter headers
extern "C" {
#include <tree_sitter/api.h>
}

namespace duckdb {

TSParser* ASTParser::CreateParser(const string &language) {
	TSParser *parser = ts_parser_new();
	if (!parser) {
		throw InvalidInputException("Failed to create tree-sitter parser");
	}
	
	// Get the language from our grammars module
	const TSLanguage *ts_language = GetLanguage(language);
	if (!ts_language) {
		ts_parser_delete(parser);
		auto supported = GetSupportedLanguages();
		string supported_str = StringUtil::Join(supported, ", ");
		throw InvalidInputException("Unsupported language: %s. Supported languages: %s", language, supported_str);
	}
	
	if (!ts_parser_set_language(parser, ts_language)) {
		ts_parser_delete(parser);
		throw InvalidInputException("Failed to set language for parser");
	}
	
	return parser;
}

TSTree* ASTParser::ParseString(const string &content, TSParser *parser) {
	TSTree *tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.length());
	if (!tree) {
		throw InvalidInputException("Failed to parse content");
	}
	return tree;
}

string ASTParser::ExtractNodeName(TSNode node, const string &content) {
	const char* node_type_str = ts_node_type(node);
	
	// Extract names for specific node types
	if (strcmp(node_type_str, "function_definition") == 0 || 
	    strcmp(node_type_str, "class_definition") == 0) {
		// For functions and classes, look for identifier child
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			const char* child_type = ts_node_type(child);
			if (strcmp(child_type, "identifier") == 0) {
				uint32_t start = ts_node_start_byte(child);
				uint32_t end = ts_node_end_byte(child);
				if (start < content.size() && end <= content.size()) {
					return content.substr(start, end - start);
				}
			}
		}
	} else if (strcmp(node_type_str, "identifier") == 0) {
		// For identifier nodes, use the node text itself
		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);
		if (start < content.size() && end <= content.size()) {
			return content.substr(start, end - start);
		}
	}
	
	return "";
}

} // namespace duckdb