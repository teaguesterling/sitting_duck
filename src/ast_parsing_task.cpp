#include "ast_parsing_task.hpp"
#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/file_system.hpp"

namespace duckdb {

ASTParsingTask::ASTParsingTask(TaskExecutor &executor, 
                               ASTParsingState &parsing_state,
                               const idx_t file_idx_start,
                               const idx_t file_idx_end,
                               const idx_t thread_id)
    : BaseExecutorTask(executor), parsing_state(parsing_state),
      file_idx_start(file_idx_start), file_idx_end(file_idx_end), thread_id(thread_id) {
}

void ASTParsingTask::ExecuteTask() {
    // Process each file assigned to this task
    for (idx_t file_idx = file_idx_start; file_idx < file_idx_end; file_idx++) {
        ProcessSingleFile(file_idx);
    }
}

void ASTParsingTask::ProcessSingleFile(idx_t file_idx) {
    try {
        const auto& file_path = parsing_state.file_paths[file_idx];
        
        // Read file content using DuckDB's thread-safe file system
        auto &fs = FileSystem::GetFileSystem(parsing_state.context);
        auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
        auto file_size = fs.GetFileSize(*handle);
        
        string content;
        content.resize(file_size);
        fs.Read(*handle, (void*)content.data(), file_size);
        
        // Get language for this specific file
        const auto& file_language = parsing_state.languages[file_idx];
        
        // Skip files with unknown/unsupported languages
        if (file_language == "unknown" || file_language.empty()) {
            if (parsing_state.ignore_errors) {
                parsing_state.files_processed.fetch_add(1);
                return;
            } else {
                throw InvalidInputException("Unknown language for file: " + file_path);
            }
        }
        
        // MEMORY SAFETY FIX: Create fresh adapter for each file to prevent state accumulation
        // This eliminates the segfault caused by persistent adapter state across multiple files
        auto& registry = LanguageAdapterRegistry::GetInstance();
        auto fresh_adapter = registry.CreateAdapter(file_language);
        const LanguageAdapter* adapter = fresh_adapter.get();
        
        if (!adapter) {
            throw InvalidInputException("Unsupported language: " + file_language);
        }
        
        // CRITICAL: Use adapter's parsing function which creates fresh parsers
        // This ensures thread safety by avoiding shared parser state
        ParsingFunction parsing_fn = adapter->GetParsingFunction();
        
        // Call the parsing function with the adapter as context
        ASTResult result = parsing_fn(adapter, content, file_language, file_path, 
                                     parsing_state.peek_size, parsing_state.peek_mode);
        
        // Update progress atomically (before moving the result)
        const idx_t node_count = result.nodes.size();
        parsing_state.files_processed.fetch_add(1);
        parsing_state.total_nodes.fetch_add(node_count);
        
        // Store result in per-thread buffer (no mutex needed!)
        parsing_state.per_thread_results[thread_id].push_back(std::move(result));
        
    } catch (const Exception &e) {
        // Handle errors based on ignore_errors flag
        parsing_state.errors_encountered.fetch_add(1);
        
        if (parsing_state.ignore_errors) {
            // Store error message for later reporting
            {
                std::lock_guard<std::mutex> lock(parsing_state.errors_mutex);
                parsing_state.error_messages.push_back(
                    "Error processing file " + parsing_state.file_paths[file_idx] + ": " + string(e.what())
                );
            }
            
            // Continue processing other files
            parsing_state.files_processed.fetch_add(1);
        } else {
            // Re-throw to stop all tasks
            throw;
        }
    }
}

} // namespace duckdb