#include "node_type_config.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

// JavaScript/TypeScript configuration
unique_ptr<LanguageConfig> CreateJavaScriptConfig() {
	auto config = make_uniq<LanguageConfig>();

	// Set reasonable defaults for JavaScript
	config->SetDefaults(
	    // definition: functions, classes, variables
	    {ASTKind::DEFINITION, 0, 0, HashMethodSingleValue {{"name"}}, 0},
	    // expression: computations and calls
	    {ASTKind::COMPUTATION, 0, 0, HashMethodStructural {}, 0},
	    // statement: execution and side effects
	    {ASTKind::EXECUTION, 0, 0, HashMethodStructural {}, 0},
	    // identifier: name references
	    {ASTKind::NAME, 1, 0, HashMethodLiteral {}, 0},
	    // unknown: parser-specific
	    {ASTKind::PARSER_SPECIFIC, 0, 0, HashMethodStructural {}, 0});

	// Function definitions
	config->AddNodeType("function_declaration", NodeTypeConfig::Definition(0, HashMethodSingleValue {{"name"}}));
	config->AddNodeType("function_expression", NodeTypeConfig::Definition(0, HashMethodSingleValue {{"name"}}));
	config->AddNodeType("arrow_function", NodeTypeConfig::Definition(0, HashMethodStructural {})); // Often anonymous
	config->AddNodeType("method_definition", NodeTypeConfig::Definition(0, HashMethodSingleValue {{"name"}}));

	// Variable/constant definitions
	config->AddNodeTypes({"variable_declaration", "lexical_declaration"},
	                     {ASTKind::DEFINITION, 1, 0, HashMethodStructural {}, 0});
	config->AddNodeType("variable_declarator", NodeTypeConfig::Definition(1, HashMethodSingleValue {{"name"}}));

	// Class/object definitions
	config->AddNodeType("class_declaration", NodeTypeConfig::Definition(2, HashMethodSingleValue {{"name"}}));
	config->AddNodeType("class_expression", NodeTypeConfig::Definition(2, HashMethodSingleValue {{"name"}}));

	// Computations and calls
	config->AddNodeType("call_expression",
	                    NodeTypeConfig::Computation(0, HashMethodSingleValue {{"function.name", "function"}}));
	config->AddNodeType("new_expression",
	                    NodeTypeConfig::Computation(0, HashMethodSingleValue {{"constructor.name", "constructor"}}));
	config->AddNodeType("member_expression",
	                    NodeTypeConfig::Computation(1, HashMethodSingleValue {{"property.name", "property"}}));

	// Operators
	config->AddNodeTypes({"binary_expression", "logical_expression"},
	                     {ASTKind::OPERATOR, 0, 0, HashMethodSingleValue {{"operator"}}, 0});
	config->AddNodeTypes({"unary_expression", "update_expression"},
	                     {ASTKind::OPERATOR, 3, 0, HashMethodSingleValue {{"operator"}}, 0});
	config->AddNodeType("assignment_expression", {ASTKind::EXECUTION, 0, 0, HashMethodSingleValue {{"operator"}}, 0});

	// Literals
	config->AddNodeTypes({"number", "string", "template_string"}, {ASTKind::LITERAL, 0, 0, HashMethodLiteral {}, 0});
	config->AddNodeTypes({"true", "false", "null", "undefined"},
	                     {ASTKind::LITERAL, 2, 0, HashMethodLiteral {}, 0x05}); // is_keyword | is_builtin
	config->AddNodeTypes({"array", "object"}, {ASTKind::LITERAL, 3, 0, HashMethodStructural {}, 0});

	// Control flow
	config->AddNodeType("if_statement", {ASTKind::FLOW_CONTROL, 0, 0, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeTypes({"for_statement", "for_in_statement", "for_of_statement", "while_statement", "do_statement"},
	                     {ASTKind::FLOW_CONTROL, 1, 0, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeTypes({"switch_statement", "switch_case"},
	                     {ASTKind::FLOW_CONTROL, 0, 0, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeTypes({"break_statement", "continue_statement", "return_statement"},
	                     {ASTKind::FLOW_CONTROL, 3, 0, HashMethodStructural {}, 0x01}); // is_keyword

	// Error handling
	config->AddNodeType("try_statement", {ASTKind::ERROR_HANDLING, 0, 0, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeType("catch_clause",
	                    {ASTKind::ERROR_HANDLING, 0, 0, HashMethodSingleValue {{"parameter.name"}}, 0x01});
	config->AddNodeType("throw_statement",
	                    {ASTKind::ERROR_HANDLING, 1, 0, HashMethodStructural {}, 0x01}); // is_keyword

	// Organization
	config->AddNodeTypes({"program", "statement_block", "block"},
	                     {ASTKind::ORGANIZATION, 0, 0, HashMethodStructural {}, 0});
	config->AddNodeType("export_statement", {ASTKind::EXTERNAL, 0, 0, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeType("import_statement",
	                    {ASTKind::EXTERNAL, 0, 0, HashMethodSingleValue {{"source"}}, 0x01}); // is_keyword

	// Comments and metadata
	config->AddNodeType("comment", {ASTKind::METADATA, 0, 0, HashMethodLiteral {}, 0});

	// Punctuation
	config->AddNodeTypes({"{", "}", "(", ")", "[", "]", ";", ",", ".", ":", "=>"},
	                     {ASTKind::ORGANIZATION, 0, 0, HashMethodStructural {}, 0x02}); // is_punctuation

	// Keywords that appear as nodes
	config->AddNodeTypes({"async", "await", "const", "let", "var", "function", "class", "extends", "static", "get",
	                      "set", "new", "this", "super"},
	                     {ASTKind::NAME, 0, 0, HashMethodLiteral {}, 0x05}); // is_keyword | is_builtin

	return config;
}

// Python configuration
unique_ptr<LanguageConfig> CreatePythonConfig() {
	auto config = make_uniq<LanguageConfig>();

	// Set Python-specific defaults
	config->SetDefaults({ASTKind::DEFINITION, 0, 0, HashMethodSingleValue {{"name"}}, 0},
	                    {ASTKind::COMPUTATION, 0, 0, HashMethodStructural {}, 0},
	                    {ASTKind::EXECUTION, 0, 0, HashMethodStructural {}, 0},
	                    {ASTKind::NAME, 1, 0, HashMethodLiteral {}, 0},
	                    {ASTKind::PARSER_SPECIFIC, 0, 0, HashMethodStructural {}, 0});

	// Function definitions
	config->AddNodeType("function_definition", NodeTypeConfig::Definition(0, HashMethodSingleValue {{"name"}}));
	config->AddNodeType("lambda", NodeTypeConfig::Definition(0, HashMethodStructural {})); // Anonymous

	// Class definitions
	config->AddNodeType("class_definition", NodeTypeConfig::Definition(2, HashMethodSingleValue {{"name"}}));

	// Variable assignments (Python doesn't have declarations)
	config->AddNodeType("assignment", {ASTKind::EXECUTION, 0, 0, HashMethodStructural {}, 0});
	config->AddNodeType("augmented_assignment", {ASTKind::EXECUTION, 1, 0, HashMethodSingleValue {{"operator"}}, 0});

	// Imports
	config->AddNodeTypes({"import_statement", "import_from_statement"},
	                     {ASTKind::EXTERNAL, 0, 0, HashMethodSingleValue {{"module_name"}}, 0x01}); // is_keyword

	// Control flow
	config->AddNodeType("if_statement", {ASTKind::FLOW_CONTROL, 0, 0, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeTypes({"for_statement", "while_statement"},
	                     {ASTKind::FLOW_CONTROL, 1, 0, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeTypes({"break_statement", "continue_statement", "return_statement", "yield_statement"},
	                     {ASTKind::FLOW_CONTROL, 3, 0, HashMethodStructural {}, 0x01}); // is_keyword

	// Exception handling
	config->AddNodeType("try_statement", {ASTKind::ERROR_HANDLING, 0, 0, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeType("except_clause",
	                    {ASTKind::ERROR_HANDLING, 0, 0, HashMethodSingleValue {{"type"}}, 0x01}); // is_keyword
	config->AddNodeType("raise_statement",
	                    {ASTKind::ERROR_HANDLING, 1, 0, HashMethodStructural {}, 0x01}); // is_keyword

	// Python-specific constructs
	config->AddNodeType("decorator", {ASTKind::METADATA, 1, 0, HashMethodSingleValue {{"expression"}}, 0});
	config->AddNodeType("with_statement", {ASTKind::FLOW_CONTROL, 2, 0, HashMethodStructural {}, 0x01}); // is_keyword

	return config;
}

// C++ configuration
unique_ptr<LanguageConfig> CreateCppConfig() {
	auto config = make_uniq<LanguageConfig>();

	// C++ has many declaration forms
	config->AddNodeType("function_definition",
	                    NodeTypeConfig::Definition(0, HashMethodSingleValue {{"declarator.declarator.identifier"}}));
	config->AddNodeType("class_specifier", NodeTypeConfig::Definition(2, HashMethodSingleValue {{"name"}}));
	config->AddNodeType("struct_specifier", NodeTypeConfig::Definition(2, HashMethodSingleValue {{"name"}}));

	// Preprocessor directives
	config->AddNodeTypes({"preproc_include", "preproc_def", "preproc_ifdef"},
	                     {ASTKind::METADATA, 2, 0, HashMethodStructural {}, 0x01}); // is_keyword

	return config;
}

// SQL configuration
unique_ptr<LanguageConfig> CreateSQLConfig() {
	auto config = make_uniq<LanguageConfig>();

	// SQL queries are transformations
	config->AddNodeTypes({"select_statement", "select"},
	                     {ASTKind::TRANSFORM, 0, 0, HashMethodCustom {"sql_primary_table"}, 0x01}); // is_keyword
	config->AddNodeTypes({"insert_statement", "update_statement", "delete_statement"},
	                     {ASTKind::EXECUTION, 0, 0, HashMethodSingleValue {{"table_name"}}, 0x01}); // is_keyword

	// DDL statements
	config->AddNodeTypes(
	    {"create_table", "create_view", "create_index"},
	    {ASTKind::DEFINITION, 3, 0, HashMethodSingleValue {{"table_name", "name"}}, 0x01}); // is_keyword

	// Table references
	config->AddNodeType("table_reference", NodeTypeConfig::Name(1, HashMethodSingleValue {{"name", "table_name"}}));
	config->AddNodeType("column_reference", NodeTypeConfig::Name(1, HashMethodSingleValue {{"column_name", "name"}}));

	return config;
}

// Rust configuration
unique_ptr<LanguageConfig> CreateRustConfig() {
	auto config = make_uniq<LanguageConfig>();

	// Rust has detailed type information
	config->AddNodeType("function_item", NodeTypeConfig::Definition(0, HashMethodSingleValue {{"name"}}));
	config->AddNodeType("struct_item", NodeTypeConfig::Definition(2, HashMethodSingleValue {{"name"}}));
	config->AddNodeType("enum_item", NodeTypeConfig::Definition(2, HashMethodSingleValue {{"name"}}));
	config->AddNodeType("impl_item", {ASTKind::DEFINITION, 3, 0, HashMethodSingleValue {{"trait", "type"}}, 0});

	// Pattern matching
	config->AddNodeType("match_expression", {ASTKind::FLOW_CONTROL, 0, 1, HashMethodStructural {}, 0x01}); // is_keyword
	config->AddNodeType("match_pattern", {ASTKind::PATTERN, 1, 0, HashMethodStructural {}, 0});

	// Ownership/borrowing
	config->AddNodeTypes({"reference_type", "mutable_specifier"},
	                     {ASTKind::TYPE, 2, 0, HashMethodStructural {}, 0x01}); // is_keyword

	return config;
}

// HTML configuration
unique_ptr<LanguageConfig> CreateHTMLConfig() {
	auto config = make_uniq<LanguageConfig>();

	// HTML elements use tag names and attributes for identity
	config->AddNodeType("element",
	                    {ASTKind::ORGANIZATION, 2, 0, HashMethodAnnotated {{"tag_name"}, {"id", "class", "name"}}, 0});

	config->AddNodeType("attribute", NodeTypeConfig::Name(0, HashMethodSingleValue {{"name"}}));

	config->AddNodeType("text", {ASTKind::LITERAL, 1, 0, HashMethodLiteral {}, 0});

	config->AddNodeType("comment", {ASTKind::METADATA, 0, 0, HashMethodLiteral {}, 0});

	return config;
}

} // namespace duckdb
