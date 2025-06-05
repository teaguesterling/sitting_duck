#include "parse_ast_function.hpp"
#include "unified_ast_backend.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"

namespace duckdb {

struct ParseASTData : public TableFunctionData {
    string code;
    string language;
    ASTResult result;
    idx_t current_row = 0;  // Track which row we're on
    bool parsed = false;
    
    ParseASTData(string code, string language) 
        : code(std::move(code)), language(std::move(language)) {}
};

static unique_ptr<FunctionData> ParseASTBind(ClientContext &context, TableFunctionBindInput &input,
                                           vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("parse_ast requires exactly 2 arguments: code and language");
    }
    
    auto code = input.inputs[0].GetValue<string>();
    auto language = input.inputs[1].GetValue<string>();
    
    // Use unified backend schema
    return_types = UnifiedASTBackend::GetFlatTableSchema();
    names = UnifiedASTBackend::GetFlatTableColumnNames();
    
    return make_uniq<ParseASTData>(code, language);
}

static void ParseASTExecute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    printf("DEBUG: ParseASTExecute called\n");
    auto &data = data_p.bind_data->CastNoConst<ParseASTData>();
    printf("DEBUG: Got bind data, code='%s', language='%s'\n", data.code.c_str(), data.language.c_str());
    
    // Parse the code if not already done
    if (!data.parsed) {
        printf("DEBUG: Starting parse...\n");
        try {
            // Use unified parsing backend
            data.result = UnifiedASTBackend::ParseToASTResult(data.code, data.language, "<inline>");
            data.parsed = true;
        } catch (const Exception &e) {
            throw IOException("Failed to parse code: " + string(e.what()));
        }
    }
    
    // Project to table format, starting from where we left off
    idx_t output_index = 0;
    UnifiedASTBackend::ProjectToTable(data.result, output, data.current_row, output_index);
    output.SetCardinality(output_index);
}

void ParseASTFunction::Register(DatabaseInstance &instance) {
    // Register parse_ast(code, language) -> TABLE with all taxonomy fields
    TableFunction parse_ast_func("parse_ast", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                                ParseASTExecute, ParseASTBind);
    parse_ast_func.name = "parse_ast";
    
    ExtensionUtil::RegisterFunction(instance, parse_ast_func);
}

} // namespace duckdb