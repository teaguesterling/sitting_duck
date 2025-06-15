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

// Function that converts semantic type name to code
static void SemanticTypeCodeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &name_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<string_t, int8_t>(
        name_vector, result, count,
        [&](string_t name_str) {
            string name = name_str.GetString();
            uint8_t code = SemanticTypes::GetSemanticTypeCode(name);
            return static_cast<int8_t>(code);
        }
    );
}

// Predicate functions for common type categories
static void IsDefinitionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<int8_t, bool>(
        semantic_type_vector, result, count,
        [&](int8_t semantic_type) {
            return SemanticTypes::IsDefinition(static_cast<uint8_t>(semantic_type));
        }
    );
}

static void IsCallFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<int8_t, bool>(
        semantic_type_vector, result, count,
        [&](int8_t semantic_type) {
            return SemanticTypes::IsCall(static_cast<uint8_t>(semantic_type));
        }
    );
}

static void IsControlFlowFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<int8_t, bool>(
        semantic_type_vector, result, count,
        [&](int8_t semantic_type) {
            return SemanticTypes::IsControlFlow(static_cast<uint8_t>(semantic_type));
        }
    );
}

static void IsIdentifierFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    D_ASSERT(args.ColumnCount() == 1);
    
    auto &semantic_type_vector = args.data[0];
    auto count = args.size();
    
    UnaryExecutor::Execute<int8_t, bool>(
        semantic_type_vector, result, count,
        [&](int8_t semantic_type) {
            return SemanticTypes::IsIdentifier(static_cast<uint8_t>(semantic_type));
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
    auto child_data = FlatVector::GetData<int8_t>(child_vector);
    
    idx_t offset = 0;
    for (idx_t i = 0; i < count; i++) {
        list_entries[i].offset = offset;
        list_entries[i].length = list_size;
        
        // Copy semantic types to child vector
        for (idx_t j = 0; j < list_size; j++) {
            child_data[offset + j] = static_cast<int8_t>(searchable_types[j]);
        }
        offset += list_size;
    }
    
    ListVector::SetListSize(list_vector, offset);
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
    
    // Register semantic_type_code(name) -> TINYINT
    ScalarFunction semantic_type_code_func("semantic_type_code",
        {LogicalType::VARCHAR}, LogicalType::TINYINT, SemanticTypeCodeFunction);
    ExtensionUtil::RegisterFunction(instance, semantic_type_code_func);
    
    // Register predicate functions
    ScalarFunction is_definition_func("is_definition",
        {LogicalType::TINYINT}, LogicalType::BOOLEAN, IsDefinitionFunction);
    ExtensionUtil::RegisterFunction(instance, is_definition_func);
    
    ScalarFunction is_call_func("is_call",
        {LogicalType::TINYINT}, LogicalType::BOOLEAN, IsCallFunction);
    ExtensionUtil::RegisterFunction(instance, is_call_func);
    
    ScalarFunction is_control_flow_func("is_control_flow",
        {LogicalType::TINYINT}, LogicalType::BOOLEAN, IsControlFlowFunction);
    ExtensionUtil::RegisterFunction(instance, is_control_flow_func);
    
    ScalarFunction is_identifier_func("is_identifier",
        {LogicalType::TINYINT}, LogicalType::BOOLEAN, IsIdentifierFunction);
    ExtensionUtil::RegisterFunction(instance, is_identifier_func);
    
    // Register get_searchable_types() -> LIST<TINYINT>
    ScalarFunction get_searchable_types_func("get_searchable_types",
        {}, LogicalType::LIST(LogicalType::TINYINT), GetSearchableTypesFunction);
    ExtensionUtil::RegisterFunction(instance, get_searchable_types_func);
}

} // namespace duckdb