#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/multi_file/multi_file_reader.hpp"
#include "unified_ast_backend.hpp"

namespace duckdb {

//! Global state for streaming AST table function
struct ReadASTStreamingGlobalState : public GlobalTableFunctionState {
    // File management using DuckDB's MultiFileReader pattern
    shared_ptr<MultiFileList> file_list;
    MultiFileListScanData file_scan_state;
    
    // Current file processing state
    unique_ptr<ASTResult> current_file_result;
    idx_t current_file_row_index = 0;
    bool current_file_parsed = false;
    bool files_exhausted = false;
    
    // Configuration
    string language;
    bool ignore_errors;
    int32_t peek_size;
    string peek_mode;
    
    ReadASTStreamingGlobalState() = default;
};

//! Bind data for streaming AST table function
struct ReadASTStreamingBindData : public TableFunctionData {
    Value file_path_value;
    string language;
    bool ignore_errors;
    int32_t peek_size;
    string peek_mode;
    
    ReadASTStreamingBindData(Value file_path_value, string language, bool ignore_errors = false, 
                           int32_t peek_size = 120, string peek_mode = "auto") 
        : file_path_value(std::move(file_path_value)), language(std::move(language)), 
          ignore_errors(ignore_errors), peek_size(peek_size), peek_mode(std::move(peek_mode)) {}
};

} // namespace duckdb