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

struct SemanticFilter {
	uint8_t mask;
	uint8_t value;
};

struct ExtractionConfig {
	ContextLevel context = ContextLevel::NATIVE; // Default to native for backward compatibility
	SourceLevel source = SourceLevel::LINES;
	StructureLevel structure = StructureLevel::FULL;
	PeekLevel peek = PeekLevel::SMART;
	int32_t peek_size = 120;          // Used when peek == CUSTOM
	bool include_full_schema = false; // When true, schema includes all columns regardless of level settings

	// Parse-time filtering (#014)
	int32_t max_depth = -1;                     // -1 = unlimited
	uint8_t prune_flag_mask = 0;                // prune if (flags & mask) != 0
	vector<SemanticFilter> prune_type_filters;  // prune if (type & f.mask) == f.value
	bool prune_unnamed = false;
	bool prune_leaves = false;
	bool prune_internal = false;
	bool has_prune = false;

	// Get a schema-level config: if include_full_schema, return max levels for schema generation
	ExtractionConfig GetSchemaConfig() const {
		if (!include_full_schema) {
			return *this;
		}
		ExtractionConfig schema_config = *this;
		schema_config.context = ContextLevel::NATIVE;
		schema_config.source = SourceLevel::FULL;
		schema_config.structure = StructureLevel::FULL;
		schema_config.peek = PeekLevel::SMART; // Just needs to be non-NONE
		return schema_config;
	}

	// Validation methods
	bool is_valid() const {
		return context <= ContextLevel::NATIVE && source <= SourceLevel::FULL && structure <= StructureLevel::FULL &&
		       peek <= PeekLevel::CUSTOM && peek_size >= 0;
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
ExtractionConfig ParseExtractionConfig(const string &context_str, const string &source_str, const string &structure_str,
                                       const string &peek_str, int32_t peek_size = 120);
void CompilePrunePolicy(const string &policy_name, ExtractionConfig &config);

// Result structure for unified parsing backend
struct ASTSource {
	string file_path; // Original file path or "<inline>" for string inputs
	string language;  // Language identifier
};

struct ASTResult {
	ASTSource source;
	vector<ASTNode> nodes;

	// Metadata
	std::chrono::time_point<std::chrono::system_clock> parse_time;
	uint32_t node_count;
	uint32_t max_depth;

	ASTResult() : parse_time(std::chrono::system_clock::now()), node_count(0), max_depth(0) {
	}
};

// Collection of parse results for multi-file parsing
struct ASTResultCollection {
	vector<ASTResult> results;

	// Helper to get total node count across all results
	idx_t GetTotalNodeCount() const {
		idx_t total = 0;
		for (const auto &result : results) {
			total += result.nodes.size();
		}
		return total;
	}
};

// Unified parsing backend - single source of truth for all AST parsing
class UnifiedASTBackend {
public:
	// Core parsing function - used by all AST functions
	static ASTResult ParseToASTResult(const string &content, const string &language,
	                                  const string &file_path = "<inline>",
	                                  const ExtractionConfig &config = ExtractionConfig());

	// Legacy parsing function (for backward compatibility)
	static ASTResult ParseToASTResult(const string &content, const string &language, const string &file_path,
	                                  int32_t peek_size, const string &peek_mode);

	// Multi-file parsing function with glob support
	static ASTResultCollection ParseFilesToASTCollection(ClientContext &context, const Value &file_path_value,
	                                                     const string &language = "auto", bool ignore_errors = false,
	                                                     int32_t peek_size = 120, const string &peek_mode = "auto");

	// Parallel multi-file parsing function
	static ASTResultCollection ParseFilesToASTCollectionParallel(ClientContext &context, const Value &file_path_value,
	                                                             const string &language = "auto",
	                                                             bool ignore_errors = false, int32_t peek_size = 120,
	                                                             const string &peek_mode = "auto");

	// Single file parsing for streaming implementation
	static unique_ptr<ASTResult> ParseSingleFileToASTResult(ClientContext &context, const string &file_path,
	                                                        const string &language = "auto", bool ignore_errors = false,
	                                                        int32_t peek_size = 120, const string &peek_mode = "auto");

	// Single file parsing with ExtractionConfig
	static unique_ptr<ASTResult> ParseSingleFileToASTResult(ClientContext &context, const string &file_path,
	                                                        const string &language, bool ignore_errors,
	                                                        const ExtractionConfig &config);

	// Helper functions for different output formats
	static vector<LogicalType> GetFlatTableSchema();
	static vector<string> GetFlatTableColumnNames();
	static LogicalType GetASTStructSchema();

	// Shared helpers for the qualified_name column. The column type is
	// LIST<STRUCT<semantic_type SEMANTIC_TYPE, name VARCHAR, index INTEGER>>,
	// and QualifiedNameValue builds one column value from an ASTNode's segment
	// vector (NULL list when the node has no qualified_name).
	static LogicalType QualifiedNameColumnType();
	static Value QualifiedNameValue(const vector<QualifiedNameSegment> &segments);

	// Shared helpers for the scope column. The column type is
	//   STRUCT<
	//     current BIGINT, function BIGINT, class BIGINT, module BIGINT,
	//     stack LIST<STRUCT<id BIGINT, kind SEMANTIC_TYPE>>
	//   >
	// ScopeValue builds one column value from an ASTNode's scope field.
	static LogicalType ScopeColumnType();
	static Value ScopeValue(const ASTNode::ScopeInfo &scope);

	// NEW: Hierarchical schema functions for structured field access
	static vector<LogicalType> GetHierarchicalTableSchema();
	static vector<string> GetHierarchicalTableColumnNames();
	static LogicalType GetHierarchicalStructSchema();

	// Dynamic schema functions based on ExtractionConfig parameters
	static vector<LogicalType> GetDynamicTableSchema(const ExtractionConfig &config);
	static vector<string> GetDynamicTableColumnNames(const ExtractionConfig &config);

	// Flat dynamic schema functions
	static vector<LogicalType> GetFlatDynamicTableSchema(const ExtractionConfig &config);
	static vector<string> GetFlatDynamicTableColumnNames(const ExtractionConfig &config);

	// Conversion helpers
	static void ProjectToTable(const ASTResult &result, DataChunk &output, idx_t &current_row, idx_t &output_index);
	static void ProjectToDynamicTable(const ASTResult &result, DataChunk &output, idx_t &current_row,
	                                  idx_t &output_index, const ExtractionConfig &config);

	static Value CreateASTStruct(const ASTResult &result);
	static Value CreateASTStructValue(const ASTResult &result); // For scalar functions

	// NEW: Hierarchical table projection
	static void ProjectToHierarchicalTable(const ASTResult &result, DataChunk &output, idx_t &current_row,
	                                       idx_t &output_index);
	static void ProjectToHierarchicalTableStreaming(const vector<ASTNode> &nodes, DataChunk &output, idx_t start_row,
	                                                idx_t &output_index, const ASTSource &source_info);
	static Value CreateHierarchicalASTStruct(const ASTResult &result);

	// Templated parsing implementation - avoids virtual calls in hot path
	// Made public so language adapters can call it from their GetParsingFunction() lambdas
	template <typename AdapterType>
	static ASTResult ParseToASTResultTemplated(const AdapterType *adapter, const string &content,
	                                           const string &language, const string &file_path,
	                                           const ExtractionConfig &config);

	// Legacy templated parsing (for backward compatibility)
	template <typename AdapterType>
	static ASTResult ParseToASTResultTemplated(const AdapterType *adapter, const string &content,
	                                           const string &language, const string &file_path, int32_t peek_size,
	                                           const string &peek_mode);

private:
	// Internal helpers
	static void PopulateSemanticFields(ASTNode &node, const LanguageAdapter *adapter, TSNode ts_node,
	                                   const string &content);
	static void ResetStructVectorState(Vector &vector);
};

} // namespace duckdb
