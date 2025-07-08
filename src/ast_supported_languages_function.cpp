#include "duckdb.hpp"
#include "language_adapter.hpp"

namespace duckdb {

struct SupportedLanguagesData : public GlobalTableFunctionState {
    SupportedLanguagesData() : offset(0) {}
    idx_t offset;
};

static unique_ptr<FunctionData> SupportedLanguagesBind(ClientContext &context, TableFunctionBindInput &input,
                                                      vector<LogicalType> &return_types, vector<string> &names) {
    names.emplace_back("language");
    return_types.emplace_back(LogicalType::VARCHAR);
    return nullptr;
}

static unique_ptr<GlobalTableFunctionState> SupportedLanguagesInit(ClientContext &context, TableFunctionInitInput &input) {
    return make_uniq<SupportedLanguagesData>();
}

static void SupportedLanguagesFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &data = data_p.global_state->Cast<SupportedLanguagesData>();
    auto &registry = LanguageAdapterRegistry::GetInstance();
    auto languages = registry.GetSupportedLanguages();
    
    idx_t count = 0;
    for (idx_t i = data.offset; i < languages.size() && count < STANDARD_VECTOR_SIZE; i++) {
        output.SetValue(0, count, Value(languages[i]));
        count++;
        data.offset++;
    }
    output.SetCardinality(count);
}

void RegisterASTSupportedLanguagesFunction(ExtensionLoader &loader) {
    TableFunction function("ast_supported_languages", {}, SupportedLanguagesFunction, SupportedLanguagesBind, SupportedLanguagesInit);
    loader.RegisterFunction(function);
}

} // namespace duckdb