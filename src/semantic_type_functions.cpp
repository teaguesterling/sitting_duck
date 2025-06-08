#include "duckdb.hpp"
#include "duckdb/main/extension_util.hpp"
#include "include/semantic_types.hpp"

namespace duckdb {

// Scalar function that converts semantic_type integer to human-readable string
static void SemanticTypeToStringFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<int8_t, string_t>(
        semantic_type_vector, result, count,
        [&](int8_t semantic_type) {
            string type_name = SemanticTypes::GetSemanticTypeName(static_cast<uint8_t>(semantic_type));
            return StringVector::AddString(result, type_name);
        }
    );
}

// Function that gets the super kind name from semantic type
static void GetSuperKindFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<int8_t, string_t>(
        semantic_type_vector, result, count,
        [&](int8_t semantic_type) {
            uint8_t super_kind = SemanticTypes::GetSuperKind(static_cast<uint8_t>(semantic_type));
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
    
    UnaryExecutor::Execute<int8_t, string_t>(
        semantic_type_vector, result, count,
        [&](int8_t semantic_type) {
            uint8_t kind = SemanticTypes::GetKind(static_cast<uint8_t>(semantic_type));
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
    
    BinaryExecutor::Execute<int8_t, string_t, bool>(
        semantic_type_vector, pattern_vector, result, count,
        [&](int8_t semantic_type, string_t pattern_str) {
            string pattern = pattern_str.GetString();
            uint8_t type = static_cast<uint8_t>(semantic_type);
            
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

void RegisterSemanticTypeFunctions(DatabaseInstance &instance) {
    // Register semantic_type_to_string(semantic_type) -> VARCHAR
    ScalarFunction semantic_type_to_string_func("semantic_type_to_string", 
        {LogicalType::TINYINT}, LogicalType::VARCHAR, SemanticTypeToStringFunction);
    ExtensionUtil::RegisterFunction(instance, semantic_type_to_string_func);
    
    // Register get_super_kind(semantic_type) -> VARCHAR
    ScalarFunction get_super_kind_func("get_super_kind",
        {LogicalType::TINYINT}, LogicalType::VARCHAR, GetSuperKindFunction);
    ExtensionUtil::RegisterFunction(instance, get_super_kind_func);
    
    // Register get_kind(semantic_type) -> VARCHAR
    ScalarFunction get_kind_func("get_kind",
        {LogicalType::TINYINT}, LogicalType::VARCHAR, GetKindFunction);
    ExtensionUtil::RegisterFunction(instance, get_kind_func);
    
    // Register is_semantic_type(semantic_type, pattern) -> BOOLEAN
    ScalarFunction is_semantic_type_func("is_semantic_type",
        {LogicalType::TINYINT, LogicalType::VARCHAR}, LogicalType::BOOLEAN, IsSemanticTypeFunction);
    ExtensionUtil::RegisterFunction(instance, is_semantic_type_func);
}

} // namespace duckdb