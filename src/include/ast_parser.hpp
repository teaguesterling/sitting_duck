#pragma once

#include "duckdb.hpp"
#include "duckdb/common/vector.hpp"
#include "language_handler.hpp"
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
	TSParser *CreateParser(const string &language);

	//! Parse a string and return the tree
	TSTree *ParseString(const string &content, TSParser *parser);

	//! Get language handler for a language
	const LanguageHandler *GetLanguageHandler(const string &language);
};

} // namespace duckdb
