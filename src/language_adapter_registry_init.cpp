#include "language_adapter.hpp"
#include "duckdb/common/helper.hpp"

namespace duckdb {

void LanguageAdapterRegistry::InitializeDefaultAdapters() {
    // Register factories instead of creating adapters immediately
    RegisterLanguageFactory("python", []() { return make_uniq<PythonAdapter>(); });
    RegisterLanguageFactory("javascript", []() { return make_uniq<JavaScriptAdapter>(); });
    RegisterLanguageFactory("cpp", []() { return make_uniq<CPPAdapter>(); });
    RegisterLanguageFactory("typescript", []() { return make_uniq<TypeScriptAdapter>(); });
    RegisterLanguageFactory("sql", []() { return make_uniq<SQLAdapter>(); });
    RegisterLanguageFactory("go", []() { return make_uniq<GoAdapter>(); });
    RegisterLanguageFactory("ruby", []() { return make_uniq<RubyAdapter>(); });
    RegisterLanguageFactory("markdown", []() { return make_uniq<MarkdownAdapter>(); });
    RegisterLanguageFactory("java", []() { return make_uniq<JavaAdapter>(); });
    // PHP disabled due to scanner dependency on tree-sitter internals
    // RegisterLanguageFactory("php", []() { return make_uniq<PHPAdapter>(); });
    RegisterLanguageFactory("html", []() { return make_uniq<HTMLAdapter>(); });
    RegisterLanguageFactory("css", []() { return make_uniq<CSSAdapter>(); });
    RegisterLanguageFactory("c", []() { return make_uniq<CAdapter>(); });
}

} // namespace duckdb