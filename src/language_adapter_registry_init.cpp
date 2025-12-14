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
    // C# enabled - .NET ecosystem support
    RegisterLanguageFactory("csharp", []() { return make_uniq<CSharpAdapter>(); });
    // Lua enabled - game scripting, embedded systems, Neovim plugins
    RegisterLanguageFactory("lua", []() { return make_uniq<LuaAdapter>(); });
    // HCL enabled - Terraform, Vault, Nomad, Waypoint configuration
    RegisterLanguageFactory("hcl", []() { return make_uniq<HCLAdapter>(); });
    // GraphQL enabled - schema and query language
    RegisterLanguageFactory("graphql", []() { return make_uniq<GraphQLAdapter>(); });
    // TOML enabled - configuration file format (Cargo.toml, pyproject.toml, etc.)
    RegisterLanguageFactory("toml", []() { return make_uniq<TOMLAdapter>(); });
    // Zig enabled - modern systems programming language
    RegisterLanguageFactory("zig", []() { return make_uniq<ZigAdapter>(); });
    // Dart enabled - client-optimized language with sound null safety
    RegisterLanguageFactory("dart", []() { return make_uniq<DartAdapter>(); });
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
    
    // CRITICAL FIX: Use fresh adapter instances to prevent static state accumulation
    // Static adapters were causing cumulative memory leaks leading to segfaults after 3+ large queries
    if (normalized_language == "python") {
        PythonAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "javascript") {
        JavaScriptAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "cpp") {
        CPPAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "typescript") {
        TypeScriptAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "sql") {
        SQLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "duckdb") {
        DuckDBAdapter adapter;  // Fresh instance - no static state persistence
        // TODO: DuckDB adapter should also support ExtractionConfig
        return adapter.ParseSQL(content);
    }
    if (normalized_language == "go") {
        GoAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "ruby") {
        RubyAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "markdown") {
        MarkdownAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "java") {
        JavaAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "php") {
        PHPAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "html") {
        HTMLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "css") {
        CSSAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "c") {
        CAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "rust") {
        RustAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "json") {
        JSONAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "bash") {
        BashAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "swift") {
        SwiftAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "r") {
        RAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "kotlin") {
        KotlinAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "csharp") {
        CSharpAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "lua") {
        LuaAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "hcl") {
        HCLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "graphql") {
        GraphQLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "toml") {
        TOMLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "zig") {
        ZigAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, config);
    }
    if (normalized_language == "dart") {
        DartAdapter adapter;  // Fresh instance - no static state persistence
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
    
    // CRITICAL FIX: Use fresh adapter instances to prevent static state accumulation (legacy version)
    if (normalized_language == "python") {
        PythonAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "javascript") {
        JavaScriptAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "cpp") {
        CPPAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "typescript") {
        TypeScriptAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "sql") {
        SQLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "duckdb") {
        DuckDBAdapter adapter;  // Fresh instance - no static state persistence
        // DuckDB adapter uses native parser, not tree-sitter template system
        return adapter.ParseSQL(content);
    }
    if (normalized_language == "go") {
        GoAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "ruby") {
        RubyAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "markdown") {
        MarkdownAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "java") {
        JavaAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "php") {
        PHPAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "html") {
        HTMLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "css") {
        CSSAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "c") {
        CAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "rust") {
        RustAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "json") {
        JSONAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "bash") {
        BashAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "swift") {
        SwiftAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "r") {
        RAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "kotlin") {
        KotlinAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "csharp") {
        CSharpAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "lua") {
        LuaAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "hcl") {
        HCLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "graphql") {
        GraphQLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "toml") {
        TOMLAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "zig") {
        ZigAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }
    if (normalized_language == "dart") {
        DartAdapter adapter;  // Fresh instance - no static state persistence
        return UnifiedASTBackend::ParseToASTResultTemplated(&adapter, content, language, file_path, peek_size, peek_mode);
    }

    // Fallback for unsupported languages
    throw InvalidInputException("Unsupported language: " + language);
}

} // namespace duckdb