#include "duckdb.hpp"
#include "duckdb/main/extension_util.hpp"
#include "include/semantic_types.hpp"

namespace duckdb {

// Scalar function that converts semantic_type integer to human-readable string
static void SemanticTypeToStringFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<uint8_t, string_t>(
        semantic_type_vector, result, count,
        [&](uint8_t semantic_type) {
            string type_name = SemanticTypes::GetSemanticTypeName(semantic_type);
            return StringVector::AddString(result, type_name);
        }
    );
}

// Function that gets the super kind name from semantic type
static void GetSuperKindFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<uint8_t, string_t>(
        semantic_type_vector, result, count,
        [&](uint8_t semantic_type) {
            uint8_t super_kind = SemanticTypes::GetSuperKind(semantic_type);
            string super_kind_name = SemanticTypes::GetSuperKindName(super_kind);
            return StringVector::AddString(result, super_kind_name);
        }
    );
}

// Function that gets the kind name from semantic type
static void GetKindFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<uint8_t, string_t>(
        semantic_type_vector, result, count,
        [&](uint8_t semantic_type) {
            uint8_t kind = SemanticTypes::GetKind(semantic_type);
            string kind_name = SemanticTypes::GetKindName(kind);
            return StringVector::AddString(result, kind_name);
        }
    );
}

// Function that checks if semantic type matches a specific pattern
static void IsSemanticTypeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 2);
    
    auto &semantic_type_vector = args.data[0];
    auto &pattern_vector = args.data[1];
    auto count = args.size();
    
    BinaryExecutor::Execute<uint8_t, string_t, bool>(
        semantic_type_vector, pattern_vector, result, count,
        [&](uint8_t semantic_type, string_t pattern_str) {
            string pattern = pattern_str.GetString();
            uint8_t type = semantic_type;
            
            // Support common patterns
            if (pattern == "FUNCTION") {
                return type == SemanticTypes::DEFINITION_FUNCTION;
            } else if (pattern == "CLASS") {
                return type == SemanticTypes::DEFINITION_CLASS;
            } else if (pattern == "VARIABLE") {
                return type == SemanticTypes::DEFINITION_VARIABLE;
            } else if (pattern == "IDENTIFIER") {
                return type == SemanticTypes::NAME_IDENTIFIER;
            } else if (pattern == "CALL") {
                return type == SemanticTypes::COMPUTATION_CALL;
            } else if (pattern == "LITERAL") {
                return (type & 0xF0) == SemanticTypes::LITERAL;
            } else if (pattern == "DEFINITION") {
                return (type & 0xF0) == SemanticTypes::DEFINITION;
            } else if (pattern == "COMPUTATION") {
                return (type & 0xC0) == SemanticTypes::COMPUTATION;
            }
            
            // Default: exact string match with semantic type name
            return SemanticTypes::GetSemanticTypeName(type) == pattern;
        }
    );
}

// Function that converts semantic type name to code
static void SemanticTypeCodeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &name_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::ExecuteWithNulls<string_t, uint8_t>(
        name_vector, result, count,
        [&](string_t name_str, ValidityMask &mask, idx_t idx) {
            string name = name_str.GetString();
            uint8_t code = SemanticTypes::GetSemanticTypeCode(name);
            if (code == 255) {
                mask.SetInvalid(idx);
                return uint8_t(0); // Return value doesn't matter when NULL
            }
            return code;
        }
    );
}

// Function that converts kind name to code
static void KindCodeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &name_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::ExecuteWithNulls<string_t, uint8_t>(
        name_vector, result, count,
        [&](string_t name_str, ValidityMask &mask, idx_t idx) {
            string name = name_str.GetString();
            uint8_t code = SemanticTypes::GetKindCode(name);
            if (code == 255) {
                mask.SetInvalid(idx);
                return uint8_t(0); // Return value doesn't matter when NULL
            }
            return code;
        }
    );
}

// Function that checks if semantic type belongs to a specific kind
static void IsKindFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 2);
    
    auto &semantic_type_vector = args.data[0];
    auto &kind_name_vector = args.data[1];
    auto count = args.size();
    
    BinaryExecutor::Execute<uint8_t, string_t, bool>(
        semantic_type_vector, kind_name_vector, result, count,
        [&](uint8_t semantic_type, string_t kind_str) {
            string kind_name = kind_str.GetString();
            uint8_t kind_code = SemanticTypes::GetKindCode(kind_name);
            if (kind_code == 255) {
                return false; // Invalid kind name
            }
            // Check if the semantic type's kind matches
            return (semantic_type & 0xF0) == kind_code;
        }
    );
}

// Predicate functions for common type categories
static void IsDefinitionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<uint8_t, bool>(
        semantic_type_vector, result, count,
        [&](uint8_t semantic_type) {
            return SemanticTypes::IsDefinition(semantic_type);
        }
    );
}

static void IsCallFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<uint8_t, bool>(
        semantic_type_vector, result, count,
        [&](uint8_t semantic_type) {
            return SemanticTypes::IsCall(semantic_type);
        }
    );
}

static void IsControlFlowFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<uint8_t, bool>(
        semantic_type_vector, result, count,
        [&](uint8_t semantic_type) {
            return SemanticTypes::IsControlFlow(semantic_type);
        }
    );
}

static void IsIdentifierFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<uint8_t, bool>(
        semantic_type_vector, result, count,
        [&](uint8_t semantic_type) {
            return SemanticTypes::IsIdentifier(semantic_type);
        }
    );
}

// Function that returns list of searchable semantic types
static void GetSearchableTypesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 0);
    
    auto searchable_types = SemanticTypes::GetSearchableTypes();
    auto count = args.size();
    
    // Create a list value for each row
    auto &list_vector = result;
    auto list_entries = FlatVector::GetData<list_entry_t>(list_vector);
    
    // Create child vector to hold the semantic type values
    auto list_size = searchable_types.size();
    ListVector::Reserve(list_vector, list_size * count);
    auto &child_vector = ListVector::GetEntry(list_vector);
    auto child_data = FlatVector::GetData<uint8_t>(child_vector);
    
    idx_t offset = 0;
    for (idx_t i = 0; i < count; i++) {
        list_entries[i].offset = offset;
        list_entries[i].length = list_size;
        
        // Copy semantic types to child vector
        for (idx_t j = 0; j < list_size; j++) {
            child_data[offset + j] = searchable_types[j];
        }
        offset += list_size;
    }
    
    ListVector::SetListSize(list_vector, offset);
}

void RegisterSemanticTypeFunctions(DatabaseInstance &instance) {
    // Register semantic_type_to_string(semantic_type) -> VARCHAR
    ScalarFunction semantic_type_to_string_func("semantic_type_to_string", 
        {LogicalType::UTINYINT}, LogicalType::VARCHAR, SemanticTypeToStringFunction);
    ExtensionUtil::RegisterFunction(instance, semantic_type_to_string_func);
    
    // Register get_super_kind(semantic_type) -> VARCHAR
    ScalarFunction get_super_kind_func("get_super_kind",
        {LogicalType::UTINYINT}, LogicalType::VARCHAR, GetSuperKindFunction);
    ExtensionUtil::RegisterFunction(instance, get_super_kind_func);
    
    // Register get_kind(semantic_type) -> VARCHAR
    ScalarFunction get_kind_func("get_kind",
        {LogicalType::UTINYINT}, LogicalType::VARCHAR, GetKindFunction);
    ExtensionUtil::RegisterFunction(instance, get_kind_func);
    
    // Register is_semantic_type(semantic_type, pattern) -> BOOLEAN
    ScalarFunction is_semantic_type_func("is_semantic_type",
        {LogicalType::UTINYINT, LogicalType::VARCHAR}, LogicalType::BOOLEAN, IsSemanticTypeFunction);
    ExtensionUtil::RegisterFunction(instance, is_semantic_type_func);
    
    // Register semantic_type_code(name) -> UTINYINT
    ScalarFunction semantic_type_code_func("semantic_type_code",
        {LogicalType::VARCHAR}, LogicalType::UTINYINT, SemanticTypeCodeFunction);
    ExtensionUtil::RegisterFunction(instance, semantic_type_code_func);
    
    // Register predicate functions
    ScalarFunction is_definition_func("is_definition",
        {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsDefinitionFunction);
    ExtensionUtil::RegisterFunction(instance, is_definition_func);
    
    ScalarFunction is_call_func("is_call",
        {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsCallFunction);
    ExtensionUtil::RegisterFunction(instance, is_call_func);
    
    ScalarFunction is_control_flow_func("is_control_flow",
        {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsControlFlowFunction);
    ExtensionUtil::RegisterFunction(instance, is_control_flow_func);
    
    ScalarFunction is_identifier_func("is_identifier",
        {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsIdentifierFunction);
    ExtensionUtil::RegisterFunction(instance, is_identifier_func);
    
    // Register get_searchable_types() -> LIST<UTINYINT>
    ScalarFunction get_searchable_types_func("get_searchable_types",
        {}, LogicalType::LIST(LogicalType::UTINYINT), GetSearchableTypesFunction);
    ExtensionUtil::RegisterFunction(instance, get_searchable_types_func);
    
    // Register kind_code(name) -> UTINYINT
    ScalarFunction kind_code_func("kind_code",
        {LogicalType::VARCHAR}, LogicalType::UTINYINT, KindCodeFunction);
    ExtensionUtil::RegisterFunction(instance, kind_code_func);
    
    // Register is_kind(semantic_type, kind_name) -> BOOLEAN
    ScalarFunction is_kind_func("is_kind",
        {LogicalType::UTINYINT, LogicalType::VARCHAR}, LogicalType::BOOLEAN, IsKindFunction);
    ExtensionUtil::RegisterFunction(instance, is_kind_func);
}

} // namespace duckdb