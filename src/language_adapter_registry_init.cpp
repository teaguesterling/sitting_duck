#include "language_adapter.hpp"
#include "duckdb_adapter.hpp"
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
}

} // namespace duckdb