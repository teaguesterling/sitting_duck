#pragma once

#include "duckdb.hpp"
#include "duckdb/parallel/task_executor.hpp"
#include "unified_ast_backend.hpp"
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>

namespace duckdb {

// Forward declaration
class LanguageAdapter;

// Shared state for parallel AST parsing tasks
struct ASTParsingState {
	ASTParsingState(ClientContext &context_p, const vector<string> &file_paths_p, const vector<string> &languages_p,
	                bool ignore_errors_p, int32_t peek_size_p, const string &peek_mode_p,
	                const unordered_map<string, unique_ptr<LanguageAdapter>> &adapters_p, idx_t num_threads_p = 1)
	    : context(context_p), file_paths(file_paths_p), languages(languages_p), ignore_errors(ignore_errors_p),
	      peek_size(peek_size_p), peek_mode(peek_mode_p), pre_created_adapters(adapters_p), files_processed(0),
	      total_nodes(0), errors_encountered(0) {
		// Initialize per-thread result buffers
		per_thread_results.resize(num_threads_p);
	}

	ClientContext &context;
	const vector<string> &file_paths;
	const vector<string> &languages;
	const bool ignore_errors;
	const int32_t peek_size;
	const string peek_mode;

	// Pre-created adapters (no singleton lookup needed)
	const unordered_map<string, unique_ptr<LanguageAdapter>> &pre_created_adapters;

	// Thread-safe result collection
	mutex results_mutex;
	vector<ASTResult> results;

	// Per-thread result buffers (eliminates mutex contention during parsing)
	vector<vector<ASTResult>> per_thread_results;

	// Error collection (when ignore_errors=true)
	mutex errors_mutex;
	vector<string> error_messages;

	// Progress tracking (atomic for thread safety)
	atomic<idx_t> files_processed;
	atomic<idx_t> total_nodes;
	atomic<idx_t> errors_encountered;

	// Collect all per-thread results into main results vector
	void CollectResults() {
		for (auto &thread_results : per_thread_results) {
			for (auto &result : thread_results) {
				results.push_back(std::move(result));
			}
			thread_results.clear();
		}
	}
};

// Task for parallel AST parsing
class ASTParsingTask : public BaseExecutorTask {
public:
	ASTParsingTask(TaskExecutor &executor, ASTParsingState &parsing_state, const idx_t file_idx_start,
	               const idx_t file_idx_end, const idx_t thread_id);

	void ExecuteTask() override;
	string TaskType() const override {
		return "ASTParsingTask";
	}

private:
	ASTParsingState &parsing_state;
	const idx_t file_idx_start;
	const idx_t file_idx_end;
	const idx_t thread_id;

	// Helper method to process a single file
	void ProcessSingleFile(idx_t file_idx);
};

} // namespace duckdb
