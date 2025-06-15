#pragma once

#include "duckdb.hpp"

namespace duckdb {

// 8-bit semantic type encoding:
// Byte layout: [ ss kk tt ll ]
// ss = super_kind (bits 6-7): DATA_STRUCTURE=00, COMPUTATION=01, CONTROL_EFFECTS=10, META_EXTERNAL=11  
// kk = kind (bits 4-5): 4 kinds within each super_kind
// tt = super_type (bits 2-3): 4 variants within each kind
// ll = language_specific (bits 0-1): language-specific sub-type (unused for now)

namespace SemanticTypes {

// Super kinds (bits 6-7)
constexpr uint8_t DATA_STRUCTURE = 0x00;    // 00xx xxxx
constexpr uint8_t COMPUTATION = 0x40;       // 01xx xxxx  
constexpr uint8_t CONTROL_EFFECTS = 0x80;   // 10xx xxxx
constexpr uint8_t META_EXTERNAL = 0xC0;     // 11xx xxxx

// Kinds within DATA_STRUCTURE (00ss ssxx)
constexpr uint8_t LITERAL = DATA_STRUCTURE | 0x00;   // 0000 xxxx
constexpr uint8_t NAME = DATA_STRUCTURE | 0x10;      // 0001 xxxx
constexpr uint8_t PATTERN = DATA_STRUCTURE | 0x20;   // 0010 xxxx
constexpr uint8_t TYPE = DATA_STRUCTURE | 0x30;      // 0011 xxxx

// Kinds within COMPUTATION (01ss ssxx)
constexpr uint8_t OPERATOR = COMPUTATION | 0x00;     // 0100 xxxx
constexpr uint8_t COMPUTATION_NODE = COMPUTATION | 0x10; // 0101 xxxx
constexpr uint8_t TRANSFORM = COMPUTATION | 0x20;    // 0110 xxxx
constexpr uint8_t DEFINITION = COMPUTATION | 0x30;   // 0111 xxxx

// Kinds within CONTROL_EFFECTS (10ss ssxx)
constexpr uint8_t EXECUTION = CONTROL_EFFECTS | 0x00;      // 1000 xxxx
constexpr uint8_t FLOW_CONTROL = CONTROL_EFFECTS | 0x10;   // 1001 xxxx
constexpr uint8_t ERROR_HANDLING = CONTROL_EFFECTS | 0x20; // 1010 xxxx
constexpr uint8_t ORGANIZATION = CONTROL_EFFECTS | 0x30;   // 1011 xxxx

// Kinds within META_EXTERNAL (11ss ssxx)
constexpr uint8_t METADATA = META_EXTERNAL | 0x00;         // 1100 xxxx
constexpr uint8_t EXTERNAL = META_EXTERNAL | 0x10;         // 1101 xxxx
constexpr uint8_t PARSER_SPECIFIC = META_EXTERNAL | 0x20;  // 1110 xxxx
constexpr uint8_t RESERVED = META_EXTERNAL | 0x30;         // 1111 xxxx

// ===== LITERAL super types (0000 ttxx) =====
constexpr uint8_t LITERAL_NUMBER = LITERAL | 0x00;       // 0000 0000 - integers, floats, decimals
constexpr uint8_t LITERAL_STRING = LITERAL | 0x04;       // 0000 0100 - strings, chars, text
constexpr uint8_t LITERAL_ATOMIC = LITERAL | 0x08;       // 0000 1000 - true, false, null, None, undefined
constexpr uint8_t LITERAL_STRUCTURED = LITERAL | 0x0C;   // 0000 1100 - arrays, objects, composite

// ===== NAME super types (0001 ttxx) =====
constexpr uint8_t NAME_KEYWORD = NAME | 0x00;            // 0001 0000 - language keywords
constexpr uint8_t NAME_IDENTIFIER = NAME | 0x04;         // 0001 0100 - simple identifiers
constexpr uint8_t NAME_QUALIFIED = NAME | 0x08;          // 0001 1000 - qualified names (obj.prop)
constexpr uint8_t NAME_SCOPED = NAME | 0x0C;             // 0001 1100 - scoped references (::, this, super)

// ===== PATTERN super types (0010 ttxx) =====
constexpr uint8_t PATTERN_DESTRUCTURE = PATTERN | 0x00;  // 0010 0000 - destructuring patterns
constexpr uint8_t PATTERN_MATCH = PATTERN | 0x04;        // 0010 0100 - pattern matching constructs
constexpr uint8_t PATTERN_TEMPLATE = PATTERN | 0x08;     // 0010 1000 - template patterns
constexpr uint8_t PATTERN_GUARD = PATTERN | 0x0C;        // 0010 1100 - guards and conditions

// ===== TYPE super types (0011 ttxx) =====
constexpr uint8_t TYPE_PRIMITIVE = TYPE | 0x00;          // 0011 0000 - basic types (int, string, etc)
constexpr uint8_t TYPE_COMPOSITE = TYPE | 0x04;          // 0011 0100 - structs, unions, tuples
constexpr uint8_t TYPE_REFERENCE = TYPE | 0x08;          // 0011 1000 - pointers, references
constexpr uint8_t TYPE_GENERIC = TYPE | 0x0C;            // 0011 1100 - generic/template types

// ===== OPERATOR super types (0100 ttxx) =====
constexpr uint8_t OPERATOR_ARITHMETIC = OPERATOR | 0x00; // 0100 0000 - +, -, *, /, %, **, //, &, |, ^, ~, <<, >>
constexpr uint8_t OPERATOR_LOGICAL = OPERATOR | 0x04;    // 0100 0100 - &&, ||, !, and, or, not, ? :
constexpr uint8_t OPERATOR_COMPARISON = OPERATOR | 0x08; // 0100 1000 - ==, !=, <, >, <=, >=, ===, is, in, not in
constexpr uint8_t OPERATOR_ASSIGNMENT = OPERATOR | 0x0C; // 0100 1100 - =, +=, -=, *=, /=, :=, etc.

// ===== COMPUTATION_NODE super types (0101 ttxx) =====
constexpr uint8_t COMPUTATION_CALL = COMPUTATION_NODE | 0x00;      // 0101 0000 - function calls
constexpr uint8_t COMPUTATION_ACCESS = COMPUTATION_NODE | 0x04;    // 0101 0100 - member access, indexing
constexpr uint8_t COMPUTATION_EXPRESSION = COMPUTATION_NODE | 0x08; // 0101 1000 - complex expressions
constexpr uint8_t COMPUTATION_LAMBDA = COMPUTATION_NODE | 0x0C;    // 0101 1100 - lambdas, anonymous functions

// ===== TRANSFORM super types (0110 ttxx) =====
constexpr uint8_t TRANSFORM_QUERY = TRANSFORM | 0x00;    // 0110 0000 - SQL queries, LINQ
constexpr uint8_t TRANSFORM_ITERATION = TRANSFORM | 0x04; // 0110 0100 - map, filter, reduce
constexpr uint8_t TRANSFORM_PROJECTION = TRANSFORM | 0x08; // 0110 1000 - select, extract operations
constexpr uint8_t TRANSFORM_AGGREGATION = TRANSFORM | 0x0C; // 0110 1100 - group by, aggregate ops

// ===== DEFINITION super types (0111 ttxx) =====
constexpr uint8_t DEFINITION_FUNCTION = DEFINITION | 0x00;   // 0111 0000 - function definitions
constexpr uint8_t DEFINITION_VARIABLE = DEFINITION | 0x04;   // 0111 0100 - variable/constant definitions
constexpr uint8_t DEFINITION_CLASS = DEFINITION | 0x08;      // 0111 1000 - class/struct definitions
constexpr uint8_t DEFINITION_MODULE = DEFINITION | 0x0C;     // 0111 1100 - modules, namespaces

// ===== EXECUTION super types (1000 ttxx) =====
constexpr uint8_t EXECUTION_STATEMENT = EXECUTION | 0x00;    // 1000 0000 - expression statements
constexpr uint8_t EXECUTION_DECLARATION = EXECUTION | 0x04;  // 1000 0100 - variable declarations
constexpr uint8_t EXECUTION_INVOCATION = EXECUTION | 0x08;   // 1000 1000 - function/method calls
constexpr uint8_t EXECUTION_MUTATION = EXECUTION | 0x0C;     // 1000 1100 - assignments, scope modifications

// ===== FLOW_CONTROL super types (1001 ttxx) =====
constexpr uint8_t FLOW_CONDITIONAL = FLOW_CONTROL | 0x00;    // 1001 0000 - if, switch, match
constexpr uint8_t FLOW_LOOP = FLOW_CONTROL | 0x04;           // 1001 0100 - for, while, do-while
constexpr uint8_t FLOW_JUMP = FLOW_CONTROL | 0x08;           // 1001 1000 - break, continue, return, goto
constexpr uint8_t FLOW_SYNC = FLOW_CONTROL | 0x0C;           // 1001 1100 - async, await, synchronized, yield

// ===== ERROR_HANDLING super types (1010 ttxx) =====
constexpr uint8_t ERROR_TRY = ERROR_HANDLING | 0x00;         // 1010 0000 - try blocks
constexpr uint8_t ERROR_CATCH = ERROR_HANDLING | 0x04;       // 1010 0100 - catch, except blocks
constexpr uint8_t ERROR_THROW = ERROR_HANDLING | 0x08;       // 1010 1000 - throw, raise statements
constexpr uint8_t ERROR_FINALLY = ERROR_HANDLING | 0x0C;     // 1010 1100 - finally, ensure blocks

// ===== ORGANIZATION super types (1011 ttxx) =====
constexpr uint8_t ORGANIZATION_BLOCK = ORGANIZATION | 0x00;  // 1011 0000 - code blocks, scopes
constexpr uint8_t ORGANIZATION_LIST = ORGANIZATION | 0x04;   // 1011 0100 - argument lists, parameter lists
constexpr uint8_t ORGANIZATION_SECTION = ORGANIZATION | 0x08; // 1011 1000 - sections, regions
constexpr uint8_t ORGANIZATION_CONTAINER = ORGANIZATION | 0x0C; // 1011 1100 - files, modules, packages

// ===== METADATA super types (1100 ttxx) =====
constexpr uint8_t METADATA_COMMENT = METADATA | 0x00;        // 1100 0000 - comments, documentation
constexpr uint8_t METADATA_ANNOTATION = METADATA | 0x04;     // 1100 0100 - decorators, attributes
constexpr uint8_t METADATA_DIRECTIVE = METADATA | 0x08;      // 1100 1000 - preprocessor directives
constexpr uint8_t METADATA_DEBUG = METADATA | 0x0C;          // 1100 1100 - debug information, source maps

// ===== EXTERNAL super types (1101 ttxx) =====
constexpr uint8_t EXTERNAL_IMPORT = EXTERNAL | 0x00;         // 1101 0000 - import statements
constexpr uint8_t EXTERNAL_EXPORT = EXTERNAL | 0x04;         // 1101 0100 - export statements
constexpr uint8_t EXTERNAL_FOREIGN = EXTERNAL | 0x08;        // 1101 1000 - foreign function interface
constexpr uint8_t EXTERNAL_EMBED = EXTERNAL | 0x0C;          // 1101 1100 - embedded content (HTML, CSS, SQL)

// ===== PARSER_SPECIFIC super types (1110 ttxx) =====
constexpr uint8_t PARSER_PUNCTUATION = PARSER_SPECIFIC | 0x00; // 1110 0000 - language-specific punctuation
constexpr uint8_t PARSER_DELIMITER = PARSER_SPECIFIC | 0x04;   // 1110 0100 - delimiters, separators
constexpr uint8_t PARSER_SYNTAX = PARSER_SPECIFIC | 0x08;      // 1110 1000 - syntax elements
constexpr uint8_t PARSER_CONSTRUCT = PARSER_SPECIFIC | 0x0C;   // 1110 1100 - unique language constructs

// ===== RESERVED super types (1111 ttxx) =====
constexpr uint8_t RESERVED_FUTURE1 = RESERVED | 0x00;         // 1111 0000 - reserved for future use
constexpr uint8_t RESERVED_FUTURE2 = RESERVED | 0x04;         // 1111 0100 - reserved for future use  
constexpr uint8_t RESERVED_FUTURE3 = RESERVED | 0x08;         // 1111 1000 - reserved for future use
constexpr uint8_t RESERVED_FUTURE4 = RESERVED | 0x0C;         // 1111 1100 - reserved for future use

// Utility functions to extract components
constexpr uint8_t GetSuperKind(uint8_t semantic_type) {
    return semantic_type & 0xC0; // Extract bits 6-7
}

constexpr uint8_t GetKind(uint8_t semantic_type) {
    return semantic_type & 0xF0; // Extract bits 4-7 (full kind value)
}

constexpr uint8_t GetSuperType(uint8_t semantic_type) {
    return (semantic_type & 0x0C) >> 2; // Extract bits 2-3 and shift to 0-3
}

constexpr uint8_t GetLanguageSpecific(uint8_t semantic_type) {
    return semantic_type & 0x03; // Extract bits 0-1
}

// Get human-readable names
string GetSemanticTypeName(uint8_t semantic_type);
string GetSuperKindName(uint8_t super_kind);
string GetKindName(uint8_t kind);

// Shorter convenience functions
inline string TypeName(uint8_t code) { return GetSemanticTypeName(code); }
inline string KindName(uint8_t kind) { return GetKindName(kind); }

// Reverse lookups - name to code
uint8_t GetSemanticTypeCode(const string& name);
uint8_t GetKindCode(const string& name);
uint8_t GetSuperKindCode(const string& name);

// Shorter convenience functions
inline uint8_t TypeCode(const string& name) { return GetSemanticTypeCode(name); }
inline uint8_t KindCode(const string& name) { return GetKindCode(name); }

// Helper predicates for common queries
bool IsDefinition(uint8_t semantic_type);
bool IsCall(uint8_t semantic_type);
bool IsControlFlow(uint8_t semantic_type);
bool IsIdentifier(uint8_t semantic_type);
bool IsLiteral(uint8_t semantic_type);
bool IsOperator(uint8_t semantic_type);
bool IsType(uint8_t semantic_type);
bool IsExternal(uint8_t semantic_type);
bool IsError(uint8_t semantic_type);
bool IsMetadata(uint8_t semantic_type);

// Get all types in a category
vector<uint8_t> GetDefinitionTypes();
vector<uint8_t> GetControlFlowTypes();
vector<uint8_t> GetSearchableTypes();  // Types typically used in searches

} // namespace SemanticTypes

} // namespace duckdb