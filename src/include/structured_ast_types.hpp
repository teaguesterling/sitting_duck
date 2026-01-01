#pragma once

#include "duckdb.hpp"
#include "semantic_types.hpp"
#include <string>

// Forward declarations to avoid circular dependencies
namespace duckdb {
struct ASTNode;
struct ASTResult;
} // namespace duckdb

namespace duckdb {

//==============================================================================
// Structured Extraction Types - Phase 1 Implementation
//
// This implements the organized ASTNode architecture with grouped fields
// for user-controlled extraction at different levels of detail.
//==============================================================================

//==============================================================================
// Extraction Level Enums
//==============================================================================

enum class ContextLevel : uint8_t {
	NONE = 0,        // No semantic analysis (raw tree only)
	NODE_TYPES_ONLY, // + semantic_type, universal_flags, arity_bin
	NORMALIZED,      // + name extraction (requires node_types_only)
	NATIVE           // + language-specific signatures (requires normalized)
};

enum class LocationLevel : uint8_t {
	NONE = 0,   // No source location info
	INPUT_ONLY, // + file_path, language
	LINES,      // + start_line, end_line
	FULL        // + start_column, end_column
};

enum class StructureLevel : uint8_t {
	NONE = 0, // No tree structure info
	MINIMAL,  // + parent_id, depth, sibling_index (O(1) fields)
	FULL      // + children_count, descendant_count (O(child_count) fields)
};

enum class PeekLevel : uint8_t {
	NONE = 0, // No source preview
	SMART,    // Adaptive preview based on node type
	FULL,     // Complete source text for node
	CUSTOM    // Fixed character limit (specified separately)
};

//==============================================================================
// Organized Field Groups
//==============================================================================

struct SourceLocation {
	string file_path;      // Available if location >= INPUT_ONLY
	string language;       // Available if location >= INPUT_ONLY
	uint32_t start_line;   // Available if location >= LINES
	uint32_t end_line;     // Available if location >= LINES
	uint32_t start_column; // Available if location >= FULL
	uint32_t end_column;   // Available if location >= FULL

	// Default constructor
	SourceLocation() : start_line(0), end_line(0), start_column(0), end_column(0) {
	}
};

struct TreeStructure {
	int64_t parent_id;         // Available if structure >= MINIMAL (O(1))
	uint32_t depth;            // Available if structure >= MINIMAL (O(1))
	uint32_t sibling_index;    // Available if structure >= MINIMAL (O(1))
	uint32_t children_count;   // Available if structure >= FULL (O(child_count))
	uint32_t descendant_count; // Available if structure >= FULL (O(child_count))

	// Default constructor
	TreeStructure() : parent_id(-1), depth(0), sibling_index(0), children_count(0), descendant_count(0) {
	}
};

struct NormalizedSemantics {
	uint8_t semantic_type;   // Available if context >= NODE_TYPES_ONLY
	uint8_t universal_flags; // Available if context >= NODE_TYPES_ONLY
	uint8_t arity_bin;       // Available if context >= NODE_TYPES_ONLY

	// Default constructor
	NormalizedSemantics() : semantic_type(0), universal_flags(0), arity_bin(0) {
	}
};

struct NativeContext {
	string signature_type; // Available if context >= NATIVE
	string parameters;     // Available if context >= NATIVE (JSON array)
	string modifiers;      // Available if context >= NATIVE (JSON array)
	string defaults;       // Available if context >= NATIVE
	string qualified_name; // Available if context >= NATIVE (future: static resolution)

	// Default constructor
	NativeContext() = default;
};

struct ContextInfo {
	string name;                    // Available if context >= NORMALIZED
	NormalizedSemantics normalized; // Available if context >= NODE_TYPES_ONLY
	NativeContext native;           // Available if context >= NATIVE

	// Default constructor
	ContextInfo() = default;
};

//==============================================================================
// Reorganized ASTNode Structure
//==============================================================================

struct StructuredASTNode {
	// Core fields (always extracted)
	int64_t node_id; // Unique identifier for this node
	string type_raw; // Raw tree-sitter node type

	// Optional grouped extractions (populated based on extraction levels)
	SourceLocation source;   // Controlled by location parameter
	TreeStructure structure; // Controlled by structure parameter
	ContextInfo context;     // Controlled by context parameter
	string peek;             // Controlled by peek parameter

	// Default constructor
	StructuredASTNode() : node_id(0) {
	}

	// Helper methods for backward compatibility
	uint32_t get_start_line() const {
		return source.start_line;
	}
	uint32_t get_end_line() const {
		return source.end_line;
	}
	uint32_t get_start_column() const {
		return source.start_column;
	}
	uint32_t get_end_column() const {
		return source.end_column;
	}
	int64_t get_parent_id() const {
		return structure.parent_id;
	}
	uint32_t get_depth() const {
		return structure.depth;
	}
	uint32_t get_children_count() const {
		return structure.children_count;
	}
	uint32_t get_descendant_count() const {
		return structure.descendant_count;
	}
	string get_name() const {
		return context.name;
	}
	uint8_t get_semantic_type() const {
		return context.normalized.semantic_type;
	}
	uint8_t get_universal_flags() const {
		return context.normalized.universal_flags;
	}

	// Conversion helpers for transitioning from old structure
	void populate_from_legacy(const ASTNode &legacy_node, ContextLevel context_level, LocationLevel location_level,
	                          StructureLevel structure_level);
};

//==============================================================================
// Structured ASTResult
//==============================================================================

struct StructuredASTResult {
	// Metadata
	SourceLocation source_info; // Global source information
	uint32_t node_count;
	uint32_t max_depth;

	// Node data
	vector<StructuredASTNode> nodes;

	// Extraction configuration used
	ContextLevel context_level;
	LocationLevel location_level;
	StructureLevel structure_level;
	PeekLevel peek_level;
	int32_t peek_size; // For CUSTOM peek level

	// Default constructor
	StructuredASTResult()
	    : node_count(0), max_depth(0), context_level(ContextLevel::NORMALIZED), location_level(LocationLevel::LINES),
	      structure_level(StructureLevel::FULL), peek_level(PeekLevel::SMART), peek_size(120) {
	}
};

//==============================================================================
// Extraction Configuration
//==============================================================================

struct ExtractionConfig {
	ContextLevel context = ContextLevel::NORMALIZED;
	LocationLevel location = LocationLevel::LINES;
	StructureLevel structure = StructureLevel::FULL;
	PeekLevel peek = PeekLevel::SMART;
	int32_t peek_size = 120; // Used when peek == CUSTOM

	// Validation methods
	bool is_valid() const {
		return context <= ContextLevel::NATIVE && location <= LocationLevel::FULL &&
		       structure <= StructureLevel::FULL && peek <= PeekLevel::CUSTOM && peek_size >= 0;
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

//==============================================================================
// Conversion Functions
//==============================================================================

// Convert legacy ASTResult to structured format
StructuredASTResult ConvertToStructuredResult(const ASTResult &legacy_result, const ExtractionConfig &config);

// Convert structured result back to legacy format (for compatibility)
ASTResult ConvertToLegacyResult(const StructuredASTResult &structured_result);

// Parse extraction config from SQL parameters
ExtractionConfig ParseExtractionConfig(const string &context_str, const string &location_str,
                                       const string &structure_str, const string &peek_str);

} // namespace duckdb
