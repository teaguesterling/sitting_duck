#pragma once

#include "duckdb.hpp"
#include "ast_type.hpp"
#include <unordered_map>
#include <vector>

namespace duckdb {

// Hash generation method enum (replaces std::variant for C++14 compatibility)
enum class HashMethodType : uint8_t {
	STRUCTURAL = 0,   // No content-based hash
	LITERAL = 1,      // Hash the literal value
	SINGLE_VALUE = 2, // Hash based on property values
	ANNOTATED = 3,    // Hash with annotations (e.g., HTML)
	CUSTOM = 4        // Language-specific custom logic
};

// Hash generation configuration
struct HashMethod {
	HashMethodType type;
	vector<string> properties; // For SINGLE_VALUE and ANNOTATED
	vector<string> attributes; // For ANNOTATED only
	string custom_id;          // For CUSTOM only

	// Constructor
	HashMethod(HashMethodType t = HashMethodType::STRUCTURAL, const vector<string> &props = vector<string>(),
	           const vector<string> &attrs = vector<string>(), const string &id = string())
	    : type(t), properties(props), attributes(attrs), custom_id(id) {
	}

	// Factory methods
	static HashMethod Structural() {
		return HashMethod(HashMethodType::STRUCTURAL);
	}

	static HashMethod Literal() {
		return HashMethod(HashMethodType::LITERAL);
	}

	static HashMethod SingleValue(const vector<string> &props) {
		return HashMethod(HashMethodType::SINGLE_VALUE, props);
	}

	static HashMethod Annotated(const vector<string> &props, const vector<string> &attrs) {
		return HashMethod(HashMethodType::ANNOTATED, props, attrs);
	}

	static HashMethod Custom(const string &id) {
		return HashMethod(HashMethodType::CUSTOM, vector<string>(), vector<string>(), id);
	}
};

// Configuration for a specific node type
struct NodeTypeConfig {
	ASTKind kind;            // Semantic category (0-15)
	uint8_t super_type;      // Built-in super type within KIND (0-3)
	uint8_t parser_specific; // Parser-specific type bits (0-7)
	HashMethod hash_method;  // How to generate unique hash
	uint8_t universal_flags; // Computed based on node type

	// Constructor
	NodeTypeConfig(ASTKind k = ASTKind::PARSER_SPECIFIC, uint8_t super = 0, uint8_t parser = 0,
	               const HashMethod &hash = HashMethod::Structural(), uint8_t flags = 0)
	    : kind(k), super_type(super), parser_specific(parser), hash_method(hash), universal_flags(flags) {
	}

	// Helpers for common patterns
	static NodeTypeConfig Definition(uint8_t super_type, const HashMethod &hash) {
		return NodeTypeConfig(ASTKind::DEFINITION, super_type, 0, hash, 0);
	}

	static NodeTypeConfig Computation(uint8_t super_type, const HashMethod &hash) {
		return NodeTypeConfig(ASTKind::COMPUTATION, super_type, 0, hash, 0);
	}

	static NodeTypeConfig Name(uint8_t super_type, const HashMethod &hash) {
		return NodeTypeConfig(ASTKind::NAME, super_type, 0, hash, 0);
	}

	static NodeTypeConfig Organization(uint8_t super_type) {
		return NodeTypeConfig(ASTKind::ORGANIZATION, super_type, 0, HashMethod::Structural(), 0);
	}
};

// Language-specific configuration
class LanguageConfig {
public:
	using ConfigMap = unordered_map<string, NodeTypeConfig>;

	LanguageConfig() = default;

	// Add a node type configuration
	void AddNodeType(const string &type_name, const NodeTypeConfig &config) {
		node_configs[type_name] = config;
	}

	// Add multiple node types with same config
	void AddNodeTypes(const vector<string> &type_names, const NodeTypeConfig &config) {
		for (const auto &name : type_names) {
			node_configs[name] = config;
		}
	}

	// Get configuration for a node type
	const NodeTypeConfig *GetNodeConfig(const string &type_name) const {
		auto it = node_configs.find(type_name);
		if (it != node_configs.end()) {
			return &it->second;
		}

		// Check default patterns
		if (StringUtil::EndsWith(type_name, "_declaration") || StringUtil::EndsWith(type_name, "_definition")) {
			return &default_definition;
		}
		if (StringUtil::EndsWith(type_name, "_expression")) {
			return &default_expression;
		}
		if (StringUtil::EndsWith(type_name, "_statement")) {
			return &default_statement;
		}
		if (type_name == "identifier" || StringUtil::EndsWith(type_name, "_identifier")) {
			return &default_identifier;
		}

		return &default_unknown;
	}

	// Set language-specific defaults
	void SetDefaults(const NodeTypeConfig &definition, const NodeTypeConfig &expression,
	                 const NodeTypeConfig &statement, const NodeTypeConfig &identifier, const NodeTypeConfig &unknown) {
		default_definition = definition;
		default_expression = expression;
		default_statement = statement;
		default_identifier = identifier;
		default_unknown = unknown;
	}

private:
	ConfigMap node_configs;

	// Default configurations for common patterns
	NodeTypeConfig default_definition = NodeTypeConfig(ASTKind::DEFINITION, 0, 0, HashMethod::Structural(), 0);
	NodeTypeConfig default_expression = NodeTypeConfig(ASTKind::COMPUTATION, 0, 0, HashMethod::Structural(), 0);
	NodeTypeConfig default_statement = NodeTypeConfig(ASTKind::EXECUTION, 0, 0, HashMethod::Structural(), 0);
	NodeTypeConfig default_identifier = NodeTypeConfig(ASTKind::NAME, 1, 0, HashMethod::Literal(), 0);
	NodeTypeConfig default_unknown = NodeTypeConfig(ASTKind::PARSER_SPECIFIC, 0, 0, HashMethod::Structural(), 0);
};

// Factory functions for language configurations
unique_ptr<LanguageConfig> CreateJavaScriptConfig();
unique_ptr<LanguageConfig> CreatePythonConfig();
unique_ptr<LanguageConfig> CreateCppConfig();
unique_ptr<LanguageConfig> CreateRustConfig();
unique_ptr<LanguageConfig> CreateSQLConfig();
unique_ptr<LanguageConfig> CreateHTMLConfig();

} // namespace duckdb
