#include "grammars.hpp"
#include "language_handler.hpp"

// Language function declarations
extern "C" {
	TSLanguage *tree_sitter_python();
	TSLanguage *tree_sitter_javascript();
	TSLanguage *tree_sitter_cpp();
	// Temporarily disabled due to ABI compatibility issues:
	// TSLanguage *tree_sitter_rust();
}

namespace duckdb {

const TSLanguage* GetLanguage(const string &language) {
	if (language == "python" || language == "py") {
		return tree_sitter_python();
	} else if (language == "javascript" || language == "js") {
		return tree_sitter_javascript();
	} else if (language == "cpp" || language == "c++") {
		return tree_sitter_cpp();
	}
	// Temporarily disabled due to ABI compatibility issues:
	// else if (language == "rust" || language == "rs") {
	//	return tree_sitter_rust();
	// }
	// Add more languages here as we add them
	return nullptr;
}

vector<string> GetSupportedLanguages() {
	// Use the language handler registry instead of hard-coding
	auto& registry = LanguageHandlerRegistry::GetInstance();
	return registry.GetSupportedLanguages();
}

} // namespace duckdb