#include "grammars.hpp"
#include "language_adapter.hpp"

// Language function declarations
extern "C" {
	TSLanguage *tree_sitter_python();
	TSLanguage *tree_sitter_javascript();
	TSLanguage *tree_sitter_cpp();
	TSLanguage *tree_sitter_typescript();
	TSLanguage *tree_sitter_sql();
	// Temporarily disabled due to ABI compatibility issues:
	// TSLanguage *tree_sitter_rust();
}

namespace duckdb {

const TSLanguage* GetLanguage(const string &language) {
	if (language == "python" || language == "py") {
		return tree_sitter_python();
	} else if (language == "javascript" || language == "js") {
		return tree_sitter_javascript();
	} else if (language == "typescript" || language == "ts") {
		return tree_sitter_typescript();
	} else if (language == "cpp" || language == "c++") {
		return tree_sitter_cpp();
	} else if (language == "sql") {
		return tree_sitter_sql();
	}
	// Temporarily disabled due to ABI compatibility issues:
	// else if (language == "rust" || language == "rs") {
	//	return tree_sitter_rust();
	// }
	// Add more languages here as we add them
	return nullptr;
}

vector<string> GetSupportedLanguages() {
	// Use the language adapter registry instead of hard-coding
	auto& registry = LanguageAdapterRegistry::GetInstance();
	return registry.GetSupportedLanguages();
}

} // namespace duckdb