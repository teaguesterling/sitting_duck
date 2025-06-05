#pragma once

#include "duckdb.hpp"
#include "ast_type.hpp"
#include "language_adapter.hpp"
#include "tree_sitter/api.h"
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

// Unified parsing backend - single source of truth for all AST parsing
class UnifiedASTBackend {
public:
    // Core parsing function - used by all AST functions
    static ASTResult ParseToASTResult(const string& content, 
                                     const string& language, 
                                     const string& file_path = "<inline>");
    
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
    static void PopulateBasicFields(ASTNode& node, const LanguageAdapter* adapter, TSNode ts_node);
    static uint64_t GenerateNodeSemanticID(const ASTNode& node);
};

} // namespace duckdb