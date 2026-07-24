#include "language_adapter.hpp"
#include "duckdb_adapter.hpp"
#include "unified_ast_backend.hpp"
#include "unified_ast_backend_impl.hpp"
#include "duckdb/common/helper.hpp"

// Built-in language registration and parse dispatch.
//
// The set of built-in languages is a compile-time decision: CMake generates
// sitting_duck_builtin_languages.def (from cmake/BuiltinLanguages.cmake, driven
// by -DSITTING_DUCK_LANGUAGES / -DSITTING_DUCK_EXCLUDE_LANGUAGES) listing one
// SD_BUILTIN_LANGUAGE_TS/SD_BUILTIN_LANGUAGE_NATIVE entry per enabled language,
// in canonical registration order. Everything language-specific in this file is
// expanded from that table, so a compiled-out language contributes no code here
// and its adapter translation unit never links. A compiled-out language behaves
// exactly like an unknown one (and its name becomes free for runtime
// registration once that lands).

namespace duckdb {

void LanguageAdapterRegistry::InitializeDefaultAdapters() {
	// Register factories instead of creating adapters immediately
#define SD_BUILTIN_LANGUAGE_TS(name, ADAPTER_CLASS)                                                                    \
	RegisterLanguageFactory(#name, []() { return make_uniq<ADAPTER_CLASS>(); });
#define SD_BUILTIN_LANGUAGE_NATIVE(name, ADAPTER_CLASS)                                                                \
	RegisterLanguageFactory(#name, []() { return make_uniq<ADAPTER_CLASS>(); });
#include "sitting_duck_builtin_languages.def"
#undef SD_BUILTIN_LANGUAGE_TS
#undef SD_BUILTIN_LANGUAGE_NATIVE
}

shared_ptr<const DynamicLanguageInfo> LanguageAdapterRegistry::ResolveDynamicLanguage(const string &language,
                                                                                      string &normalized) const {
	normalized = language;
	lock_guard<mutex> lock(registry_mutex_);
	auto alias_it = alias_to_language.find(language);
	if (alias_it != alias_to_language.end()) {
		normalized = alias_it->second;
	}
	auto dynamic_it = dynamic_languages.find(normalized);
	if (dynamic_it != dynamic_languages.end()) {
		return dynamic_it->second;
	}
	return nullptr;
}

[[noreturn]] static void ThrowUnsupportedLanguage(const string &language) {
	throw InvalidInputException("Unsupported language: " + language +
	                            ". Use register_language() to add a custom tree-sitter grammar.");
}

// Phase 2: Template-based parsing with zero virtual calls - NEW ExtractionConfig version
ASTResult LanguageAdapterRegistry::ParseContentTemplated(const string &content, const string &language,
                                                         const string &file_path,
                                                         const ExtractionConfig &config) const {
	string normalized_language;
	auto dynamic_info = ResolveDynamicLanguage(language, normalized_language);
	if (dynamic_info) {
		DynamicLanguageAdapter adapter(std::move(dynamic_info));
		return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
	}

	// CRITICAL FIX: Use fresh adapter instances to prevent static state accumulation
	// Static adapters were causing cumulative memory leaks leading to segfaults after 3+ large queries
#define SD_BUILTIN_LANGUAGE_TS(name, ADAPTER_CLASS)                                                                    \
	if (normalized_language == #name) {                                                                                \
		ADAPTER_CLASS adapter; /* Fresh instance - no static state persistence */                                      \
		return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);           \
	}
	// Native (non-tree-sitter) parsers, i.e. the DuckDB adapter.
	// TODO: DuckDB adapter should also support ExtractionConfig
#define SD_BUILTIN_LANGUAGE_NATIVE(name, ADAPTER_CLASS)                                                                \
	if (normalized_language == #name) {                                                                                \
		ADAPTER_CLASS adapter; /* Fresh instance - no static state persistence */                                      \
		return adapter.ParseSQL(content);                                                                              \
	}
#include "sitting_duck_builtin_languages.def"
#undef SD_BUILTIN_LANGUAGE_TS
#undef SD_BUILTIN_LANGUAGE_NATIVE

	// Fallback for unsupported languages
	ThrowUnsupportedLanguage(language);
}

// Legacy version for backward compatibility
ASTResult LanguageAdapterRegistry::ParseContentTemplated(const string &content, const string &language,
                                                         const string &file_path, int32_t peek_size,
                                                         const string &peek_mode) const {
	string normalized_language;
	auto dynamic_info = ResolveDynamicLanguage(language, normalized_language);
	if (dynamic_info) {
		DynamicLanguageAdapter adapter(std::move(dynamic_info));
		return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	}

	// CRITICAL FIX: Use fresh adapter instances to prevent static state accumulation (legacy version)
#define SD_BUILTIN_LANGUAGE_TS(name, ADAPTER_CLASS)                                                                    \
	if (normalized_language == #name) {                                                                                \
		ADAPTER_CLASS adapter; /* Fresh instance - no static state persistence */                                      \
		return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size,         \
		                                                    peek_mode);                                                \
	}
	// DuckDB adapter uses native parser, not tree-sitter template system
#define SD_BUILTIN_LANGUAGE_NATIVE(name, ADAPTER_CLASS)                                                                \
	if (normalized_language == #name) {                                                                                \
		ADAPTER_CLASS adapter; /* Fresh instance - no static state persistence */                                      \
		return adapter.ParseSQL(content);                                                                              \
	}
#include "sitting_duck_builtin_languages.def"
#undef SD_BUILTIN_LANGUAGE_TS
#undef SD_BUILTIN_LANGUAGE_NATIVE

	// Fallback for unsupported languages
	ThrowUnsupportedLanguage(language);
}

} // namespace duckdb
