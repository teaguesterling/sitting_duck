#include "parse_ast_function.hpp"
#include "unified_ast_backend.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/table_function.hpp"

namespace duckdb {

struct ParseASTData : public TableFunctionData {
    string code;
    string language;
    ExtractionConfig extraction_config;
    ASTResult result;
    idx_t current_row = 0;  // Track which row we're on
    bool parsed = false;
    
    ParseASTData(string code, string language, const ExtractionConfig& config) 
        : code(std::move(code)), language(std::move(language)), extraction_config(config) {}
};

static unique_ptr<FunctionData> ParseASTBind(ClientContext &context, TableFunctionBindInput &input,
                                           vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("parse_ast requires exactly 2 arguments: code and language");
    }
    
    auto code = input.inputs[0].GetValue<string>();
    auto language = input.inputs[1].GetValue<string>();
    
    // Parse extraction config parameters (same as read_ast)
    string context_str = "native";  // Default to native for backward compatibility
    string source_str = "lines";
    string structure_str = "full";
    string peek_str = "smart";
    int32_t peek_size = 120;
    
    // Extract named parameters
    for (auto &param : input.named_parameters) {
        if (param.first == "context") {
            context_str = param.second.GetValue<string>();
        } else if (param.first == "source") {
            source_str = param.second.GetValue<string>();
        } else if (param.first == "structure") {
            structure_str = param.second.GetValue<string>();
        } else if (param.first == "peek") {
            if (param.second.type().id() == LogicalTypeId::INTEGER || 
                param.second.type().id() == LogicalTypeId::BIGINT) {
                // INTEGER: custom size
                peek_size = param.second.GetValue<int32_t>();
                peek_str = "custom";
            } else {
                // VARCHAR: named mode
                peek_str = param.second.GetValue<string>();
            }
        }
    }
    
    // Create extraction config
    ExtractionConfig config = ParseExtractionConfig(context_str, source_str, structure_str, peek_str, peek_size);
    
    // Use unified backend schema
    return_types = UnifiedASTBackend::GetFlatTableSchema();
    names = UnifiedASTBackend::GetFlatTableColumnNames();
    
    return make_uniq<ParseASTData>(code, language, config);
}

static void ParseASTExecute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &data = data_p.bind_data->CastNoConst<ParseASTData>();
    
    // Parse the code if not already done
    if (!data.parsed) {
        try {
            // Use unified parsing backend with extraction config
            data.result = UnifiedASTBackend::ParseToASTResult(data.code, data.language, "<inline>", data.extraction_config);
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
    
    // Parse extraction config parameters (same as read_ast)
    string context_str = "native";  // Default to native for backward compatibility
    string source_str = "lines";
    string structure_str = "full";
    string peek_str = "smart";
    int32_t peek_size = 120;
    
    // Extract named parameters
    for (auto &param : input.named_parameters) {
        if (param.first == "context") {
            context_str = param.second.GetValue<string>();
        } else if (param.first == "source") {
            source_str = param.second.GetValue<string>();
        } else if (param.first == "structure") {
            structure_str = param.second.GetValue<string>();
        } else if (param.first == "peek") {
            if (param.second.type().id() == LogicalTypeId::INTEGER || 
                param.second.type().id() == LogicalTypeId::BIGINT) {
                // INTEGER: custom size
                peek_size = param.second.GetValue<int32_t>();
                peek_str = "custom";
            } else {
                // VARCHAR: named mode
                peek_str = param.second.GetValue<string>();
            }
        }
    }
    
    // Create extraction config
    ExtractionConfig config = ParseExtractionConfig(context_str, source_str, structure_str, peek_str, peek_size);
    
    return make_uniq<ParseASTData>(code, language, config);
}

static void ParseASTHierarchicalExecute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &data = data_p.bind_data->CastNoConst<ParseASTData>();
    
    // Parse the code if not already done
    if (!data.parsed) {
        try {
            // Use unified parsing backend with extraction config
            data.result = UnifiedASTBackend::ParseToASTResult(data.code, data.language, "<inline>", data.extraction_config);
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

void ParseASTFunction::Register(ExtensionLoader &loader) {
    // Register parse_ast_flat(code, language) -> TABLE with flat schema (legacy)
    TableFunction parse_ast_flat_func("parse_ast_flat", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                                     ParseASTExecute, ParseASTBind);
    parse_ast_flat_func.name = "parse_ast_flat";
    
    // Add extraction config parameters
    parse_ast_flat_func.named_parameters["context"] = LogicalType::VARCHAR;
    parse_ast_flat_func.named_parameters["source"] = LogicalType::VARCHAR;
    parse_ast_flat_func.named_parameters["structure"] = LogicalType::VARCHAR;
    parse_ast_flat_func.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    
    loader.RegisterFunction(parse_ast_flat_func);
    
    // Register parse_ast(code, language) -> TABLE with hierarchical schema
    TableFunction parse_ast_func("parse_ast", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
                                ParseASTHierarchicalExecute, ParseASTHierarchicalBind);
    parse_ast_func.name = "parse_ast";
    
    // Add extraction config parameters
    parse_ast_func.named_parameters["context"] = LogicalType::VARCHAR;
    parse_ast_func.named_parameters["source"] = LogicalType::VARCHAR;
    parse_ast_func.named_parameters["structure"] = LogicalType::VARCHAR;
    parse_ast_func.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    
    loader.RegisterFunction(parse_ast_func);
}

} // namespace duckdb