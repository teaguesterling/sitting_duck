#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/file_system.hpp"
#include "ast_type.hpp"

namespace duckdb {

struct ReadASTObjectsData : public TableFunctionData {
    vector<string> files;
    string language;
    idx_t current_file_idx = 0;
    
    ReadASTObjectsData(vector<string> files, string language)
        : files(std::move(files)), language(std::move(language)) {}
};

class ReadASTObjectsFunction {
public:
    static TableFunction GetFunction();
    
private:
    static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
                                       vector<LogicalType> &return_types, vector<string> &names);
    static void Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);
    
    static unique_ptr<ASTType> ParseFile(ClientContext &context, const string &file_path, const string &language);
};

} // namespace duckdb