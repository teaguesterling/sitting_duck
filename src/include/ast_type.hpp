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

// Universal Flags - orthogonal properties that apply across semantic types
enum class ASTFlagValues : uint8_t {
    IS_KEYWORD = 0x01,     // Reserved language keywords (def, class, if, for, etc.)
    IS_PUBLIC = 0x02,      // Externally visible/accessible (public, export, etc.)
    IS_UNSAFE = 0x04,      // Unsafe operations (Rust unsafe, C pointers, inline asm)
    RESERVED = 0x08        // Reserved for future orthogonal properties
};

// Extraction Level Enums for Structured Extraction
enum class ContextLevel : uint8_t {
    NONE = 0,           // No semantic analysis (raw tree only)
    NODE_TYPES_ONLY,    // + semantic_type, universal_flags, arity_bin
    NORMALIZED,         // + name extraction (requires node_types_only)
    NATIVE              // + language-specific signatures (requires normalized)
};

enum class SourceLevel : uint8_t {
    NONE = 0,           // No source location info
    PATH,               // + file_path, language  
    LINES_ONLY,         // + start_line, end_line (no path duplication)
    LINES,              // + file_path, language, start_line, end_line
    FULL                // + file_path, language, start_line, end_line, start_column, end_column
};

enum class StructureLevel : uint8_t {
    NONE = 0,           // No tree structure info
    MINIMAL,            // + parent_id, depth, sibling_index (O(1) fields)
    FULL                // + children_count, descendant_count (O(child_count) fields)
};

enum class PeekLevel : uint8_t {
    NONE = 0,           // No source preview
    SMART,              // Adaptive preview based on node type
    FULL,               // Complete source text for node
    CUSTOM              // Fixed character limit (specified separately)
};

// Organized Field Groups for Structured Extraction
struct SourceLocation {
    string file_path;       // Available if source >= PATH
    string language;        // Available if source >= PATH  
    uint32_t start_line;    // Available if source >= LINES_ONLY
    uint32_t end_line;      // Available if source >= LINES_ONLY
    uint32_t start_column;  // Available if source >= FULL
    uint32_t end_column;    // Available if source >= FULL
    
    // Default constructor
    SourceLocation() : start_line(0), end_line(0), start_column(0), end_column(0) {}
};

struct TreeStructure {
    int64_t parent_id;          // Available if structure >= MINIMAL (O(1))
    uint32_t depth;             // Available if structure >= MINIMAL (O(1))
    uint32_t sibling_index;     // Available if structure >= MINIMAL (O(1))
    uint32_t children_count;    // Available if structure >= FULL (O(child_count))
    uint32_t descendant_count;  // Available if structure >= FULL (O(child_count))
    
    // Default constructor
    TreeStructure() : parent_id(-1), depth(0), sibling_index(0), 
                     children_count(0), descendant_count(0) {}
};

struct NormalizedSemantics {
    uint8_t semantic_type;      // Available if context >= NODE_TYPES_ONLY
    uint8_t universal_flags;    // Available if context >= NODE_TYPES_ONLY  
    uint8_t arity_bin;          // Available if context >= NODE_TYPES_ONLY
    
    // Default constructor
    NormalizedSemantics() : semantic_type(0), universal_flags(0), arity_bin(0) {}
};

struct ParameterInfo {
    string name;               // Parameter name
    string type;               // Parameter type (empty if not typed)
    string default_value;      // Default value (empty if none)
    bool is_optional;          // Whether parameter is optional
    bool is_variadic;          // Whether parameter is variadic (*args, **kwargs, ...rest)
    string annotations;        // JSON for language-specific metadata
    
    // Constructor
    ParameterInfo(const string& param_name = "", 
                  const string& param_type = "",
                  const string& default_val = "",
                  bool optional = false,
                  bool variadic = false,
                  const string& annot = "{}")
        : name(param_name), type(param_type), default_value(default_val),
          is_optional(optional), is_variadic(variadic), annotations(annot) {}
};

struct NativeContext {
    string signature_type;              // Return type (functions) | Variable type | Class type
    vector<ParameterInfo> parameters;   // Function/method parameters (empty for non-parameterized)
    vector<string> modifiers;           // ['async', 'public', 'static'] - cross-language standard
    string qualified_name;              // 'MyClass.my_method' (if determinable from AST)
    string annotations;                 // JSON for language-specific metadata, decorators, etc.
    
    // Default constructor
    NativeContext() = default;
    
    // Helper constructor
    NativeContext(const string& sig_type,
                  const vector<ParameterInfo>& params = {},
                  const vector<string>& mods = {},
                  const string& qual_name = "",
                  const string& annot = "{}")
        : signature_type(sig_type), parameters(params), modifiers(mods),
          qualified_name(qual_name), annotations(annot) {}
};

struct ContextInfo {
    string name;                    // Available if context >= NORMALIZED
    NormalizedSemantics normalized; // Available if context >= NODE_TYPES_ONLY
    NativeContext native;           // Available if context >= NATIVE
    bool native_extraction_attempted = false; // Track if native extraction was attempted
    
    // Default constructor
    ContextInfo() = default;
};

// Legacy type definitions for backward compatibility
struct ASTTypeInfo {
    string raw;        // Raw parser type (e.g., "binary_expression")
    string normalized; // Normalized type (e.g., "BinaryExpression")
    string kind;       // KIND name (e.g., "COMPUTATION")
};

struct ASTNameInfo {
    string raw;        // Raw identifier text
    string qualified;  // Fully qualified name (e.g., "MyClass.myMethod")
};

struct ASTNode {
    // Core Semantic Identity
    uint64_t node_id = 0;  // Unique identifier for this node
    
    // FLATTENED STRUCTURE: Direct fields instead of nested structs
    // Tree structure fields (flat)
    int64_t parent_id = -1;          // Parent node ID
    uint32_t depth = 0;              // Depth from root
    uint32_t sibling_index = 0;      // Position among siblings
    uint32_t children_count = 0;     // Number of direct children
    uint32_t descendant_count = 0;   // Total descendants (DFS count)
    
    // Legacy tree fields (flat) - kept separate for future use
    int64_t node_index = 0;          // Position in depth-first traversal
    int64_t parent_index = -1;       // Parent's position (-1 for root)
    uint32_t legacy_sibling_index = 0; // Position among siblings (legacy)
    uint8_t node_depth = 0;          // Depth from root (legacy)
    
    // Legacy file position fields (flat)
    int64_t start_line = 0;
    int64_t end_line = 0;
    uint16_t start_column = 0;
    uint16_t end_column = 0;
    
    // Legacy subtree fields (flat)
    uint8_t tree_depth = 0;          // Max depth of subtree rooted here
    uint16_t legacy_children_count = 0;   // Number of children (legacy)
    uint16_t legacy_descendant_count = 0; // Total descendants (legacy)
    
    // FULLY FLATTENED FIELDS (no nested structs except native context)
    
    // Source location fields (flattened from SourceLocation)
    string file_path;           // Source file path
    string language;            // Programming language
    uint32_t source_start_line = 0;     // Starting line number
    uint32_t source_end_line = 0;       // Ending line number
    uint32_t source_start_column = 0;   // Starting column
    uint32_t source_end_column = 0;     // Ending column
    
    // Context fields (flattened from ContextInfo, except native)
    string name_raw;            // Raw node name/identifier
    string name_qualified;      // Fully qualified name
    bool native_extraction_attempted = false; // Track native extraction attempts
    NativeContext native;       // ONLY remaining nested struct - language-specific data
    
    // Type fields (flattened from ASTTypeInfo)
    string type_raw;            // Raw parser type name
    string type_normalized;     // Normalized type name
    string type_kind;           // Type kind name
    
    // Content preview
    string peek;               // Source code snippet
    
    // Legacy taxonomy fields (BACKWARD COMPATIBILITY)
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
    
    // FULLY FLAT TRANSITION: Make legacy fields computed properties
    // These provide backward compatibility by referencing flat fields
    void UpdateComputedLegacyFields() {
        // Copy from flat source fields to legacy file position fields
        start_line = static_cast<int64_t>(source_start_line);
        end_line = static_cast<int64_t>(source_end_line);
        start_column = static_cast<uint16_t>(source_start_column);
        end_column = static_cast<uint16_t>(source_end_column);
        
        // Copy from structure fields to flat legacy tree fields
        parent_index = parent_id;
        node_depth = static_cast<uint8_t>(depth);
        legacy_sibling_index = sibling_index;
        node_index = static_cast<int64_t>(node_id);
        
        // Copy from structure fields to flat legacy subtree fields
        legacy_children_count = static_cast<uint16_t>(children_count);
        legacy_descendant_count = static_cast<uint16_t>(descendant_count);
        
        // Legacy name and semantic info references flat fields
        // Note: These legacy fields will be removed in a future version
        // (semantic_type, universal_flags, arity_bin are already flat - no copying needed)
        
        // Update computed taxonomy fields from semantic_type
        UpdateLegacyFields();
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