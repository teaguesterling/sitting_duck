#pragma once

#include "duckdb/common/helper.hpp"
#include "duckdb/common/exception.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

// Custom deleters for tree-sitter types
struct TSParserDeleter {
	void operator()(TSParser *parser) const {
		if (parser) {
			ts_parser_delete(parser);
		}
	}
};

struct TSTreeDeleter {
	void operator()(TSTree *tree) const {
		if (tree) {
			ts_tree_delete(tree);
		}
	}
};

// Smart pointer type aliases using DuckDB's unique_ptr
using TSParserPtr = unique_ptr<TSParser, TSParserDeleter>;
using TSTreePtr = unique_ptr<TSTree, TSTreeDeleter>;

// RAII wrapper for TSParser with helper methods
class TSParserWrapper {
public:
	TSParserWrapper() : parser_(ts_parser_new()) {
		if (!parser_) {
			throw InternalException("Failed to create tree-sitter parser");
		}
	}

	// Disable copying
	TSParserWrapper(const TSParserWrapper &) = delete;
	TSParserWrapper &operator=(const TSParserWrapper &) = delete;

	// Enable moving
	TSParserWrapper(TSParserWrapper &&) = default;
	TSParserWrapper &operator=(TSParserWrapper &&) = default;

	// Access underlying parser
	TSParser *get() const {
		return parser_.get();
	}
	TSParser *operator->() const {
		return parser_.get();
	}

	// Set language with validation
	void SetLanguage(const TSLanguage *language, const string &language_name) {
		if (!language) {
			throw InvalidInputException("Language is null for: " + language_name);
		}

		// Validate ABI version
		uint32_t language_version = ts_language_version(language);
		if (language_version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION ||
		    language_version > TREE_SITTER_LANGUAGE_VERSION) {
			throw InternalException("Incompatible language version for " + language_name +
			                        ". Expected: " + std::to_string(TREE_SITTER_LANGUAGE_VERSION) +
			                        ", Got: " + std::to_string(language_version));
		}

		if (!ts_parser_set_language(parser_.get(), language)) {
			throw InternalException("Failed to set language: " + language_name);
		}
	}

	// Parse string and return owned tree
	TSTreePtr ParseString(const string &content) {
		TSTree *tree = ts_parser_parse_string(parser_.get(), nullptr, content.c_str(), content.length());

		if (!tree) {
			throw InternalException("Failed to parse content");
		}

		return TSTreePtr(tree);
	}

private:
	TSParserPtr parser_;
};

// RAII wrapper for TSTree with helper methods
class TSTreeWrapper {
public:
	explicit TSTreeWrapper(TSTreePtr tree) : tree_(std::move(tree)) {
		if (!tree_) {
			throw InternalException("Tree is null");
		}
	}

	// Disable copying
	TSTreeWrapper(const TSTreeWrapper &) = delete;
	TSTreeWrapper &operator=(const TSTreeWrapper &) = delete;

	// Enable moving
	TSTreeWrapper(TSTreeWrapper &&) = default;
	TSTreeWrapper &operator=(TSTreeWrapper &&) = default;

	// Access underlying tree
	TSTree *get() const {
		return tree_.get();
	}

	// Get root node
	TSNode RootNode() const {
		return ts_tree_root_node(tree_.get());
	}

private:
	TSTreePtr tree_;
};

} // namespace duckdb
