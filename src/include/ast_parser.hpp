#pragma once

#include "duckdb.hpp"
#include "duckdb/common/vector.hpp"
#include <tree_sitter/api.h>

// Forward declarations for tree-sitter types
extern "C" {
    typedef struct TSParser TSParser;
    typedef struct TSTree TSTree;
    typedef struct TSLanguage TSLanguage;
}

namespace duckdb {

// Forward declaration - defined in ast_type.hpp
struct ASTNode;

class ASTParser {
public:
	ASTParser() = default;
	~ASTParser() = default;

public:
	//! Create a parser for a language
	TSParser* CreateParser(const string &language);
	
	//! Parse a string and return the tree
	TSTree* ParseString(const string &content, TSParser *parser);
	
	//! Extract node name
	string ExtractNodeName(TSNode node, const string &content);
};

} // namespace duckdb