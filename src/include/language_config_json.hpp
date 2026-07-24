#pragma once

#include "node_config.hpp"

namespace duckdb {

// Parse a runtime JSON semantic-config document into a map from raw tree-sitter
// node type to NodeConfig. Used to load node-type semantics for grammars that
// are registered at runtime rather than compiled into the extension.
//
// The `language_name` is the name the caller is registering the grammar under.
// If the JSON carries a "language" field it must match this name.
//
// Throws duckdb::InvalidInputException on malformed JSON or any invalid entry,
// with a message specific enough to locate the offending key or node type.
unordered_map<string, NodeConfig> ParseLanguageConfigJSON(const string &json_text, const string &language_name);

} // namespace duckdb
