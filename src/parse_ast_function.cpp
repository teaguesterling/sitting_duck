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
    auto &data = data_p.bind_data->CastNoConst<ParseASTData>();
    
    // Parse the code if not already done
    if (!data.parsed) {
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

//==============================================================================
// NEW: Hierarchical Schema Functions
//==============================================================================

static unique_ptr<FunctionData> ParseASTHierarchicalBind(ClientContext &context, TableFunctionBindInput &input,
                                                        vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("parse_ast requires exactly 2 arguments: code and language");
    }
    
    auto code = input.inputs[0].GetValue<string>();
    auto language = input.inputs[1].GetValue<string>();
    
    // Use hierarchical STRUCT schema
    return_types = UnifiedASTBackend::GetHierarchicalTableSchema();
    names = UnifiedASTBackend::GetHierarchicalTableColumnNames();
    
    return make_uniq<ParseASTData>(code, language);
}

static void ParseASTHierarchicalExecute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &data = data_p.bind_data->CastNoConst<ParseASTData>();
    
    // Parse the code if not already done
    if (!data.parsed) {
        try {
            // Use unified parsing backend
            data.result = UnifiedASTBackend::ParseToASTResult(data.code, data.language, "<inline>");
            data.parsed = true;
        } catch (const Exception &e) {
            throw IOException("Failed to parse code: " + string(e.what()));
        }
    }
    
    // Project to hierarchical STRUCT table format using streaming projection
    idx_t output_index = 0;
    idx_t old_output_index = output_index;
    UnifiedASTBackend::ProjectToHierarchicalTableStreaming(data.result.nodes, output, data.current_row, output_index, data.result.source);
    
    // Update current_row based on how many rows were processed
    idx_t rows_processed = output_index - old_output_index;
    data.current_row += rows_processed;
    
    output.SetCardinality(output_index);
}

void ParseASTFunction::Register(DatabaseInstance &instance) {
    // Register parse_ast_flat(code, language) -> TABLE with flat schema (legacy)
    TableFunction parse_ast_flat_func("parse_ast_flat", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                                     ParseASTExecute, ParseASTBind);
    parse_ast_flat_func.name = "parse_ast_flat";
    ExtensionUtil::RegisterFunction(instance, parse_ast_flat_func);
    
    // Register parse_ast(code, language) -> TABLE with flat schema (temporarily)
    TableFunction parse_ast_func("parse_ast", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                                ParseASTHierarchicalExecute, ParseASTHierarchicalBind);
    parse_ast_func.name = "parse_ast";
    ExtensionUtil::RegisterFunction(instance, parse_ast_func);
}

} // namespace duckdb