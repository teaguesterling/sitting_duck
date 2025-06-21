#pragma once

#include "duckdb.hpp"
#include "semantic_types.hpp"
#include <unordered_map>

namespace duckdb {

// Extraction strategy for node names/values
enum class ExtractionStrategy : uint8_t {
    NONE = 0,                   // No extraction needed
    NODE_TEXT = 1,              // Extract the node's own text content
    FIRST_CHILD = 2,            // Extract text from first child
    FIND_IDENTIFIER = 3,        // Find first child of type "identifier" 
    FIND_PROPERTY = 4,          // Find first child of type "property_identifier"
    FIND_ASSIGNMENT_TARGET = 5, // Find identifier in parent assignment (universal pattern)
    CUSTOM = 6                  // Language-specific custom logic
};

// Simple node configuration
struct NodeConfig {
    uint8_t semantic_type;          // 8-bit semantic type encoding
    ExtractionStrategy name_strategy;  // How to extract names
    ExtractionStrategy value_strategy; // How to extract values
    uint8_t flags;                  // Basic flags (is_punctuation, etc.)
    
    // Constructor
    NodeConfig(uint8_t sem_type = 0, 
               ExtractionStrategy name_strat = ExtractionStrategy::NONE,
               ExtractionStrategy value_strat = ExtractionStrategy::NONE,
               uint8_t node_flags = 0)
        : semantic_type(sem_type), name_strategy(name_strat), 
          value_strategy(value_strat), flags(node_flags) {}
};

// Universal flags for orthogonal node properties
namespace ASTNodeFlags {
    constexpr uint8_t IS_KEYWORD = 0x01;   // Reserved language keywords
    constexpr uint8_t IS_PUBLIC = 0x02;    // Externally visible/accessible
    constexpr uint8_t IS_UNSAFE = 0x04;    // Unsafe operations
    constexpr uint8_t RESERVED = 0x08;     // Reserved for future use
}

// Normalized type constants for cross-language consistency
namespace NormalizedTypes {
    // Declarations/Definitions
    constexpr const char* FUNCTION_DECLARATION = "function_declaration";
    constexpr const char* CLASS_DECLARATION = "class_declaration";
    constexpr const char* VARIABLE_DECLARATION = "variable_declaration";
    constexpr const char* METHOD_DECLARATION = "method_declaration";
    constexpr const char* PARAMETER_DECLARATION = "parameter_declaration";
    
    // Expressions/Computations
    constexpr const char* FUNCTION_CALL = "function_call";
    constexpr const char* VARIABLE_REFERENCE = "variable_reference";
    constexpr const char* BINARY_EXPRESSION = "binary_expression";
    constexpr const char* UNARY_EXPRESSION = "unary_expression";
    constexpr const char* ASSIGNMENT_EXPRESSION = "assignment_expression";
    
    // Literals
    constexpr const char* STRING_LITERAL = "string_literal";
    constexpr const char* NUMBER_LITERAL = "number_literal";
    constexpr const char* BOOLEAN_LITERAL = "boolean_literal";
    constexpr const char* NULL_LITERAL = "null_literal";
    
    // Control Flow
    constexpr const char* IF_STATEMENT = "if_statement";
    constexpr const char* FOR_STATEMENT = "for_statement";
    constexpr const char* WHILE_STATEMENT = "while_statement";
    constexpr const char* RETURN_STATEMENT = "return_statement";
    constexpr const char* BREAK_STATEMENT = "break_statement";
    constexpr const char* CONTINUE_STATEMENT = "continue_statement";
    
    // Structure/Organization
    constexpr const char* BLOCK = "block";
    constexpr const char* MODULE = "module";
    constexpr const char* IMPORT_STATEMENT = "import_statement";
    constexpr const char* EXPORT_STATEMENT = "export_statement";
    
    // Other
    constexpr const char* COMMENT = "comment";
    constexpr const char* IDENTIFIER = "identifier";
    constexpr const char* OPERATOR = "operator";
    constexpr const char* PUNCTUATION = "punctuation";
}

} // namespace duckdb