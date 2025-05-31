#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/file_system.hpp"

namespace duckdb {

struct ReadASTObjectsHybridData : public TableFunctionData {
    vector<string> files;
    string language;
    idx_t current_file_idx = 0;
    
    ReadASTObjectsHybridData(vector<string> files, string language)
        : files(std::move(files)), language(std::move(language)) {}
};

class ReadASTObjectsHybridFunction {
public:
    static TableFunction GetFunctionTwoArgs();
    static TableFunction GetFunctionOneArg();
    
private:
    static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
                                       vector<LogicalType> &return_types, vector<string> &names);
    static unique_ptr<FunctionData> BindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names);
    static void Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);
    
    static Value ParseFileToStructs(ClientContext &context, const string &file_path, const string &language, LogicalType &nodes_type);
};

} // namespace duckdb