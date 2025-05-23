#pragma once

#include "duckdb.hpp"
#include "duckdb/common/vector.hpp"

// Forward declarations for tree-sitter types
extern "C" {
    typedef struct TSParser TSParser;
    typedef struct TSTree TSTree;
    typedef struct TSNode TSNode;
    typedef struct TSLanguage TSLanguage;
}

namespace duckdb {

struct ASTNode {
	int64_t node_id;          // Unique identifier for this node
	string type;              // Node type (e.g., 'function_definition')
	string name;              // Node name if applicable
	string file_path;         // Source file path
	int32_t start_line;       // Starting line number (1-based)
	int32_t start_column;     // Starting column (0-based)
	int32_t end_line;         // Ending line number (1-based)
	int32_t end_column;       // Ending column (0-based)
	int64_t parent_id;        // Parent node ID (NULL for root)
	int32_t depth;            // Depth in tree (0 for root)
	int32_t sibling_index;    // Position among siblings (0-based)
	string source_text;       // Actual source code for this node
};

class TreeSitterParser {
public:
	TreeSitterParser(const string &language);
	~TreeSitterParser();

public:
	//! Parse a file and return the AST
	vector<ASTNode> ParseFile(const string &file_path);
	
	//! Parse a string and return the AST
	vector<ASTNode> ParseString(const string &content, const string &file_path = "");

private:
	//! Convert tree to vector of ASTNodes
	vector<ASTNode> TreeToNodes(TSTree *tree, const string &content, const string &file_path);
	
	//! Recursively process nodes
	void ProcessNode(TSNode node, const string &content, const string &file_path,
	                 vector<ASTNode> &nodes, int64_t parent_id, int32_t depth, int32_t sibling_index);
	
	//! Generate node ID based on file path and position
	int64_t GenerateNodeId(const string &file_path, uint32_t start_byte, uint32_t end_byte);

private:
	TSParser *parser;
	const TSLanguage *language;
};

} // namespace duckdb