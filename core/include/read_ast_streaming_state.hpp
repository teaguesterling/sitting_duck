#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/file_system.hpp"
#include "unified_ast_backend.hpp"
#include "language_adapter.hpp"
#include <atomic>
#include <unordered_map>

namespace duckdb {

//! Global state for streaming AST table function with true parallel file parsing.
//! DuckDB calls the execute function from multiple threads, each with its own
//! ReadASTLocalState. Threads claim files atomically via next_file_idx.
struct ReadASTStreamingGlobalState : public GlobalTableFunctionState {
	// All expanded file paths to process
	vector<string> all_file_paths;
	// Pre-resolved language for each file (parallel index with all_file_paths)
	vector<string> resolved_languages;
	// Atomic index for lock-free file claiming by worker threads
	atomic<idx_t> next_file_idx {0};

	// Configuration
	string language;
	bool ignore_errors = false;
	ExtractionConfig extraction_config;

	ReadASTStreamingGlobalState() = default;

	idx_t MaxThreads() const override {
		// Parallel overhead isn't worth it for small file sets.
		// Below 4 files, DuckDB's single-threaded path is faster.
		auto n = all_file_paths.size();
		if (n < 4) {
			return 1;
		}
		return n;
	}
};

//! Per-thread local state for parallel file parsing.
//! Each thread parses one file at a time and streams its nodes before claiming the next.
struct ReadASTLocalState : public LocalTableFunctionState {
	// The parsed result for the file this thread is currently streaming
	unique_ptr<ASTResult> current_result;
	// Current row index within current_result->nodes
	idx_t current_row_idx = 0;

	// Per-thread adapter cache: reuse adapters for same-language files
	// (creating fresh adapters per file is expensive — grammar loading, parser init)
	unordered_map<string, unique_ptr<LanguageAdapter>> adapter_cache;

	LanguageAdapter *GetOrCreateAdapter(const string &language) {
		auto it = adapter_cache.find(language);
		if (it != adapter_cache.end()) {
			return it->second.get();
		}
		auto &registry = LanguageAdapterRegistry::GetInstance();
		auto adapter = registry.CreateAdapter(language);
		auto *ptr = adapter.get();
		adapter_cache[language] = std::move(adapter);
		return ptr;
	}

	ReadASTLocalState() = default;
};

//! Bind data for streaming AST table function
struct ReadASTStreamingBindData : public TableFunctionData {
	Value file_path_value;        // For single pattern or legacy compatibility
	vector<string> file_patterns; // For array patterns (DuckDB-consistent)
	bool use_patterns_vector;     // Flag to indicate which field to use
	string language;
	bool ignore_errors;
	int32_t peek_size;
	string peek_mode;
	int32_t batch_size;
	ExtractionConfig extraction_config; // NEW: Full extraction configuration

	// Constructor for Value-based input (legacy)
	ReadASTStreamingBindData(Value file_path_value, string language, bool ignore_errors = false,
	                         int32_t peek_size = 120, string peek_mode = "auto", int32_t batch_size = 1)
	    : file_path_value(std::move(file_path_value)), use_patterns_vector(false), language(std::move(language)),
	      ignore_errors(ignore_errors), peek_size(peek_size), peek_mode(std::move(peek_mode)), batch_size(batch_size) {
		// Initialize extraction_config with legacy parameters for backward compatibility
		extraction_config.context = ContextLevel::NATIVE; // Default to native for backward compatibility
		extraction_config.source = SourceLevel::LINES;
		extraction_config.structure = StructureLevel::FULL;
		extraction_config.peek_size = peek_size;

		// Map legacy peek_mode to PeekLevel
		if (peek_mode == "none") {
			extraction_config.peek = PeekLevel::NONE;
		} else if (peek_mode == "smart") {
			extraction_config.peek = PeekLevel::SMART;
		} else if (peek_mode == "full") {
			extraction_config.peek = PeekLevel::FULL;
		} else if (peek_mode == "auto") {
			extraction_config.peek = PeekLevel::SMART; // Map auto to smart
		} else {
			extraction_config.peek = PeekLevel::SMART; // Default fallback
		}
	}

	// Constructor for vector<string> patterns (new DuckDB-consistent approach)
	ReadASTStreamingBindData(vector<string> file_patterns, string language, bool ignore_errors = false,
	                         int32_t peek_size = 120, string peek_mode = "auto", int32_t batch_size = 1)
	    : file_patterns(std::move(file_patterns)), use_patterns_vector(true), language(std::move(language)),
	      ignore_errors(ignore_errors), peek_size(peek_size), peek_mode(std::move(peek_mode)), batch_size(batch_size) {
		// Initialize extraction_config with legacy parameters for backward compatibility
		extraction_config.context = ContextLevel::NATIVE; // Default to native for backward compatibility
		extraction_config.source = SourceLevel::LINES;
		extraction_config.structure = StructureLevel::FULL;
		extraction_config.peek_size = peek_size;

		// Map legacy peek_mode to PeekLevel
		if (peek_mode == "none") {
			extraction_config.peek = PeekLevel::NONE;
		} else if (peek_mode == "smart") {
			extraction_config.peek = PeekLevel::SMART;
		} else if (peek_mode == "full") {
			extraction_config.peek = PeekLevel::FULL;
		} else if (peek_mode == "auto") {
			extraction_config.peek = PeekLevel::SMART; // Map auto to smart
		} else {
			extraction_config.peek = PeekLevel::SMART; // Default fallback
		}
	}

	// NEW: Constructor with full ExtractionConfig
	ReadASTStreamingBindData(vector<string> file_patterns, string language, bool ignore_errors,
	                         const ExtractionConfig &config, int32_t batch_size = 1)
	    : file_patterns(std::move(file_patterns)), use_patterns_vector(true), language(std::move(language)),
	      ignore_errors(ignore_errors), peek_size(config.peek_size), peek_mode("smart"), batch_size(batch_size),
	      extraction_config(config) {
	}
};

} // namespace duckdb
