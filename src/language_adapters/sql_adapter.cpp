#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_sql();
}

namespace duckdb {

//==============================================================================
// SQL Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> SQLAdapter::node_configs = {
    // SQL-specific node types - DDL statements
    DEF_TYPE("create_table", DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("create_view", DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("create_index", DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("drop_statement", EXECUTION_STATEMENT, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("alter_table", EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0)
    
    // DML statements - queries and transforms
    DEF_TYPE("select_statement", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("insert_statement", EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("update_statement", EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("delete_statement", EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0)
    
    // Identifiers and names - most common unclassified types
    DEF_TYPE("identifier", NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE("field", NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE("object_reference", NAME_QUALIFIED, NODE_TEXT, NONE, 0)
    DEF_TYPE("column_reference", NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE("table_reference", NAME_QUALIFIED, NODE_TEXT, NONE, 0)
    DEF_TYPE("relation", NAME_QUALIFIED, NODE_TEXT, NONE, 0)
    DEF_TYPE("function_call", COMPUTATION_CALL, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("invocation", COMPUTATION_CALL, FIND_IDENTIFIER, NONE, 0)
    
    // Expressions and operations
    DEF_TYPE("binary_expression", COMPUTATION_EXPRESSION, NONE, NONE, 0)
    DEF_TYPE("term", COMPUTATION_EXPRESSION, NONE, NONE, 0)
    
    // Punctuation and operators
    DEF_TYPE(",", PARSER_PUNCTUATION, NONE, NONE, 0)
    DEF_TYPE(".", PARSER_PUNCTUATION, NONE, NONE, 0)
    DEF_TYPE(":", PARSER_PUNCTUATION, NONE, NONE, 0)
    DEF_TYPE("(", PARSER_DELIMITER, NONE, NONE, 0)
    DEF_TYPE(")", PARSER_DELIMITER, NONE, NONE, 0)
    DEF_TYPE("=", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE("!=", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE("<>", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE("<=", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE(">=", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE("<", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE(">", OPERATOR_COMPARISON, NONE, NONE, 0)
    
    // Literals - name and value both contain the literal text
    DEF_TYPE("string_literal", LITERAL_STRING, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE("number_literal", LITERAL_NUMBER, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE("boolean_literal", LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE("literal", LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, 0)
    
    // Keywords with semantic meaning - query operations
    DEF_TYPE("keyword_select", TRANSFORM_QUERY, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_from", TRANSFORM_QUERY, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_where", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_having", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_order", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_by", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_group", TRANSFORM_AGGREGATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_join", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_on", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Logical and comparison operators
    DEF_TYPE("keyword_and", OPERATOR_LOGICAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_not", OPERATOR_LOGICAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_in", OPERATOR_COMPARISON, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Data manipulation operations
    DEF_TYPE("keyword_insert", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_update", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_delete", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_into", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_values", LITERAL_STRUCTURED, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Schema definition operations
    DEF_TYPE("keyword_create", DEFINITION_CLASS, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_drop", EXECUTION_STATEMENT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_alter", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_table", TYPE_COMPOSITE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_view", TYPE_COMPOSITE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_index", TYPE_REFERENCE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Constraint annotations (using METADATA_ANNOTATION as suggested)
    DEF_TYPE("keyword_constraint", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_primary", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_foreign", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_key", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_unique", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_check", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_references", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Literals and defaults
    DEF_TYPE("keyword_null", LITERAL_ATOMIC, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_default", LITERAL_ATOMIC, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Additional SQL keywords
    DEF_TYPE("keyword_type", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_union", TRANSFORM_AGGREGATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_all", TRANSFORM_QUERY, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_bigint", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_name", NAME_IDENTIFIER, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_for", FLOW_LOOP, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_over", TRANSFORM_QUERY, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_if", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_json", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Data types
    DEF_TYPE("bigint", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    
    // SQL constructs
    DEF_TYPE("function_argument", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("window_specification", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("window_function", COMPUTATION_CALL, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("set_operation", TRANSFORM_AGGREGATION, NONE, NONE, 0)
    DEF_TYPE("not_like", OPERATOR_COMPARISON, NONE, NONE, 0)
    
    // Generic keywords and aliases
    DEF_TYPE("keyword", PARSER_CONSTRUCT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_as", NAME_SCOPED, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("comment", METADATA_COMMENT, NONE, NODE_TEXT, 0)
    
    // Query clauses
    DEF_TYPE("where_clause", FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE("having_clause", FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE("order_by_clause", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("group_by_clause", TRANSFORM_AGGREGATION, NONE, NONE, 0)
};

#undef DEF_TYPE

string SQLAdapter::GetLanguageName() const {
    return "sql";
}

vector<string> SQLAdapter::GetAliases() const {
    return {"sql"};
}

void SQLAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_sql();
    parser_wrapper_->SetLanguage(ts_language, "SQL");
}

unique_ptr<TSParserWrapper> SQLAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_sql();
    fresh_parser->SetLanguage(ts_language, "SQL");
    return fresh_parser;
}

string SQLAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string SQLAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // SQL-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("table") != string::npos || node_type.find("view") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string SQLAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool SQLAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In SQL, most objects are "public" in the sense they're accessible
    // Could refine this to check for schema-qualified names
    return true;
}

uint8_t SQLAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* SQLAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

ParsingFunction SQLAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const SQLAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}




} // namespace duckdb
