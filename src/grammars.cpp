#include "grammars.hpp"

// Language function declarations
extern "C" {
	TSLanguage *tree_sitter_python();
}

namespace duckdb {

const TSLanguage* GetLanguage(const string &language) {
	if (language == "python") {
		return tree_sitter_python();
	}
	// Add more languages here as we add them
	return nullptr;
}

vector<string> GetSupportedLanguages() {
	return {"python"};
}

} // namespace duckdb