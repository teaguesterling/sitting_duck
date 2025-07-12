#pragma once

#include "duckdb.hpp"
#include "ast_type.hpp"
#include "language_adapter.hpp"
#include "tree_sitter/api.h"
#include "duckdb/main/client_context.hpp"
#include <chrono>
#include <vector>

namespace duckdb {

//==============================================================================
// Extraction Configuration System
//==============================================================================

struct ExtractionConfig {
    ContextLevel context = ContextLevel::NATIVE;  // Default to native for backward compatibility
    SourceLevel source = SourceLevel::LINES; 
    StructureLevel structure = StructureLevel::FULL;
    PeekLevel peek = PeekLevel::SMART;
    int32_t peek_size = 120;  // Used when peek == CUSTOM
    
    // Validation methods
    bool is_valid() const {
        return context <= ContextLevel::NATIVE &&
               source <= SourceLevel::FULL &&
               structure <= StructureLevel::FULL &&
               peek <= PeekLevel::CUSTOM &&
               peek_size >= 0;
    }
    
    // Performance estimation
    string get_performance_tier() const {
        if (context == ContextLevel::NONE && structure == StructureLevel::NONE) {
            return "FASTEST";
        }
        if (context <= ContextLevel::NORMALIZED && structure <= StructureLevel::MINIMAL) {
            return "FAST";
        }
        if (context <= ContextLevel::NATIVE && structure <= StructureLevel::FULL) {
            return "RICH";
        }
        return "MAXIMUM";
    }
};

// Parse extraction config from SQL parameters
ExtractionConfig ParseExtractionConfig(const string& context_str,
                                     const string& source_str, 
                                     const string& structure_str,
                                     const string& peek_str,
                                     int32_t peek_size = 120);

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
                                     const ExtractionConfig& config = ExtractionConfig());
    
    // Legacy parsing function (for backward compatibility)
    static ASTResult ParseToASTResult(const string& content, 
                                     const string& language, 
                                     const string& file_path,
                                     int32_t peek_size,
                                     const string& peek_mode);
    
    // Multi-file parsing function with glob support
    static ASTResultCollection ParseFilesToASTCollection(ClientContext &context,
                                                         const Value &file_path_value,
                                                         const string& language = "auto",
                                                         bool ignore_errors = false,
                                                         int32_t peek_size = 120,
                                                         const string& peek_mode = "auto");
    
    // Parallel multi-file parsing function
    static ASTResultCollection ParseFilesToASTCollectionParallel(ClientContext &context,
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
    
    // Single file parsing with ExtractionConfig
    static unique_ptr<ASTResult> ParseSingleFileToASTResult(ClientContext &context,
                                                           const string& file_path,
                                                           const string& language,
                                                           bool ignore_errors,
                                                           const ExtractionConfig& config);
    
    // Helper functions for different output formats
    static vector<LogicalType> GetFlatTableSchema();
    static vector<string> GetFlatTableColumnNames();
    static LogicalType GetASTStructSchema();
    
    // NEW: Hierarchical schema functions for structured field access
    static vector<LogicalType> GetHierarchicalTableSchema();
    static vector<string> GetHierarchicalTableColumnNames();
    static LogicalType GetHierarchicalStructSchema();
    
    // Dynamic schema functions based on ExtractionConfig parameters
    static vector<LogicalType> GetDynamicTableSchema(const ExtractionConfig& config);
    static vector<string> GetDynamicTableColumnNames(const ExtractionConfig& config);
    
    // Flat dynamic schema functions 
    static vector<LogicalType> GetFlatDynamicTableSchema(const ExtractionConfig& config);
    static vector<string> GetFlatDynamicTableColumnNames(const ExtractionConfig& config);
    
    // Conversion helpers
    static void ProjectToTable(const ASTResult& result, DataChunk& output, idx_t& current_row, idx_t& output_index);
    static void ProjectToDynamicTable(const ASTResult& result, DataChunk& output, idx_t& current_row, idx_t& output_index, const ExtractionConfig& config);
    
    // PHASE 1: Safe minimal projection (node_id, type only) - DIRECT FIELD ACCESS
    static void SafeProjectMinimal(const vector<ASTNode>& nodes, DataChunk& output, 
                                  idx_t& current_row, idx_t& output_index);
    
    static Value CreateASTStruct(const ASTResult& result);
    static Value CreateASTStructValue(const ASTResult& result); // For scalar functions
    
    // NEW: Hierarchical table projection
    static void ProjectToHierarchicalTable(const ASTResult& result, DataChunk& output, idx_t& current_row, idx_t& output_index);
    static void ProjectToHierarchicalTableStreaming(const vector<ASTNode>& nodes, DataChunk& output, 
                                                   idx_t start_row, idx_t& output_index, 
                                                   const ASTSource& source_info);
    static Value CreateHierarchicalASTStruct(const ASTResult& result);
    
    // Templated parsing implementation - avoids virtual calls in hot path
    // Made public so language adapters can call it from their GetParsingFunction() lambdas
    template<typename AdapterType>
    static ASTResult ParseToASTResultTemplated(const AdapterType* adapter,
                                               const string& content, 
                                               const string& language, 
                                               const string& file_path,
                                               const ExtractionConfig& config);
                                               
    // Legacy templated parsing (for backward compatibility)
    template<typename AdapterType>
    static ASTResult ParseToASTResultTemplated(const AdapterType* adapter,
                                               const string& content, 
                                               const string& language, 
                                               const string& file_path,
                                               int32_t peek_size,
                                               const string& peek_mode);
    
private:
    // Internal helpers
    static void PopulateSemanticFields(ASTNode& node, const LanguageAdapter* adapter, TSNode ts_node, const string& content);
    static void ResetStructVectorState(Vector& vector);
};

} // namespace duckdb