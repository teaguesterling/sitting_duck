#pragma once

#include "duckdb.hpp"
#include "ast_type.hpp"
#include "language_adapter.hpp"
#include "tree_sitter/api.h"
#include "duckdb/main/client_context.hpp"
#include <chrono>
#include <vector>

namespace duckdb {

// Result structure for unified parsing backend
struct ASTSource {
    string file_path;     // Original file path or "<inline>" for string inputs
    string language;      // Language identifier
};

struct ASTResult {
    ASTSource source;
    vector<ASTNode> nodes;
    
    // Metadata
    std::chrono::time_point<std::chrono::system_clock> parse_time;
    uint32_t node_count;
    uint32_t max_depth;
    
    ASTResult() : parse_time(std::chrono::system_clock::now()), node_count(0), max_depth(0) {}
};

// Collection of parse results for multi-file parsing
struct ASTResultCollection {
    vector<ASTResult> results;
    
    // Helper to get total node count across all results
    idx_t GetTotalNodeCount() const {
        idx_t total = 0;
        for (const auto& result : results) {
            total += result.nodes.size();
        }
        return total;
    }
};

// Unified parsing backend - single source of truth for all AST parsing
class UnifiedASTBackend {
public:
    // Core parsing function - used by all AST functions
    static ASTResult ParseToASTResult(const string& content, 
                                     const string& language, 
                                     const string& file_path = "<inline>",
                                     int32_t peek_size = 120,
                                     const string& peek_mode = "auto");
    
    // Multi-file parsing function with glob support
    static ASTResultCollection ParseFilesToASTCollection(ClientContext &context,
                                                         const Value &file_path_value,
                                                         const string& language = "auto",
                                                         bool ignore_errors = false,
                                                         int32_t peek_size = 120,
                                                         const string& peek_mode = "auto");
    
    // Single file parsing for streaming implementation
    static unique_ptr<ASTResult> ParseSingleFileToASTResult(ClientContext &context,
                                                           const string& file_path,
                                                           const string& language = "auto",
                                                           bool ignore_errors = false,
                                                           int32_t peek_size = 120,
                                                           const string& peek_mode = "auto");
    
    // Helper functions for different output formats
    static vector<LogicalType> GetFlatTableSchema();
    static vector<string> GetFlatTableColumnNames();
    static LogicalType GetASTStructSchema();
    
    // Conversion helpers
    static void ProjectToTable(const ASTResult& result, DataChunk& output, idx_t& current_row, idx_t& output_index);
    static Value CreateASTStruct(const ASTResult& result);
    static Value CreateASTStructValue(const ASTResult& result); // For scalar functions
    
private:
    // Internal helpers
    static void PopulateSemanticFields(ASTNode& node, const LanguageAdapter* adapter, TSNode ts_node);
};

} // namespace duckdb