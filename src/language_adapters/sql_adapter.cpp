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
    
    // Core SQL constructs 
    DEF_TYPE("select_expression", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("select", TRANSFORM_QUERY, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("from", TRANSFORM_QUERY, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE(";", PARSER_PUNCTUATION, NONE, NONE, 0)
    DEF_TYPE("statement", EXECUTION_STATEMENT, NONE, NONE, 0)
    DEF_TYPE("column_definition", DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("*", OPERATOR_ARITHMETIC, NONE, NONE, 0)
    DEF_TYPE("where", FLOW_CONDITIONAL, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("order_target", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("all_fields", TRANSFORM_PROJECTION, NONE, NONE, 0)
    DEF_TYPE("parenthesized_expression", COMPUTATION_EXPRESSION, NONE, NONE, 0)
    DEF_TYPE("program", DEFINITION_MODULE, NONE, NONE, 0)
    DEF_TYPE("+", OPERATOR_ARITHMETIC, NONE, NONE, 0)
    DEF_TYPE("list", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("subquery", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("order_by", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("group_by", TRANSFORM_AGGREGATION, NONE, NONE, 0)
    
    // Additional SQL keywords
    DEF_TYPE("keyword_or", OPERATOR_LOGICAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_int", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("int", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_replace", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_like", OPERATOR_COMPARISON, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_when", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_then", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Query clauses
    DEF_TYPE("where_clause", FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE("having_clause", FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE("order_by_clause", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("group_by_clause", TRANSFORM_AGGREGATION, NONE, NONE, 0)
    
    // Major remaining SQL constructs for improved classification - 9547 nodes
    DEF_TYPE("cast", COMPUTATION_CALL, FIND_IDENTIFIER, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("varchar", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_varchar", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("-", OPERATOR_ARITHMETIC, NODE_TEXT, NONE, 0)
    DEF_TYPE("join", TRANSFORM_ITERATION, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_end", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_case", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_else", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("case", FLOW_CONDITIONAL, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("create_query", DEFINITION_CLASS, NONE, NONE, 0)
    DEF_TYPE("keyword_limit", TRANSFORM_QUERY, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_between", OPERATOR_COMPARISON, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("between_expression", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE("limit", TRANSFORM_QUERY, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("]", PARSER_DELIMITER, NONE, NONE, 0)
    DEF_TYPE("[", PARSER_DELIMITER, NONE, NONE, 0)
    DEF_TYPE("cte", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("op_other", OPERATOR_ARITHMETIC, NONE, NONE, 0)
    DEF_TYPE(":=", OPERATOR_ASSIGNMENT, NONE, NONE, 0)
    DEF_TYPE("keyword_with", TRANSFORM_QUERY, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("with", TRANSFORM_QUERY, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_distinct", TRANSFORM_PROJECTION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("distinct", TRANSFORM_PROJECTION, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_order_by", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_group_by", TRANSFORM_AGGREGATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // DuckDB SQL perfection - remaining 4,885 unclassified nodes
    DEF_TYPE("direction", ORGANIZATION_LIST, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_cast", COMPUTATION_CALL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_is", OPERATOR_COMPARISON, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_desc", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_left", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("::", COMPUTATION_ACCESS, NONE, NONE, 0)
    DEF_TYPE("keyword_first", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("/", OPERATOR_ARITHMETIC, NONE, NONE, 0)
    DEF_TYPE("keyword_date", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_outer", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("column", NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE("decimal", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_decimal", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("column_definitions", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("keyword_temporary", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("is_not", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE("smallint", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_smallint", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_copy", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Additional DuckDB constructs from the full list
    DEF_TYPE("keyword_right", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_inner", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_full", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_cross", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_using", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_natural", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_exists", OPERATOR_COMPARISON, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_any", OPERATOR_COMPARISON, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_some", OPERATOR_COMPARISON, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_unique", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_last", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_asc", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_nulls", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_rows", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_range", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_unbounded", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_preceding", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_following", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_current", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_row", ORGANIZATION_LIST, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    
    // Complete DuckDB SQL constructs - remaining nodes
    DEF_TYPE("function_arguments", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("keyword_varying", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("subscript", COMPUTATION_ACCESS, NONE, NONE, 0)
    DEF_TYPE("keyword_partition", TRANSFORM_AGGREGATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("partition_by", TRANSFORM_AGGREGATION, NONE, NONE, 0)
    DEF_TYPE("exists", OPERATOR_COMPARISON, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("insert", EXECUTION_MUTATION, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_filter", FLOW_CONDITIONAL, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_text", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("filter_expression", FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE("keyword_language", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("timestamp", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_timestamp", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("constraints", METADATA_ANNOTATION, NONE, NONE, 0)
    DEF_TYPE("constraint", METADATA_ANNOTATION, NONE, NONE, 0)
    DEF_TYPE("ordered_columns", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("array_size_definition", TYPE_COMPOSITE, NONE, NONE, 0)
    DEF_TYPE("unary_expression", OPERATOR_ARITHMETIC, NONE, NONE, 0)
    DEF_TYPE("keyword_true", LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("create_type", DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("double", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_double", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_temp", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("%", OPERATOR_ARITHMETIC, NONE, NONE, 0)
    DEF_TYPE("keyword_time", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_function", DEFINITION_FUNCTION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_without", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_zone", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("marginalia", METADATA_COMMENT, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE("keyword_header", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_delimiter", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_recursive", FLOW_LOOP, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("filename", LITERAL_STRING, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE("frame_definition", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("keyword_false", LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_database", DEFINITION_MODULE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_float", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("cross_join", TRANSFORM_ITERATION, NONE, NONE, 0)
    DEF_TYPE("float", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("not_in", OPERATOR_COMPARISON, NONE, NONE, 0)
    DEF_TYPE("offset", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("keyword_offset", TRANSFORM_QUERY, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_intersect", TRANSFORM_AGGREGATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("window_frame", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("values", LITERAL_STRUCTURED, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_current_timestamp", LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_set", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_boolean", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("assignment", OPERATOR_ASSIGNMENT, NONE, NONE, 0)
    DEF_TYPE("keyword_off", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_csv", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_value", LITERAL_ATOMIC, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_parquet", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("update", EXECUTION_MUTATION, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_lines", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("table_option", METADATA_ANNOTATION, NONE, NONE, 0)
    DEF_TYPE("keyword_except", TRANSFORM_AGGREGATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("create_schema", DEFINITION_MODULE, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("keyword_schema", DEFINITION_MODULE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_analyze", EXECUTION_STATEMENT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("rename_object", EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("keyword_to", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_array", TYPE_COMPOSITE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_rename", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_materialized", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_returning", EXECUTION_MUTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_input", EXTERNAL_IMPORT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("array", TYPE_COMPOSITE, NONE, NONE, 0)
    DEF_TYPE("tinyint", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_char", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("time", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_transaction", EXECUTION_STATEMENT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_statement", EXECUTION_STATEMENT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_real", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("lateral_cross_join", TRANSFORM_ITERATION, NONE, NONE, 0)
    DEF_TYPE("char", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_tinyint", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_begin", EXECUTION_STATEMENT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("transaction", EXECUTION_STATEMENT, NONE, NONE, 0)
    DEF_TYPE("keyword_serial", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_comment", METADATA_COMMENT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("create_sequence", DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0)
    DEF_TYPE("keyword_nothing", LITERAL_ATOMIC, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_do", EXECUTION_STATEMENT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("index_fields", ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE("parameter", NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_window", TRANSFORM_QUERY, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_format", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_ignore", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("returning", EXECUTION_MUTATION, NONE, NONE, 0)
    DEF_TYPE("keyword_lateral", TRANSFORM_ITERATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("interval", TYPE_PRIMITIVE, NODE_TEXT, NONE, 0)
    DEF_TYPE("keyword_interval", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("window_clause", TRANSFORM_QUERY, NONE, NONE, 0)
    DEF_TYPE("op_unary_other", OPERATOR_ARITHMETIC, NONE, NONE, 0)
    DEF_TYPE("keyword_commit", EXECUTION_STATEMENT, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_sequence", DEFINITION_VARIABLE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_uuid", TYPE_PRIMITIVE, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
    DEF_TYPE("keyword_conflict", METADATA_ANNOTATION, NODE_TEXT, NONE, ASTNodeFlags::IS_KEYWORD)
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
