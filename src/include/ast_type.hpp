#pragma once

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types/value.hpp"
#include "tree_sitter/api.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace duckdb {

// KIND Taxonomy Constants
enum class ASTKind : uint8_t {
    // Data & Structure (00xx)
    LITERAL = 0,      // 0000: Raw constants and primitive values
    NAME = 1,         // 0001: Identifiers and name references
    PATTERN = 2,      // 0010: Structured data patterns and matching
    TYPE = 3,         // 0011: Type expressions and references
    
    // Computation (01xx)
    OPERATOR = 4,     // 0100: Pure computational operations
    COMPUTATION = 5,  // 0101: Complex expressions and invocations
    TRANSFORM = 6,    // 0110: Data transformation and queries
    DEFINITION = 7,   // 0111: Introduction of named entities
    
    // Control & Effects (10xx)
    EXECUTION = 8,    // 1000: Side-effect causing operations
    FLOW_CONTROL = 9, // 1001: Program control flow and branching
    ERROR_HANDLING = 10, // 1010: Exception management
    ORGANIZATION = 11,   // 1011: Structural containers and scope
    
    // Meta & External (11xx)
    METADATA = 12,       // 1100: Annotations and code metadata
    EXTERNAL = 13,       // 1101: Dependencies and external interfaces
    PARSER_SPECIFIC = 14,// 1110: Language-specific constructs
    RESERVED = 15        // 1111: Reserved for future use
};

// Universal Flags
enum class ASTUniversalFlags : uint8_t {
    IS_KEYWORD = 0x01,     // Bare language keywords
    IS_PUNCTUATION = 0x02, // Structural delimiters
    IS_BUILTIN = 0x04,     // Language built-in vs user construct
    IS_PUBLIC = 0x08       // Externally visible
};

// Nested type definitions for ASTNode structure
struct ASTTypeInfo {
    string raw;        // Raw parser type (e.g., "binary_expression")
    string normalized; // Normalized type (e.g., "BinaryExpression")
    string kind;       // KIND name (e.g., "COMPUTATION")
};

struct ASTNameInfo {
    string raw;        // Raw identifier text
    string qualified;  // Fully qualified name (e.g., "MyClass.myMethod")
};

struct ASTFilePosition {
    int64_t start_line = 0;
    int64_t end_line = 0;
    uint16_t start_column = 0;
    uint16_t end_column = 0;
};

struct ASTTreePosition {
    int64_t node_index = 0;     // Position in depth-first traversal
    int64_t parent_index = -1;  // Parent's position (-1 for root)
    uint32_t sibling_index = 0; // Position among siblings
    uint8_t node_depth = 0;     // Depth from root
};

struct ASTSubtreeInfo {
    uint8_t tree_depth = 0;     // Max depth of subtree rooted here
    uint16_t children_count = 0;
    uint16_t descendant_count = 0;
};

struct ASTNode {
    // Core Semantic Identity - simplified to use node_index from tree traversal
    uint64_t node_id = 0;  // For now, just use tree_position.node_index
    
    // Grouped information
    ASTTypeInfo type;
    ASTNameInfo name;
    ASTFilePosition file_position;
    ASTTreePosition tree_position;
    ASTSubtreeInfo subtree;
    
    // Content preview
    string peek;  // First 120 chars of source_text
    
    // Taxonomy fields using unified semantic type encoding
    uint8_t semantic_type = 0;    // 8-bit encoding: [ss kk tt ll] where:
                                  // ss = super_kind (2 bits), kk = kind (2 bits)
                                  // tt = super_type (2 bits), ll = language_specific (2 bits)
    uint8_t universal_flags = 0;  // is_keyword, is_punctuation, is_builtin, is_public
    uint8_t arity_bin = 0;        // 3-bit Fibonacci-binned complexity
    
    // Legacy decoded fields for compatibility (computed from semantic_type)
    uint8_t kind = 0;           // Extracted from semantic_type bits 4-7
    uint8_t super_type = 0;     // Extracted from semantic_type bits 2-3
    
    // Default constructor
    ASTNode() = default;
    
    // Update legacy fields from semantic_type
    void UpdateLegacyFields() {
        kind = (semantic_type & 0x30) >> 4;      // Extract bits 4-5
        super_type = (semantic_type & 0x0C) >> 2; // Extract bits 2-3
    }
    
    Value ToValue() const;
    static ASTNode FromValue(const Value &value);
    
    // Helper methods for node_id (semantic identity)
    static constexpr uint8_t GetKIND(uint64_t node_id) { 
        return (node_id & 0xF0) >> 4; 
    }
    static constexpr uint8_t GetUniversalFlags(uint64_t node_id) { 
        return node_id & 0x0F; 
    }
    static constexpr bool IsKeyword(uint64_t node_id) { 
        return node_id & 0x01; 
    }
    static constexpr bool IsPunctuation(uint64_t node_id) { 
        return node_id & 0x02; 
    }
    static constexpr bool IsBuiltin(uint64_t node_id) { 
        return node_id & 0x04; 
    }
    static constexpr bool IsPublic(uint64_t node_id) { 
        return node_id & 0x08; 
    }
    
    // Taxonomy generation functions
    static uint64_t GenerateSemanticID(ASTKind kind, uint8_t universal_flags, 
                                      uint8_t super_type = 0, uint8_t parser_type = 0, 
                                      uint8_t arity = 0, uint16_t primary_hash = 0, 
                                      uint16_t parent_hash = 0);
    
    static uint8_t BinArityFibonacci(uint32_t count) {
        // Fibonacci sequence binning: 0, 1, 2, 3, 4-5, 6-8, 9-13, 14+
        if (count == 0) return 0;      // 000
        if (count == 1) return 1;      // 001
        if (count == 2) return 2;      // 010
        if (count == 3) return 3;      // 011
        if (count <= 5) return 4;      // 100
        if (count <= 8) return 5;      // 101
        if (count <= 13) return 6;     // 110
        return 7;                      // 111 (14+)
    }
    
    static string GetKindName(ASTKind kind) {
        switch(kind) {
            case ASTKind::LITERAL: return "LITERAL";
            case ASTKind::NAME: return "NAME";
            case ASTKind::PATTERN: return "PATTERN";
            case ASTKind::TYPE: return "TYPE";
            case ASTKind::OPERATOR: return "OPERATOR";
            case ASTKind::COMPUTATION: return "COMPUTATION";
            case ASTKind::TRANSFORM: return "TRANSFORM";
            case ASTKind::DEFINITION: return "DEFINITION";
            case ASTKind::EXECUTION: return "EXECUTION";
            case ASTKind::FLOW_CONTROL: return "FLOW_CONTROL";
            case ASTKind::ERROR_HANDLING: return "ERROR_HANDLING";
            case ASTKind::ORGANIZATION: return "ORGANIZATION";
            case ASTKind::METADATA: return "METADATA";
            case ASTKind::EXTERNAL: return "EXTERNAL";
            case ASTKind::PARSER_SPECIFIC: return "PARSER_SPECIFIC";
            case ASTKind::RESERVED: return "RESERVED";
            default: return "UNKNOWN";
        }
    }
    void UpdateTaxonomyFields();
};

class ASTType {
public:
    ASTType() = default;
    ASTType(const string &file_path, const string &language);
    ~ASTType();
    
    // Disable copy constructor and assignment (tree-sitter tree can't be copied)
    ASTType(const ASTType&) = delete;
    ASTType& operator=(const ASTType&) = delete;
    
    // Enable move semantics
    ASTType(ASTType&& other) noexcept;
    ASTType& operator=(ASTType&& other) noexcept;
    
    // Core properties
    const string& GetFilePath() const { return file_path; }
    const string& GetLanguage() const { return language; }
    idx_t NodeCount() const { return nodes.size(); }
    const vector<ASTNode>& GetNodes() const { return nodes; }
    
    // Tree operations
    void ParseFile(const string &source_code, TSParser *parser);
    
    // Node access methods
    vector<ASTNode> FindNodes(const string &type) const;
    unique_ptr<ASTNode> GetNodeById(int64_t node_id) const;
    vector<ASTNode> GetChildren(int64_t parent_id) const;
    unique_ptr<ASTNode> GetParent(int64_t node_id) const;
    int32_t MaxDepth() const;
    
    // Serialization
    string ToJSON() const;
    Value Serialize() const;
    static unique_ptr<ASTType> Deserialize(const Value &value);
    
    // For building AST during deserialization
    void AddNode(const ASTNode &node) { nodes.push_back(node); }
    void BuildIndexes();
    
private:
    string file_path;
    string language;
    vector<ASTNode> nodes;
    unordered_map<int64_t, idx_t> node_id_to_index;
    unordered_map<int64_t, vector<idx_t>> parent_to_children;
    TSTree *tree = nullptr;
    
    void ClearTree();
};

// DuckDB type registration
void RegisterASTType(DatabaseInstance &db);

} // namespace duckdb