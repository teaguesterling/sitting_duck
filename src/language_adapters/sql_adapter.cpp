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
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> SQLAdapter::node_configs = {
    // SQL-specific node types - DDL statements
    DEF_TYPE(create_table, DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(create_view, DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(create_index, DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(drop_statement, EXECUTION_STATEMENT, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(alter_table, EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0x01)
    
    // DML statements - queries and transforms
    DEF_TYPE(select_statement, TRANSFORM_QUERY, NONE, NONE, 0x01)
    DEF_TYPE(insert_statement, EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(update_statement, EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(delete_statement, EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0x01)
    
    // Identifiers and names
    DEF_TYPE(identifier, NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE(column_reference, NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE(table_reference, NAME_QUALIFIED, NODE_TEXT, NONE, 0)
    DEF_TYPE(function_call, COMPUTATION_CALL, FIND_IDENTIFIER, NONE, 0)
    
    // Literals - name and value both contain the literal text
    DEF_TYPE(string_literal, LITERAL_STRING, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE(number_literal, LITERAL_NUMBER, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE(boolean_literal, LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE(literal, LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, 0)
    
    // Keywords and comments
    DEF_TYPE(keyword, NAME_KEYWORD, NODE_TEXT, NONE, 0)
    DEF_TYPE(comment, METADATA_COMMENT, NONE, NODE_TEXT, 0x08)
    
    // Query clauses
    DEF_TYPE(where_clause, FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE(having_clause, FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE(order_by_clause, ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE(group_by_clause, TRANSFORM_AGGREGATION, NONE, NONE, 0)
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
