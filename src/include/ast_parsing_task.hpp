#pragma once

#include "duckdb.hpp"
#include "duckdb/parallel/task_executor.hpp"
#include "unified_ast_backend.hpp"
#include <atomic>
#include <mutex>
#include <vector>

namespace duckdb {

// Shared state for parallel AST parsing tasks
struct ASTParsingState {
    ASTParsingState(ClientContext &context_p, 
                   const vector<string> &file_paths_p,
                   const vector<string> &languages_p,
                   bool ignore_errors_p,
                   int32_t peek_size_p,
                   const string &peek_mode_p)
        : context(context_p), file_paths(file_paths_p), languages(languages_p),
          ignore_errors(ignore_errors_p), peek_size(peek_size_p), peek_mode(peek_mode_p),
          files_processed(0), total_nodes(0), errors_encountered(0) {}

    ClientContext &context;
    const vector<string> &file_paths;
    const vector<string> &languages;
    const bool ignore_errors;
    const int32_t peek_size;
    const string peek_mode;
    
    // Thread-safe result collection
    mutex results_mutex;
    vector<ASTResult> results;
    
    // Error collection (when ignore_errors=true)
    mutex errors_mutex;
    vector<string> error_messages;
    
    // Progress tracking (atomic for thread safety)
    atomic<idx_t> files_processed;
    atomic<idx_t> total_nodes;
    atomic<idx_t> errors_encountered;
};

// Task for parallel AST parsing
class ASTParsingTask : public BaseExecutorTask {
public:
    ASTParsingTask(TaskExecutor &executor, 
                   ASTParsingState &parsing_state,
                   const idx_t file_idx_start,
                   const idx_t file_idx_end);

    void ExecuteTask() override;
    string TaskType() const override { return "ASTParsingTask"; }

private:
    ASTParsingState &parsing_state;
    const idx_t file_idx_start;
    const idx_t file_idx_end;
    
    // Helper method to process a single file
    void ProcessSingleFile(idx_t file_idx);
};

} // namespace duckdb