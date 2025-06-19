#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/multi_file/multi_file_reader.hpp"
#include "unified_ast_backend.hpp"
#include <unordered_map>

namespace duckdb {

// Forward declaration
class LanguageAdapter;

//! Global state for streaming AST table function with parallel batch processing
struct ReadASTStreamingGlobalState : public GlobalTableFunctionState {
    // Traditional single-threaded streaming (for small file sets)
    shared_ptr<MultiFileList> file_list;
    MultiFileListScanData file_scan_state;
    
    // Current file processing state (used by both modes)
    unique_ptr<ASTResult> current_file_result;
    idx_t current_file_row_index = 0;
    bool current_file_parsed = false;
    bool files_exhausted = false;
    
    // PARALLEL PROCESSING (no batching - let DuckDB handle it!)
    bool use_parallel_batching = false;      // Reuse flag name for compatibility
    vector<string> all_file_paths;           // All files to process
    vector<string> resolved_languages;       // Pre-resolved languages for all files
    bool parallel_processing_complete = false; // True when all parsing is done
    
    // Result management (all results from parallel processing)
    vector<ASTResult> current_batch_results; // All results from parallel processing
    idx_t current_batch_result_index = 0;    // Index within results
    idx_t current_batch_row_index = 0;       // Row index within current result
    
    // Configuration
    string language;
    bool ignore_errors;
    int32_t peek_size;
    string peek_mode;
    
    // Pre-created language adapters (eliminates singleton contention)
    unordered_map<string, unique_ptr<LanguageAdapter>> pre_created_adapters;
    
    ReadASTStreamingGlobalState() = default;
};

//! Bind data for streaming AST table function
struct ReadASTStreamingBindData : public TableFunctionData {
    Value file_path_value;          // For single pattern or legacy compatibility  
    vector<string> file_patterns;   // For array patterns (DuckDB-consistent)
    bool use_patterns_vector;       // Flag to indicate which field to use
    string language;
    bool ignore_errors;
    int32_t peek_size;
    string peek_mode;
    
    // Constructor for Value-based input (legacy)
    ReadASTStreamingBindData(Value file_path_value, string language, bool ignore_errors = false, 
                           int32_t peek_size = 120, string peek_mode = "auto") 
        : file_path_value(std::move(file_path_value)), use_patterns_vector(false),
          language(std::move(language)), ignore_errors(ignore_errors), 
          peek_size(peek_size), peek_mode(std::move(peek_mode)) {}
    
    // Constructor for vector<string> patterns (new DuckDB-consistent approach)
    ReadASTStreamingBindData(vector<string> file_patterns, string language, bool ignore_errors = false,
                           int32_t peek_size = 120, string peek_mode = "auto")
        : file_patterns(std::move(file_patterns)), use_patterns_vector(true),
          language(std::move(language)), ignore_errors(ignore_errors),
          peek_size(peek_size), peek_mode(std::move(peek_mode)) {}
};

} // namespace duckdb