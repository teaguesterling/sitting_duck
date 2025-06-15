#include "include/semantic_types.hpp"
#include <unordered_map>
#include <vector>

namespace duckdb {

namespace SemanticTypes {

string GetSemanticTypeName(uint8_t semantic_type) {
    switch (semantic_type) {
        // LITERAL types
        case LITERAL_NUMBER: return "LITERAL_NUMBER";
        case LITERAL_STRING: return "LITERAL_STRING";
        case LITERAL_ATOMIC: return "LITERAL_ATOMIC";
        case LITERAL_STRUCTURED: return "LITERAL_STRUCTURED";
        
        // NAME types
        case NAME_KEYWORD: return "NAME_KEYWORD";
        case NAME_IDENTIFIER: return "NAME_IDENTIFIER";
        case NAME_QUALIFIED: return "NAME_QUALIFIED";
        case NAME_SCOPED: return "NAME_SCOPED";
        
        // PATTERN types
        case PATTERN_DESTRUCTURE: return "PATTERN_DESTRUCTURE";
        case PATTERN_MATCH: return "PATTERN_MATCH";
        case PATTERN_TEMPLATE: return "PATTERN_TEMPLATE";
        case PATTERN_GUARD: return "PATTERN_GUARD";
        
        // TYPE types
        case TYPE_PRIMITIVE: return "TYPE_PRIMITIVE";
        case TYPE_COMPOSITE: return "TYPE_COMPOSITE";
        case TYPE_REFERENCE: return "TYPE_REFERENCE";
        case TYPE_GENERIC: return "TYPE_GENERIC";
        
        // OPERATOR types
        case OPERATOR_ARITHMETIC: return "OPERATOR_ARITHMETIC";
        case OPERATOR_LOGICAL: return "OPERATOR_LOGICAL";
        case OPERATOR_COMPARISON: return "OPERATOR_COMPARISON";
        case OPERATOR_ASSIGNMENT: return "OPERATOR_ASSIGNMENT";
        
        // COMPUTATION_NODE types
        case COMPUTATION_CALL: return "COMPUTATION_CALL";
        case COMPUTATION_ACCESS: return "COMPUTATION_ACCESS";
        case COMPUTATION_EXPRESSION: return "COMPUTATION_EXPRESSION";
        case COMPUTATION_LAMBDA: return "COMPUTATION_LAMBDA";
        
        // TRANSFORM types
        case TRANSFORM_QUERY: return "TRANSFORM_QUERY";
        case TRANSFORM_ITERATION: return "TRANSFORM_ITERATION";
        case TRANSFORM_PROJECTION: return "TRANSFORM_PROJECTION";
        case TRANSFORM_AGGREGATION: return "TRANSFORM_AGGREGATION";
        
        // DEFINITION types
        case DEFINITION_FUNCTION: return "DEFINITION_FUNCTION";
        case DEFINITION_VARIABLE: return "DEFINITION_VARIABLE";
        case DEFINITION_CLASS: return "DEFINITION_CLASS";
        case DEFINITION_MODULE: return "DEFINITION_MODULE";
        
        // EXECUTION types
        case EXECUTION_STATEMENT: return "EXECUTION_STATEMENT";
        case EXECUTION_DECLARATION: return "EXECUTION_DECLARATION";
        case EXECUTION_INVOCATION: return "EXECUTION_INVOCATION";
        case EXECUTION_MUTATION: return "EXECUTION_MUTATION";
        
        // FLOW_CONTROL types
        case FLOW_CONDITIONAL: return "FLOW_CONDITIONAL";
        case FLOW_LOOP: return "FLOW_LOOP";
        case FLOW_JUMP: return "FLOW_JUMP";
        case FLOW_SYNC: return "FLOW_SYNC";
        
        // ERROR_HANDLING types
        case ERROR_TRY: return "ERROR_TRY";
        case ERROR_CATCH: return "ERROR_CATCH";
        case ERROR_THROW: return "ERROR_THROW";
        case ERROR_FINALLY: return "ERROR_FINALLY";
        
        // ORGANIZATION types
        case ORGANIZATION_BLOCK: return "ORGANIZATION_BLOCK";
        case ORGANIZATION_LIST: return "ORGANIZATION_LIST";
        case ORGANIZATION_SECTION: return "ORGANIZATION_SECTION";
        case ORGANIZATION_CONTAINER: return "ORGANIZATION_CONTAINER";
        
        // METADATA types
        case METADATA_COMMENT: return "METADATA_COMMENT";
        case METADATA_ANNOTATION: return "METADATA_ANNOTATION";
        case METADATA_DIRECTIVE: return "METADATA_DIRECTIVE";
        case METADATA_DEBUG: return "METADATA_DEBUG";
        
        // EXTERNAL types
        case EXTERNAL_IMPORT: return "EXTERNAL_IMPORT";
        case EXTERNAL_EXPORT: return "EXTERNAL_EXPORT";
        case EXTERNAL_FOREIGN: return "EXTERNAL_FOREIGN";
        case EXTERNAL_EMBED: return "EXTERNAL_EMBED";
        
        // PARSER_SPECIFIC types
        case PARSER_PUNCTUATION: return "PARSER_PUNCTUATION";
        case PARSER_DELIMITER: return "PARSER_DELIMITER";
        case PARSER_SYNTAX: return "PARSER_SYNTAX";
        case PARSER_CONSTRUCT: return "PARSER_CONSTRUCT";
        
        // RESERVED types
        case RESERVED_FUTURE1: return "RESERVED_FUTURE1";
        case RESERVED_FUTURE2: return "RESERVED_FUTURE2";
        case RESERVED_FUTURE3: return "RESERVED_FUTURE3";
        case RESERVED_FUTURE4: return "RESERVED_FUTURE4";
        
        default: return "UNKNOWN_SEMANTIC_TYPE";
    }
}

string GetSuperKindName(uint8_t super_kind) {
    switch (super_kind) {
        case DATA_STRUCTURE: return "DATA_STRUCTURE";
        case COMPUTATION: return "COMPUTATION";
        case CONTROL_EFFECTS: return "CONTROL_EFFECTS";
        case META_EXTERNAL: return "META_EXTERNAL";
        default: return "UNKNOWN_SUPER_KIND";
    }
}

string GetKindName(uint8_t kind) {
    switch (kind) {
        // DATA_STRUCTURE kinds
        case LITERAL: return "LITERAL";
        case NAME: return "NAME";
        case PATTERN: return "PATTERN";
        case TYPE: return "TYPE";
        
        // COMPUTATION kinds
        case OPERATOR: return "OPERATOR";
        case COMPUTATION_NODE: return "COMPUTATION_NODE";
        case TRANSFORM: return "TRANSFORM";
        case DEFINITION: return "DEFINITION";
        
        // CONTROL_EFFECTS kinds
        case EXECUTION: return "EXECUTION";
        case FLOW_CONTROL: return "FLOW_CONTROL";
        case ERROR_HANDLING: return "ERROR_HANDLING";
        case ORGANIZATION: return "ORGANIZATION";
        
        // META_EXTERNAL kinds
        case METADATA: return "METADATA";
        case EXTERNAL: return "EXTERNAL";
        case PARSER_SPECIFIC: return "PARSER_SPECIFIC";
        case RESERVED: return "RESERVED";
        
        default: return "UNKNOWN_KIND";
    }
}

// Reverse lookup - name to code
uint8_t GetSemanticTypeCode(const string& name) {
    // Create a static map for efficient lookup
    static unordered_map<string, uint8_t> name_to_code = {
        // LITERAL types
        {"LITERAL_NUMBER", LITERAL_NUMBER},
        {"LITERAL_STRING", LITERAL_STRING},
        {"LITERAL_ATOMIC", LITERAL_ATOMIC},
        {"LITERAL_STRUCTURED", LITERAL_STRUCTURED},
        
        // NAME types
        {"NAME_KEYWORD", NAME_KEYWORD},
        {"NAME_IDENTIFIER", NAME_IDENTIFIER},
        {"NAME_QUALIFIED", NAME_QUALIFIED},
        {"NAME_SCOPED", NAME_SCOPED},
        
        // PATTERN types
        {"PATTERN_DESTRUCTURE", PATTERN_DESTRUCTURE},
        {"PATTERN_MATCH", PATTERN_MATCH},
        {"PATTERN_TEMPLATE", PATTERN_TEMPLATE},
        {"PATTERN_GUARD", PATTERN_GUARD},
        
        // TYPE types
        {"TYPE_PRIMITIVE", TYPE_PRIMITIVE},
        {"TYPE_COMPOSITE", TYPE_COMPOSITE},
        {"TYPE_REFERENCE", TYPE_REFERENCE},
        {"TYPE_GENERIC", TYPE_GENERIC},
        
        // OPERATOR types
        {"OPERATOR_ARITHMETIC", OPERATOR_ARITHMETIC},
        {"OPERATOR_LOGICAL", OPERATOR_LOGICAL},
        {"OPERATOR_COMPARISON", OPERATOR_COMPARISON},
        {"OPERATOR_ASSIGNMENT", OPERATOR_ASSIGNMENT},
        
        // COMPUTATION_NODE types
        {"COMPUTATION_CALL", COMPUTATION_CALL},
        {"COMPUTATION_ACCESS", COMPUTATION_ACCESS},
        {"COMPUTATION_EXPRESSION", COMPUTATION_EXPRESSION},
        {"COMPUTATION_LAMBDA", COMPUTATION_LAMBDA},
        
        // TRANSFORM types
        {"TRANSFORM_QUERY", TRANSFORM_QUERY},
        {"TRANSFORM_ITERATION", TRANSFORM_ITERATION},
        {"TRANSFORM_PROJECTION", TRANSFORM_PROJECTION},
        {"TRANSFORM_AGGREGATION", TRANSFORM_AGGREGATION},
        
        // DEFINITION types
        {"DEFINITION_FUNCTION", DEFINITION_FUNCTION},
        {"DEFINITION_VARIABLE", DEFINITION_VARIABLE},
        {"DEFINITION_CLASS", DEFINITION_CLASS},
        {"DEFINITION_MODULE", DEFINITION_MODULE},
        
        // EXECUTION types
        {"EXECUTION_STATEMENT", EXECUTION_STATEMENT},
        {"EXECUTION_DECLARATION", EXECUTION_DECLARATION},
        {"EXECUTION_INVOCATION", EXECUTION_INVOCATION},
        {"EXECUTION_MUTATION", EXECUTION_MUTATION},
        
        // FLOW_CONTROL types
        {"FLOW_CONDITIONAL", FLOW_CONDITIONAL},
        {"FLOW_LOOP", FLOW_LOOP},
        {"FLOW_JUMP", FLOW_JUMP},
        {"FLOW_SYNC", FLOW_SYNC},
        
        // ERROR_HANDLING types
        {"ERROR_TRY", ERROR_TRY},
        {"ERROR_CATCH", ERROR_CATCH},
        {"ERROR_THROW", ERROR_THROW},
        {"ERROR_FINALLY", ERROR_FINALLY},
        
        // ORGANIZATION types
        {"ORGANIZATION_BLOCK", ORGANIZATION_BLOCK},
        {"ORGANIZATION_LIST", ORGANIZATION_LIST},
        {"ORGANIZATION_SECTION", ORGANIZATION_SECTION},
        {"ORGANIZATION_CONTAINER", ORGANIZATION_CONTAINER},
        
        // METADATA types
        {"METADATA_COMMENT", METADATA_COMMENT},
        {"METADATA_ANNOTATION", METADATA_ANNOTATION},
        {"METADATA_DIRECTIVE", METADATA_DIRECTIVE},
        {"METADATA_DEBUG", METADATA_DEBUG},
        
        // EXTERNAL types
        {"EXTERNAL_IMPORT", EXTERNAL_IMPORT},
        {"EXTERNAL_EXPORT", EXTERNAL_EXPORT},
        {"EXTERNAL_FOREIGN", EXTERNAL_FOREIGN},
        {"EXTERNAL_EMBED", EXTERNAL_EMBED},
        
        // PARSER_SPECIFIC types
        {"PARSER_PUNCTUATION", PARSER_PUNCTUATION},
        {"PARSER_DELIMITER", PARSER_DELIMITER},
        {"PARSER_SYNTAX", PARSER_SYNTAX},
        {"PARSER_CONSTRUCT", PARSER_CONSTRUCT},
        
        // RESERVED types
        {"RESERVED_FUTURE1", RESERVED_FUTURE1},
        {"RESERVED_FUTURE2", RESERVED_FUTURE2},
        {"RESERVED_FUTURE3", RESERVED_FUTURE3},
        {"RESERVED_FUTURE4", RESERVED_FUTURE4}
    };
    
    auto it = name_to_code.find(name);
    if (it != name_to_code.end()) {
        return it->second;
    }
    return 255; // Invalid code
}

uint8_t GetKindCode(const string& name) {
    static unordered_map<string, uint8_t> kind_to_code = {
        // DATA_STRUCTURE kinds
        {"LITERAL", LITERAL},
        {"NAME", NAME},
        {"PATTERN", PATTERN},
        {"TYPE", TYPE},
        
        // COMPUTATION kinds
        {"OPERATOR", OPERATOR},
        {"COMPUTATION_NODE", COMPUTATION_NODE},
        {"TRANSFORM", TRANSFORM},
        {"DEFINITION", DEFINITION},
        
        // CONTROL_EFFECTS kinds
        {"EXECUTION", EXECUTION},
        {"FLOW_CONTROL", FLOW_CONTROL},
        {"ERROR_HANDLING", ERROR_HANDLING},
        {"ORGANIZATION", ORGANIZATION},
        
        // META_EXTERNAL kinds
        {"METADATA", METADATA},
        {"EXTERNAL", EXTERNAL},
        {"PARSER_SPECIFIC", PARSER_SPECIFIC},
        {"RESERVED", RESERVED}
    };
    
    auto it = kind_to_code.find(name);
    if (it != kind_to_code.end()) {
        return it->second;
    }
    return 255; // Invalid code
}

// Helper predicates
bool IsDefinition(uint8_t semantic_type) {
    return (semantic_type & 0xF0) == DEFINITION;
}

bool IsCall(uint8_t semantic_type) {
    return semantic_type == COMPUTATION_CALL || semantic_type == EXECUTION_INVOCATION;
}

bool IsControlFlow(uint8_t semantic_type) {
    return (semantic_type & 0xF0) == FLOW_CONTROL;
}

bool IsIdentifier(uint8_t semantic_type) {
    return semantic_type == NAME_IDENTIFIER || semantic_type == NAME_QUALIFIED || semantic_type == NAME_SCOPED;
}

bool IsLiteral(uint8_t semantic_type) {
    return (semantic_type & 0xF0) == LITERAL;
}

bool IsOperator(uint8_t semantic_type) {
    return (semantic_type & 0xF0) == OPERATOR;
}

bool IsType(uint8_t semantic_type) {
    return (semantic_type & 0xF0) == TYPE;
}

bool IsExternal(uint8_t semantic_type) {
    return (semantic_type & 0xF0) == EXTERNAL;
}

bool IsError(uint8_t semantic_type) {
    return (semantic_type & 0xF0) == ERROR_HANDLING;
}

bool IsMetadata(uint8_t semantic_type) {
    return (semantic_type & 0xF0) == METADATA;
}

// Get all types in a category
vector<uint8_t> GetDefinitionTypes() {
    return {DEFINITION_FUNCTION, DEFINITION_VARIABLE, DEFINITION_CLASS, DEFINITION_MODULE};
}

vector<uint8_t> GetControlFlowTypes() {
    return {FLOW_CONDITIONAL, FLOW_LOOP, FLOW_JUMP, FLOW_SYNC};
}

vector<uint8_t> GetSearchableTypes() {
    // Primary search targets - definitions, calls, imports/exports, and control flow
    return {
        // All definitions
        DEFINITION_FUNCTION, DEFINITION_VARIABLE, DEFINITION_CLASS, DEFINITION_MODULE,
        // Calls and access
        COMPUTATION_CALL, COMPUTATION_ACCESS,
        // External dependencies
        EXTERNAL_IMPORT, EXTERNAL_EXPORT,
        // Control flow
        FLOW_CONDITIONAL, FLOW_LOOP, FLOW_JUMP,
        // Error handling
        ERROR_TRY, ERROR_CATCH, ERROR_THROW,
        // Key identifiers
        NAME_IDENTIFIER, NAME_QUALIFIED
    };
}

} // namespace SemanticTypes

} // namespace duckdb