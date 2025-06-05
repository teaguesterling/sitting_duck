#include "include/semantic_types.hpp"

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

} // namespace SemanticTypes

} // namespace duckdb