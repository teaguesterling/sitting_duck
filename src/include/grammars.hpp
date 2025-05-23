#pragma once

#include "duckdb/common/vector.hpp"

extern "C" {
    typedef struct TSLanguage TSLanguage;
}

namespace duckdb {

//! Get the tree-sitter language for a given language name
const TSLanguage* GetLanguage(const string &language);

//! Get list of supported languages
vector<string> GetSupportedLanguages();

} // namespace duckdb