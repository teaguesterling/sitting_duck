#include "ast_parser.hpp"
#include "grammars.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types/hash.hpp"

// Tree-sitter headers
extern "C" {
#include <tree_sitter/api.h>
}

namespace duckdb {

TreeSitterParser::TreeSitterParser(const string &language) {
	parser = ts_parser_new();
	if (!parser) {
		throw InvalidInputException("Failed to create tree-sitter parser");
	}
	
	// Get the language from our grammars module
	this->language = GetLanguage(language);
	if (!this->language) {
		ts_parser_delete(parser);
		auto supported = GetSupportedLanguages();
		string supported_str = StringUtil::Join(supported, ", ");
		throw InvalidInputException("Unsupported language: %s. Supported languages: %s", language, supported_str);
	}
	
	if (!ts_parser_set_language(parser, this->language)) {
		ts_parser_delete(parser);
		throw InvalidInputException("Failed to set language for parser");
	}
}

TreeSitterParser::~TreeSitterParser() {
	if (parser) {
		ts_parser_delete(parser);
	}
}

vector<ASTNode> TreeSitterParser::ParseFile(const string &file_path, FileSystem &fs) {
	if (!fs.FileExists(file_path)) {
		throw IOException("File not found: %s", file_path);
	}
	
	// Read file content
	auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
	auto file_size = fs.GetFileSize(*handle);
	string content(file_size, '\0');
	fs.Read(*handle, (void*)content.data(), file_size);
	handle->Close();
	
	return ParseString(content, file_path);
}

vector<ASTNode> TreeSitterParser::ParseString(const string &content, const string &file_path) {
	TSTree *tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.length());
	if (!tree) {
		throw InvalidInputException("Failed to parse content");
	}
	
	auto nodes = TreeToNodes(tree, content, file_path);
	ts_tree_delete(tree);
	return nodes;
}

vector<ASTNode> TreeSitterParser::TreeToNodes(TSTree *tree, const string &content, const string &file_path) {
	vector<ASTNode> nodes;
	TSNode root = ts_tree_root_node(tree);
	ProcessNode(root, content, file_path, nodes, -1, 0, 0);
	return nodes;
}

void TreeSitterParser::ProcessNode(TSNode node, const string &content, const string &file_path,
                                  vector<ASTNode> &nodes, int64_t parent_id, int32_t depth, int32_t sibling_index) {
	ASTNode ast_node;
	
	// Get node information
	TSPoint start_point = ts_node_start_point(node);
	TSPoint end_point = ts_node_end_point(node);
	uint32_t start_byte = ts_node_start_byte(node);
	uint32_t end_byte = ts_node_end_byte(node);
	
	// Generate unique node ID
	ast_node.node_id = GenerateNodeId(file_path, start_byte, end_byte);
	
	// Node type
	ast_node.type = ts_node_type(node);
	
	// Try to get node name (for named nodes like function names)
	if (ts_node_is_named(node)) {
		// For now, we'll leave name extraction for phase 2
		ast_node.name = "";
	}
	
	// File and position information
	ast_node.file_path = file_path;
	ast_node.start_line = start_point.row + 1;  // Convert to 1-based
	ast_node.start_column = start_point.column;
	ast_node.end_line = end_point.row + 1;      // Convert to 1-based
	ast_node.end_column = end_point.column;
	
	// Tree structure information
	ast_node.parent_id = parent_id;
	ast_node.depth = depth;
	ast_node.sibling_index = sibling_index;
	
	// Extract source text
	if (start_byte < end_byte && end_byte <= content.length()) {
		ast_node.source_text = content.substr(start_byte, end_byte - start_byte);
	}
	
	nodes.push_back(ast_node);
	
	// Process children
	uint32_t child_count = ts_node_child_count(node);
	for (uint32_t child_idx = 0; child_idx < child_count; child_idx++) {
		TSNode child = ts_node_child(node, child_idx);
		ProcessNode(child, content, file_path, nodes, ast_node.node_id, depth + 1, child_idx);
	}
}

int64_t TreeSitterParser::GenerateNodeId(const string &file_path, uint32_t start_byte, uint32_t end_byte) {
	// Simple hash-based ID generation
	// Combine file path hash with position
	hash_t path_hash = Hash(file_path.c_str());
	
	// Combine path hash with position to create unique ID
	// Using a simple combination that should be unique within a file
	return static_cast<int64_t>((path_hash & 0xFFFFFFFF) << 32) | 
	       static_cast<int64_t>((start_byte & 0xFFFF) << 16) | 
	       static_cast<int64_t>(end_byte & 0xFFFF);
}

} // namespace duckdb