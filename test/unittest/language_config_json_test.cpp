#include "catch.hpp"
#include "language_config_json.hpp"
#include "semantic_types.hpp"

using namespace duckdb;

// Canonical sample config, mirrors test/data/dynamic_grammars/jsond_types.json.
static const char *JSOND_CONFIG = R"JSON({
  "language": "jsond",
  "node_types": {
    "document": {
      "semantic_type": "DEFINITION_MODULE",
      "flags": ["IS_SCOPE"]
    },
    "object": {
      "semantic_type": "LITERAL_STRUCTURED",
      "refinement": 2
    },
    "array": {
      "semantic_type": "LITERAL_STRUCTURED",
      "refinement": 1
    },
    "pair": {
      "semantic_type": "DEFINITION_VARIABLE",
      "name_strategy": "FIRST_CHILD",
      "flags": ["NAME_DEFINITION"]
    },
    "string": {
      "semantic_type": "LITERAL_STRING",
      "name_strategy": "NODE_TEXT"
    },
    "number": {
      "semantic_type": "LITERAL_NUMBER",
      "name_strategy": "NODE_TEXT"
    },
    "true": {
      "semantic_type": "LITERAL_ATOMIC",
      "name_strategy": "NODE_TEXT"
    },
    "false": {
      "semantic_type": "LITERAL_ATOMIC",
      "name_strategy": "NODE_TEXT"
    },
    "null": {
      "semantic_type": "LITERAL_ATOMIC",
      "name_strategy": "NODE_TEXT"
    }
  }
})JSON";

TEST_CASE("ParseLanguageConfigJSON parses the canonical jsond config", "[language_config_json]") {
	auto configs = ParseLanguageConfigJSON(JSOND_CONFIG, "jsond");

	REQUIRE(configs.size() == 9);

	SECTION("pair entry: definition variable with FIRST_CHILD and NAME_DEFINITION") {
		auto it = configs.find("pair");
		REQUIRE(it != configs.end());
		const NodeConfig &pair = it->second;
		REQUIRE(pair.semantic_type == SemanticTypes::GetSemanticTypeCode("DEFINITION_VARIABLE"));
		REQUIRE(pair.name_strategy == ExtractionStrategy::FIRST_CHILD);
		REQUIRE((pair.flags & ASTNodeFlags::NAME_DEFINITION) == ASTNodeFlags::NAME_DEFINITION);
	}

	SECTION("array entry: structured literal with refinement 1 OR'd in") {
		auto it = configs.find("array");
		REQUIRE(it != configs.end());
		uint8_t expected = (uint8_t)(SemanticTypes::GetSemanticTypeCode("LITERAL_STRUCTURED") | 1);
		REQUIRE(it->second.semantic_type == expected);
	}

	SECTION("document entry: has IS_SCOPE flag") {
		auto it = configs.find("document");
		REQUIRE(it != configs.end());
		REQUIRE((it->second.flags & ASTNodeFlags::IS_SCOPE) == ASTNodeFlags::IS_SCOPE);
		REQUIRE(it->second.semantic_type == SemanticTypes::GetSemanticTypeCode("DEFINITION_MODULE"));
	}

	SECTION("native strategy defaults to NONE") {
		REQUIRE(configs.at("pair").native_strategy == NativeExtractionStrategy::NONE);
	}
}

TEST_CASE("ParseLanguageConfigJSON accepts a config without the language field", "[language_config_json]") {
	const char *json = R"JSON({
  "node_types": {
    "document": { "semantic_type": "DEFINITION_MODULE" }
  }
})JSON";
	auto configs = ParseLanguageConfigJSON(json, "anything");
	REQUIRE(configs.size() == 1);
	REQUIRE(configs.count("document") == 1);
}

TEST_CASE("ParseLanguageConfigJSON coerces native_strategy to NONE", "[language_config_json]") {
	SECTION("valid native_strategy name is accepted but stored as NONE") {
		const char *json = R"JSON({
  "node_types": {
    "func": {
      "semantic_type": "DEFINITION_FUNCTION",
      "native_strategy": "FUNCTION_WITH_PARAMS"
    }
  }
})JSON";
		auto configs = ParseLanguageConfigJSON(json, "lang");
		REQUIRE(configs.at("func").native_strategy == NativeExtractionStrategy::NONE);
	}

	SECTION("unknown native_strategy name errors") {
		const char *json = R"JSON({
  "node_types": {
    "func": {
      "semantic_type": "DEFINITION_FUNCTION",
      "native_strategy": "NOT_A_REAL_STRATEGY"
    }
  }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "lang"), InvalidInputException);
	}
}

TEST_CASE("ParseLanguageConfigJSON rejects invalid configs", "[language_config_json]") {
	SECTION("language mismatch") {
		const char *json = R"JSON({
  "language": "foo",
  "node_types": { "x": { "semantic_type": "DEFINITION_MODULE" } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "bar"), InvalidInputException);
	}

	SECTION("missing node_types") {
		const char *json = R"JSON({ "language": "foo" })JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("unknown top-level key") {
		const char *json = R"JSON({
  "node_types": { "x": { "semantic_type": "DEFINITION_MODULE" } },
  "extra": true
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("unknown per-node key") {
		const char *json = R"JSON({
  "node_types": { "x": { "semantic_type": "DEFINITION_MODULE", "bogus": 1 } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("missing semantic_type") {
		const char *json = R"JSON({
  "node_types": { "x": { "name_strategy": "NODE_TEXT" } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("unknown semantic_type name") {
		const char *json = R"JSON({
  "node_types": { "x": { "semantic_type": "NOT_A_TYPE" } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("unknown name_strategy") {
		const char *json = R"JSON({
  "node_types": { "x": { "semantic_type": "DEFINITION_MODULE", "name_strategy": "MADE_UP" } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("CUSTOM name_strategy is explicitly rejected") {
		const char *json = R"JSON({
  "node_types": { "x": { "semantic_type": "DEFINITION_MODULE", "name_strategy": "CUSTOM" } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("unknown flag name") {
		const char *json = R"JSON({
  "node_types": { "x": { "semantic_type": "DEFINITION_MODULE", "flags": ["NOT_A_FLAG"] } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("refinement out of range (4)") {
		const char *json = R"JSON({
  "node_types": { "x": { "semantic_type": "LITERAL_STRUCTURED", "refinement": 4 } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("refinement out of range (-1)") {
		const char *json = R"JSON({
  "node_types": { "x": { "semantic_type": "LITERAL_STRUCTURED", "refinement": -1 } }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("malformed JSON") {
		const char *json = R"JSON({ "node_types": { "x": )JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("top-level array instead of object") {
		const char *json = R"JSON([1, 2, 3])JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}

	SECTION("node entry is not an object") {
		const char *json = R"JSON({
  "node_types": { "x": "DEFINITION_MODULE" }
})JSON";
		REQUIRE_THROWS_AS(ParseLanguageConfigJSON(json, "foo"), InvalidInputException);
	}
}
