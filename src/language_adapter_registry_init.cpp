#include "language_adapter.hpp"
#include "duckdb_adapter.hpp"
#include "unified_ast_backend.hpp"
#include "unified_ast_backend_impl.hpp"
#include "duckdb/common/helper.hpp"

namespace duckdb {

void LanguageAdapterRegistry::InitializeDefaultAdapters() {
    // Register factories instead of creating adapters immediately
    RegisterLanguageFactory("python", []() { return make_uniq<PythonAdapter>(); });
    RegisterLanguageFactory("javascript", []() { return make_uniq<JavaScriptAdapter>(); });
    RegisterLanguageFactory("cpp", []() { return make_uniq<CPPAdapter>(); });
    RegisterLanguageFactory("typescript", []() { return make_uniq<TypeScriptAdapter>(); });
    RegisterLanguageFactory("sql", []() { return make_uniq<SQLAdapter>(); });
    // DuckDB native parser - SQL parsing with database-native accuracy
    RegisterLanguageFactory("duckdb", []() { return make_uniq<DuckDBAdapter>(); });
    RegisterLanguageFactory("go", []() { return make_uniq<GoAdapter>(); });
    RegisterLanguageFactory("ruby", []() { return make_uniq<RubyAdapter>(); });
    RegisterLanguageFactory("markdown", []() { return make_uniq<MarkdownAdapter>(); });
    RegisterLanguageFactory("java", []() { return make_uniq<JavaAdapter>(); });
    // PHP enabled - scanner dependency resolved with basic implementation
    RegisterLanguageFactory("php", []() { return make_uniq<PHPAdapter>(); });
    RegisterLanguageFactory("html", []() { return make_uniq<HTMLAdapter>(); });
    RegisterLanguageFactory("css", []() { return make_uniq<CSSAdapter>(); });
    RegisterLanguageFactory("c", []() { return make_uniq<CAdapter>(); });
    // Rust enabled - complete implementation with semantic types
    RegisterLanguageFactory("rust", []() { return make_uniq<RustAdapter>(); });
    // JSON enabled - configuration and data format support
    RegisterLanguageFactory("json", []() { return make_uniq<JSONAdapter>(); });
    // YAML grammar has complex self-modifying structure incompatible with tree-sitter CLI
    // RegisterLanguageFactory("yaml", []() { return make_uniq<YAMLAdapter>(); });
    // Bash enabled - shell scripting support
    RegisterLanguageFactory("bash", []() { return make_uniq<BashAdapter>(); });
    // Swift enabled - iOS/macOS development support
    RegisterLanguageFactory("swift", []() { return make_uniq<SwiftAdapter>(); });
    // R enabled - statistical computing and data analysis support
    RegisterLanguageFactory("r", []() { return make_uniq<RAdapter>(); });
    // Kotlin enabled - JVM ecosystem and Android development support
    RegisterLanguageFactory("kotlin", []() { return make_uniq<KotlinAdapter>(); });
}

// Phase 2: Template-based parsing with zero virtual calls - NEW ExtractionConfig version
ASTResult LanguageAdapterRegistry::ParseContentTemplated(const string& content, const string& language, 
                                                        const string& file_path, const ExtractionConfig& config) const {
    // Normalize language name using alias resolution
    string normalized_language = language;
    auto alias_it = alias_to_language.find(language);
    if (alias_it != alias_to_language.end()) {
        normalized_language = alias_it->second;
    }
    
    // Fast runtime dispatch to compile-time templates - ZERO virtual calls in parsing!
    if (normalized_language == "python") {
        static PythonAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "javascript") {
        static JavaScriptAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "cpp") {
        static CPPAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "typescript") {
        static TypeScriptAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "sql") {
        static SQLAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "duckdb") {
        static DuckDBAdapter adapter;
        // TODO: DuckDB adapter should also support ExtractionConfig
        return adapter.ParseSQL(content);
    }
    if (normalized_language == "go") {
        static GoAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "ruby") {
        static RubyAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "markdown") {
        static MarkdownAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "java") {
        static JavaAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "php") {
        static PHPAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "html") {
        static HTMLAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "css") {
        static CSSAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "c") {
        static CAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "rust") {
        static RustAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "json") {
        static JSONAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "bash") {
        static BashAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "swift") {
        static SwiftAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "r") {
        static RAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "kotlin") {
        static KotlinAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    
    // Fallback for unsupported languages
    throw InvalidInputException("Unsupported language: " + language);
}

// Legacy version for backward compatibility
ASTResult LanguageAdapterRegistry::ParseContentTemplated(const string& content, const string& language, 
                                                        const string& file_path, int32_t peek_size, const string& peek_mode) const {
    // Normalize language name using alias resolution
    string normalized_language = language;
    auto alias_it = alias_to_language.find(language);
    if (alias_it != alias_to_language.end()) {
        normalized_language = alias_it->second;
    }
    
    // Fast runtime dispatch to compile-time templates - ZERO virtual calls in parsing!
    if (normalized_language == "python") {
        static PythonAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "javascript") {
        static JavaScriptAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "cpp") {
        static CPPAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "typescript") {
        static TypeScriptAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "sql") {
        static SQLAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "duckdb") {
        static DuckDBAdapter adapter;
        // DuckDB adapter uses native parser, not tree-sitter template system
        return adapter.ParseSQL(content);
    }
    if (normalized_language == "go") {
        static GoAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "ruby") {
        static RubyAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "markdown") {
        static MarkdownAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "java") {
        static JavaAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "php") {
        static PHPAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "html") {
        static HTMLAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "css") {
        static CSSAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "c") {
        static CAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "rust") {
        static RustAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "json") {
        static JSONAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "bash") {
        static BashAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "swift") {
        static SwiftAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "r") {
        static RAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "kotlin") {
        static KotlinAdapter adapter;
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    
    // Fallback for unsupported languages
    throw InvalidInputException("Unsupported language: " + language);
}

} // namespace duckdb