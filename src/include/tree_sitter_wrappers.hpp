#pragma once

#include "duckdb/common/helper.hpp"
#include "duckdb/common/exception.hpp"
#include <tree_sitter/api.h>
#include <chrono>
#include <cstdint>
#include <limits>

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

	// Parse string and return owned tree.
	// timeout_ms > 0 bounds the parse: tree-sitter's progress callback is used to
	// cancel parses that exceed the deadline, which then surfaces as a clean
	// DuckDB error instead of an unbounded run. timeout_ms <= 0 disables the bound.
	TSTreePtr ParseString(const string &content, int64_t timeout_ms = -1) {
		// tree-sitter takes the input length as uint32_t; larger inputs would be
		// silently truncated to a partial parse, so reject them explicitly.
		if (content.length() > std::numeric_limits<uint32_t>::max()) {
			throw InvalidInputException("Cannot parse input of " + std::to_string(content.length()) +
			                            " bytes: exceeds tree-sitter's 4 GiB input limit");
		}

		if (timeout_ms <= 0) {
			TSTree *tree =
			    ts_parser_parse_string(parser_.get(), nullptr, content.c_str(), static_cast<uint32_t>(content.length()));
			if (!tree) {
				throw InternalException("Failed to parse content");
			}
			return TSTreePtr(tree);
		}

		// Deadline-bounded parse via ts_parser_parse_with_options (the
		// version-correct mechanism in tree-sitter 0.25+; the old
		// ts_parser_set_timeout_micros API is deprecated).
		struct DeadlineParseState {
			const string *content;
			std::chrono::steady_clock::time_point deadline;
			bool timed_out;
		};
		DeadlineParseState state {&content, std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms),
		                          false};

		TSInput input;
		input.payload = &state;
		input.read = [](void *payload, uint32_t byte_index, TSPoint, uint32_t *bytes_read) -> const char * {
			auto &s = *static_cast<DeadlineParseState *>(payload);
			if (byte_index >= s.content->size()) {
				*bytes_read = 0;
				return "";
			}
			*bytes_read = static_cast<uint32_t>(s.content->size() - byte_index);
			return s.content->data() + byte_index;
		};
		input.encoding = TSInputEncodingUTF8;
		input.decode = nullptr;

		TSParseOptions options;
		options.payload = &state;
		options.progress_callback = [](TSParseState *parse_state) -> bool {
			auto &s = *static_cast<DeadlineParseState *>(parse_state->payload);
			if (std::chrono::steady_clock::now() >= s.deadline) {
				s.timed_out = true;
				return true; // cancel the parse
			}
			return false;
		};

		TSTree *tree = ts_parser_parse_with_options(parser_.get(), nullptr, input, options);
		if (!tree) {
			if (state.timed_out) {
				throw InvalidInputException(
				    "Parse exceeded the timeout of " + std::to_string(timeout_ms) +
				    " ms. Raise the limit with parse_timeout_ms := N or disable it with parse_timeout_ms := 0");
			}
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
