#include "grammars.hpp"

// Language function declarations
extern "C" {
	TSLanguage *tree_sitter_python();
	TSLanguage *tree_sitter_javascript();
}

namespace duckdb {

const TSLanguage* GetLanguage(const string &language) {
	if (language == "python" || language == "py") {
		return tree_sitter_python();
	} else if (language == "javascript" || language == "js") {
		return tree_sitter_javascript();
	}
	// Add more languages here as we add them
	return nullptr;
}

vector<string> GetSupportedLanguages() {
	return {"python", "javascript"};
}

} // namespace duckdb