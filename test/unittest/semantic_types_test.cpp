#include "catch.hpp"
#include "semantic_types.hpp"
#include <unordered_set>

using namespace duckdb;
using namespace duckdb::SemanticTypes;

TEST_CASE("Semantic type name conversions", "[semantic_types]") {
    SECTION("TypeName returns correct names for valid codes") {
        REQUIRE(TypeName(240) == "DEFINITION_FUNCTION");
        REQUIRE(TypeName(248) == "DEFINITION_CLASS");
        REQUIRE(TypeName(208) == "COMPUTATION_CALL");
        REQUIRE(TypeName(84) == "NAME_IDENTIFIER");
        REQUIRE(TypeName(0) == "PARSER_CONSTRUCT");  // New: 0 is now PARSER_CONSTRUCT
    }
    
    SECTION("TypeName returns UNKNOWN for invalid codes") {
        REQUIRE(TypeName(255) == "UNKNOWN_SEMANTIC_TYPE");
        REQUIRE(TypeName(127) == "UNKNOWN_SEMANTIC_TYPE");
    }
    
    SECTION("TypeCode returns correct codes for valid names") {
        REQUIRE(TypeCode("DEFINITION_FUNCTION") == 240);
        REQUIRE(TypeCode("DEFINITION_CLASS") == 248);
        REQUIRE(TypeCode("COMPUTATION_CALL") == 208);
        REQUIRE(TypeCode("NAME_IDENTIFIER") == 84);
        REQUIRE(TypeCode("PARSER_CONSTRUCT") == 0);  // New: PARSER_CONSTRUCT is now 0
    }
    
    SECTION("TypeCode returns 255 for invalid names") {
        REQUIRE(TypeCode("INVALID_TYPE") == 255);
        REQUIRE(TypeCode("") == 255);
    }
}

TEST_CASE("Semantic type round-trip conversions", "[semantic_types]") {
    // Test all valid semantic types
    std::vector<uint8_t> all_types = {
        // PARSER_SPECIFIC types
        0, 4, 8, 12,
        // RESERVED types  
        16, 20, 24, 28,
        // METADATA types
        32, 36, 40, 44,
        // EXTERNAL types
        48, 52, 56, 60,
        // LITERAL types
        64, 68, 72, 76,
        // NAME types
        80, 84, 88, 92,
        // PATTERN types
        96, 100, 104, 108,
        // TYPE types
        112, 116, 120, 124,
        // EXECUTION types
        128, 132, 136, 140,
        // FLOW_CONTROL types
        144, 148, 152, 156,
        // ERROR_HANDLING types
        160, 164, 168, 172,
        // ORGANIZATION types
        176, 180, 184, 188,
        // OPERATOR types
        192, 196, 200, 204,
        // COMPUTATION_NODE types
        208, 212, 216, 220,
        // TRANSFORM types
        224, 228, 232, 236,
        // DEFINITION types
        240, 244, 248, 252
    };
    
    SECTION("All types round-trip correctly") {
        for (uint8_t type : all_types) {
            string name = TypeName(type);
            REQUIRE(name != "UNKNOWN_SEMANTIC_TYPE");
            REQUIRE(TypeCode(name) == type);
        }
    }
}

TEST_CASE("Semantic type predicates", "[semantic_types]") {
    SECTION("IsDefinition predicate") {
        REQUIRE(IsDefinition(240)); // DEFINITION_FUNCTION
        REQUIRE(IsDefinition(244)); // DEFINITION_VARIABLE
        REQUIRE(IsDefinition(248)); // DEFINITION_CLASS
        REQUIRE(IsDefinition(252)); // DEFINITION_MODULE
        
        REQUIRE(!IsDefinition(208));  // COMPUTATION_CALL
        REQUIRE(!IsDefinition(84));  // NAME_IDENTIFIER
    }
    
    SECTION("IsCall predicate") {
        REQUIRE(IsCall(208));  // COMPUTATION_CALL
        REQUIRE(IsCall(136)); // EXECUTION_STATEMENT_CALL
        
        REQUIRE(!IsCall(240)); // DEFINITION_FUNCTION
        REQUIRE(!IsCall(84));  // NAME_IDENTIFIER
    }
    
    SECTION("IsControlFlow predicate") {
        REQUIRE(IsControlFlow(144)); // FLOW_CONDITIONAL
        REQUIRE(IsControlFlow(148)); // FLOW_LOOP
        REQUIRE(IsControlFlow(152)); // FLOW_JUMP
        REQUIRE(IsControlFlow(156)); // FLOW_SYNC
        
        REQUIRE(!IsControlFlow(112)); // DEFINITION_FUNCTION
        REQUIRE(!IsControlFlow(80));  // COMPUTATION_CALL
    }
    
    SECTION("IsIdentifier predicate") {
        REQUIRE(IsIdentifier(20)); // NAME_IDENTIFIER
        REQUIRE(IsIdentifier(24)); // NAME_QUALIFIED
        REQUIRE(IsIdentifier(28)); // NAME_SCOPED
        
        REQUIRE(!IsIdentifier(16)); // NAME_KEYWORD
        REQUIRE(!IsIdentifier(112)); // DEFINITION_FUNCTION
    }
    
    SECTION("IsLiteral predicate") {
        REQUIRE(IsLiteral(0));  // LITERAL_NUMBER
        REQUIRE(IsLiteral(4));  // LITERAL_STRING
        REQUIRE(IsLiteral(8));  // LITERAL_ATOMIC
        REQUIRE(IsLiteral(12)); // LITERAL_STRUCTURED
        
        REQUIRE(!IsLiteral(20));  // NAME_IDENTIFIER
        REQUIRE(!IsLiteral(112)); // DEFINITION_FUNCTION
    }
    
    SECTION("IsOperator predicate") {
        REQUIRE(IsOperator(64)); // OPERATOR_ARITHMETIC
        REQUIRE(IsOperator(68)); // OPERATOR_LOGICAL
        REQUIRE(IsOperator(72)); // OPERATOR_COMPARISON
        REQUIRE(IsOperator(76)); // OPERATOR_ASSIGNMENT
        
        REQUIRE(!IsOperator(80));  // COMPUTATION_CALL
        REQUIRE(!IsOperator(112)); // DEFINITION_FUNCTION
    }
}

TEST_CASE("Semantic type collections", "[semantic_types]") {
    SECTION("GetDefinitionTypes returns all definition types") {
        auto types = GetDefinitionTypes();
        REQUIRE(types.size() == 4);
        REQUIRE(std::find(types.begin(), types.end(), 112) != types.end()); // DEFINITION_FUNCTION
        REQUIRE(std::find(types.begin(), types.end(), 116) != types.end()); // DEFINITION_VARIABLE
        REQUIRE(std::find(types.begin(), types.end(), 120) != types.end()); // DEFINITION_CLASS
        REQUIRE(std::find(types.begin(), types.end(), 124) != types.end()); // DEFINITION_MODULE
    }
    
    SECTION("GetControlFlowTypes returns all control flow types") {
        auto types = GetControlFlowTypes();
        REQUIRE(types.size() == 4);
        REQUIRE(std::find(types.begin(), types.end(), 144) != types.end()); // FLOW_CONDITIONAL
        REQUIRE(std::find(types.begin(), types.end(), 148) != types.end()); // FLOW_LOOP
        REQUIRE(std::find(types.begin(), types.end(), 152) != types.end()); // FLOW_JUMP
        REQUIRE(std::find(types.begin(), types.end(), 156) != types.end()); // FLOW_SYNC
    }
    
    SECTION("GetSearchableTypes returns reasonable set") {
        auto types = GetSearchableTypes();
        REQUIRE(types.size() > 10);
        
        // Should include all definitions
        REQUIRE(std::find(types.begin(), types.end(), 112) != types.end()); // DEFINITION_FUNCTION
        REQUIRE(std::find(types.begin(), types.end(), 120) != types.end()); // DEFINITION_CLASS
        
        // Should include calls
        REQUIRE(std::find(types.begin(), types.end(), 80) != types.end()); // COMPUTATION_CALL
        
        // Should include imports/exports
        REQUIRE(std::find(types.begin(), types.end(), 208) != types.end()); // EXTERNAL_IMPORT
        REQUIRE(std::find(types.begin(), types.end(), 212) != types.end()); // EXTERNAL_EXPORT
        
        // Should not have duplicates
        std::unordered_set<uint8_t> unique_types(types.begin(), types.end());
        REQUIRE(unique_types.size() == types.size());
    }
}

TEST_CASE("Semantic type bit manipulation", "[semantic_types]") {
    SECTION("GetSuperKind extracts correct bits") {
        REQUIRE(GetSuperKind(112) == COMPUTATION);     // DEFINITION_FUNCTION
        REQUIRE(GetSuperKind(20) == DATA_STRUCTURE);   // NAME_IDENTIFIER
        REQUIRE(GetSuperKind(144) == CONTROL_EFFECTS); // FLOW_CONDITIONAL
        REQUIRE(GetSuperKind(192) == META_EXTERNAL);   // METADATA_COMMENT
    }
    
    SECTION("GetKind extracts correct bits") {
        REQUIRE(GetKind(112) == DEFINITION);      // DEFINITION_FUNCTION
        REQUIRE(GetKind(80) == COMPUTATION_NODE); // COMPUTATION_CALL
        REQUIRE(GetKind(144) == FLOW_CONTROL);    // FLOW_CONDITIONAL
        REQUIRE(GetKind(20) == NAME);             // NAME_IDENTIFIER
    }
    
    SECTION("Super kind names are correct") {
        REQUIRE(GetSuperKindName(DATA_STRUCTURE) == "DATA_STRUCTURE");
        REQUIRE(GetSuperKindName(COMPUTATION) == "COMPUTATION");
        REQUIRE(GetSuperKindName(CONTROL_EFFECTS) == "CONTROL_EFFECTS");
        REQUIRE(GetSuperKindName(META_EXTERNAL) == "META_EXTERNAL");
        REQUIRE(GetSuperKindName(255) == "UNKNOWN_SUPER_KIND");
    }
    
    SECTION("Kind names are correct") {
        REQUIRE(GetKindName(LITERAL) == "LITERAL");
        REQUIRE(GetKindName(NAME) == "NAME");
        REQUIRE(GetKindName(DEFINITION) == "DEFINITION");
        REQUIRE(GetKindName(FLOW_CONTROL) == "FLOW_CONTROL");
        REQUIRE(GetKindName(255) == "UNKNOWN_KIND");
    }
}