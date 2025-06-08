#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/file_system.hpp"
#include <unordered_set>

namespace duckdb {

// Helper function to detect language from file extension
string DetectLanguageFromExtension(const string &file_path);

struct FilterConfig {
    std::unordered_set<string> exclude_types;
    std::unordered_set<string> include_types;
    bool has_include_filter = false;  // Track if include_types was specified
    
    FilterConfig() = default;
    FilterConfig(const vector<string> &exclude, const vector<string> &include) {
        for (const auto &type : exclude) {
            exclude_types.insert(type);
        }
        if (!include.empty()) {
            has_include_filter = true;
            for (const auto &type : include) {
                include_types.insert(type);
            }
        }
    }
    
    bool ShouldIncludeNode(const string &node_type) const {
        // If include filter is specified, only include nodes in that set
        if (has_include_filter) {
            return include_types.count(node_type) > 0;
        }
        // Otherwise, exclude nodes in the exclude set
        return exclude_types.count(node_type) == 0;
    }
};

struct ReadASTObjectsHybridData : public TableFunctionData {
    vector<string> files;
    string language;
    FilterConfig filter_config;
    idx_t current_file_idx = 0;
    
    ReadASTObjectsHybridData(vector<string> files, string language, FilterConfig filter_config)
        : files(std::move(files)), language(std::move(language)), filter_config(std::move(filter_config)) {}
};

class ReadASTObjectsHybridFunction {
public:
    static TableFunction GetFunctionOneArg();        // 1 arg + named params
    static TableFunction GetFunctionWithFilters();   // 2 args + named params
    
private:
    static unique_ptr<FunctionData> BindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names);
    static unique_ptr<FunctionData> BindWithFilters(ClientContext &context, TableFunctionBindInput &input,
                                                   vector<LogicalType> &return_types, vector<string> &names);
    static void Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);
    
    static Value ParseFileToStructs(ClientContext &context, const string &file_path, const string &language, 
                                   LogicalType &nodes_type, const FilterConfig &filter_config);
};


} // namespace duckdb