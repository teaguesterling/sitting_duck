#include "duckdb_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/statement/insert_statement.hpp"
#include "duckdb/parser/statement/update_statement.hpp"
#include "duckdb/parser/statement/delete_statement.hpp"
#include "duckdb/parser/statement/create_statement.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "duckdb/parser/tableref/joinref.hpp"
#include "duckdb/parser/tableref/subqueryref.hpp"
#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include <cstring>
#include <queue>
#include <mutex>

namespace duckdb {

//==============================================================================
// DuckDB Native Parser Adapter implementation
// 
// Credit: Inspired by and building upon the excellent work of zacMode's
// duckdb_extension_parser_tools (https://github.com/zacMode/duckdb_extension_parser_tools)
// This implementation extends those concepts to provide full semantic AST analysis
// using DuckDB's native parser for maximum SQL accuracy.
//==============================================================================

// SQL-specific semantic types using language-specific bits (bits 0-1)
namespace SQLSemanticTypes {
    // TRANSFORM_QUERY variants (1110 00xx)
    constexpr uint8_t TRANSFORM_QUERY_SELECT = SemanticTypes::TRANSFORM_QUERY | 0x00;    // SELECT statements
    constexpr uint8_t TRANSFORM_QUERY_CTE = SemanticTypes::TRANSFORM_QUERY | 0x01;       // WITH/CTE  
    constexpr uint8_t TRANSFORM_QUERY_WINDOW = SemanticTypes::TRANSFORM_QUERY | 0x02;    // Window functions
    constexpr uint8_t TRANSFORM_QUERY_SUBQUERY = SemanticTypes::TRANSFORM_QUERY | 0x03;  // Subqueries

    // DEFINITION_CLASS variants (1111 10xx) 
    constexpr uint8_t DEFINITION_TABLE = SemanticTypes::DEFINITION_CLASS | 0x00;         // CREATE TABLE
    constexpr uint8_t DEFINITION_VIEW = SemanticTypes::DEFINITION_CLASS | 0x01;          // CREATE VIEW  
    constexpr uint8_t DEFINITION_INDEX = SemanticTypes::DEFINITION_CLASS | 0x02;         // CREATE INDEX
    constexpr uint8_t DEFINITION_CONSTRAINT = SemanticTypes::DEFINITION_CLASS | 0x03;    // Constraints

    // COMPUTATION_CALL variants (1101 00xx)
    constexpr uint8_t COMPUTATION_CALL_FUNCTION = SemanticTypes::COMPUTATION_CALL | 0x00;   // Function calls
    constexpr uint8_t COMPUTATION_CALL_AGGREGATE = SemanticTypes::COMPUTATION_CALL | 0x01;  // Aggregate functions
    constexpr uint8_t COMPUTATION_CALL_WINDOW = SemanticTypes::COMPUTATION_CALL | 0x02;     // Window functions
    constexpr uint8_t COMPUTATION_CALL_CAST = SemanticTypes::COMPUTATION_CALL | 0x03;       // CAST operations

    // EXECUTION_MUTATION variants (1000 11xx)
    constexpr uint8_t EXECUTION_MUTATION_INSERT = SemanticTypes::EXECUTION_MUTATION | 0x00; // INSERT
    constexpr uint8_t EXECUTION_MUTATION_UPDATE = SemanticTypes::EXECUTION_MUTATION | 0x01; // UPDATE
    constexpr uint8_t EXECUTION_MUTATION_DELETE = SemanticTypes::EXECUTION_MUTATION | 0x02; // DELETE
    constexpr uint8_t EXECUTION_MUTATION_ALTER = SemanticTypes::EXECUTION_MUTATION | 0x03;  // ALTER
}

// DuckDBASTNode is now defined in the header file

string DuckDBAdapter::GetLanguageName() const {
    return "duckdb";
}

vector<string> DuckDBAdapter::GetAliases() const {
    return {"duckdb", "duckdb-sql"};
}

void DuckDBAdapter::InitializeParser() const {
    std::call_once(parser_init_flag_, [this]() {
        const_cast<DuckDBAdapter*>(this)->parser_ = make_uniq<Parser>();
    });
}

unique_ptr<TSParserWrapper> DuckDBAdapter::CreateFreshParser() const {
    // DuckDB adapter doesn't use tree-sitter, return nullptr
    return nullptr;
}

string DuckDBAdapter::GetNormalizedType(const string &node_type) const {
    // DuckDB nodes are already semantically meaningful
    return node_type;
}

string DuckDBAdapter::ExtractNodeName(TSNode node, const string &content) const {
    // Not used in DuckDB adapter - names extracted during AST conversion
    return "";
}

string DuckDBAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    // Not used in DuckDB adapter - values extracted during AST conversion  
    return "";
}

bool DuckDBAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In SQL context, most constructs are "public" (accessible)
    return true;
}

uint8_t DuckDBAdapter::GetNodeFlags(const string &node_type) const {
    // Determine flags based on node type
    if (node_type.find("keyword") != string::npos) {
        return ASTNodeFlags::IS_KEYWORD;
    }
    return 0;
}

const NodeConfig* DuckDBAdapter::GetNodeConfig(const string &node_type) const {
    // For DuckDB adapter, we handle semantic types directly in conversion
    // Return nullptr to indicate no static configuration
    return nullptr;
}

DuckDBAdapter::DuckDBASTNode DuckDBAdapter::ConvertSelectStatement(const SelectStatement& stmt) const {
    DuckDBASTNode root("select_statement", SQLSemanticTypes::TRANSFORM_QUERY_SELECT);
    
    // Convert the main query node - need to check type and cast
    if (stmt.node) {
        if (stmt.node->type == QueryNodeType::SELECT_NODE) {
            const auto& select_node = stmt.node->Cast<SelectNode>();
            auto query_node = ConvertSelectNode(select_node);
            root.children.push_back(std::move(query_node));
        } else {
            // Handle other query node types as generic nodes
            DuckDBASTNode query_node("query_node", SQLSemanticTypes::TRANSFORM_QUERY_SELECT);
            query_node.value = stmt.node->ToString();
            root.children.push_back(std::move(query_node));
        }
        
        // Add CTEs if present (CTEs are part of QueryNode, not SelectStatement)
        if (!stmt.node->cte_map.map.empty()) {
            DuckDBASTNode cte_node("with_clause", SQLSemanticTypes::TRANSFORM_QUERY_CTE);
            for (const auto& cte_pair : stmt.node->cte_map.map) {
                DuckDBASTNode cte("cte", SQLSemanticTypes::TRANSFORM_QUERY_CTE);
                cte.name = cte_pair.first; // CTE name
                // CTE details would need more parsing
                cte_node.children.push_back(std::move(cte));
            }
            root.children.push_back(std::move(cte_node));
        }
    }
    
    return root;
}

DuckDBAdapter::DuckDBASTNode DuckDBAdapter::ConvertSelectNode(const SelectNode& node) const {
    DuckDBASTNode select_node("select_node", SQLSemanticTypes::TRANSFORM_QUERY_SELECT);
    
    // Convert SELECT list
    if (!node.select_list.empty()) {
        DuckDBASTNode select_list("select_list", SemanticTypes::ORGANIZATION_LIST);
        for (const auto& expr : node.select_list) {
            auto expr_node = ConvertExpression(*expr);
            select_list.children.push_back(std::move(expr_node));
        }
        select_node.children.push_back(std::move(select_list));
    }
    
    // Convert FROM clause
    if (node.from_table) {
        auto from_node = ConvertTableRef(*node.from_table);
        select_node.children.push_back(std::move(from_node));
    }
    
    // Convert WHERE clause
    if (node.where_clause) {
        DuckDBASTNode where_node("where_clause", SemanticTypes::FLOW_CONDITIONAL);
        auto where_expr = ConvertExpression(*node.where_clause);
        where_node.children.push_back(std::move(where_expr));
        select_node.children.push_back(std::move(where_node));
    }
    
    // Convert GROUP BY
    if (!node.groups.group_expressions.empty()) {
        DuckDBASTNode group_by("group_by_clause", SemanticTypes::TRANSFORM_AGGREGATION);
        for (const auto& group_expr : node.groups.group_expressions) {
            auto expr_node = ConvertExpression(*group_expr);
            group_by.children.push_back(std::move(expr_node));
        }
        select_node.children.push_back(std::move(group_by));
    }
    
    // Convert HAVING clause
    if (node.having) {
        DuckDBASTNode having_node("having_clause", SemanticTypes::FLOW_CONDITIONAL);
        auto having_expr = ConvertExpression(*node.having);
        having_node.children.push_back(std::move(having_expr));
        select_node.children.push_back(std::move(having_node));
    }
    
    // Note: ORDER BY is handled in the parent QueryNode's modifiers, not in SelectNode
    // We would need to check the parent QueryNode for ORDER BY clauses
    
    return select_node;
}

DuckDBAdapter::DuckDBASTNode DuckDBAdapter::ConvertTableRef(const TableRef& table_ref) const {
    switch (table_ref.type) {
        case TableReferenceType::BASE_TABLE: {
            const auto& base_table = table_ref.Cast<BaseTableRef>();
            DuckDBASTNode table_node("table_reference", SemanticTypes::NAME_QUALIFIED);
            table_node.name = base_table.table_name;
            if (!base_table.schema_name.empty()) {
                table_node.value = base_table.schema_name + "." + base_table.table_name;
            } else {
                table_node.value = base_table.table_name;
            }
            return table_node;
        }
        case TableReferenceType::JOIN: {
            const auto& join_ref = table_ref.Cast<JoinRef>();
            DuckDBASTNode join_node("join", SemanticTypes::TRANSFORM_ITERATION);
            
            // Add left and right table references
            auto left_node = ConvertTableRef(*join_ref.left);
            auto right_node = ConvertTableRef(*join_ref.right);
            join_node.children.push_back(std::move(left_node));
            join_node.children.push_back(std::move(right_node));
            
            // Add join condition if present
            if (join_ref.condition) {
                auto condition_node = ConvertExpression(*join_ref.condition);
                join_node.children.push_back(std::move(condition_node));
            }
            
            return join_node;
        }
        case TableReferenceType::SUBQUERY: {
            const auto& subquery_ref = table_ref.Cast<SubqueryRef>();
            DuckDBASTNode subquery_node("subquery", SQLSemanticTypes::TRANSFORM_QUERY_SUBQUERY);
            
            if (subquery_ref.subquery && subquery_ref.subquery->node) {
                if (subquery_ref.subquery->node->type == QueryNodeType::SELECT_NODE) {
                    const auto& select_node = subquery_ref.subquery->node->Cast<SelectNode>();
                    auto query_node = ConvertSelectNode(select_node);
                    subquery_node.children.push_back(std::move(query_node));
                } else {
                    DuckDBASTNode query_node("query_node", SQLSemanticTypes::TRANSFORM_QUERY_SELECT);
                    query_node.value = subquery_ref.subquery->node->ToString();
                    subquery_node.children.push_back(std::move(query_node));
                }
            }
            
            return subquery_node;
        }
        default: {
            DuckDBASTNode unknown_node("unknown_table_ref", SemanticTypes::NAME_QUALIFIED);
            return unknown_node;
        }
    }
}

DuckDBAdapter::DuckDBASTNode DuckDBAdapter::ConvertExpression(const ParsedExpression& expr) const {
    switch (expr.type) {
        case ExpressionType::COLUMN_REF: {
            const auto& col_ref = expr.Cast<ColumnRefExpression>();
            DuckDBASTNode col_node("column_reference", SemanticTypes::NAME_IDENTIFIER);
            col_node.name = col_ref.GetColumnName();
            col_node.value = col_ref.ToString();
            return col_node;
        }
        case ExpressionType::FUNCTION: {
            const auto& func_expr = expr.Cast<FunctionExpression>();
            DuckDBASTNode func_node("function_call", SQLSemanticTypes::COMPUTATION_CALL_FUNCTION);
            func_node.name = func_expr.function_name;
            func_node.value = func_expr.function_name;
            
            // Add function arguments
            for (const auto& arg : func_expr.children) {
                auto arg_node = ConvertExpression(*arg);
                func_node.children.push_back(std::move(arg_node));
            }
            
            return func_node;
        }
        case ExpressionType::VALUE_CONSTANT: {
            const auto& const_expr = expr.Cast<ConstantExpression>();
            DuckDBASTNode const_node("literal", SemanticTypes::LITERAL_ATOMIC);
            const_node.value = const_expr.value.ToString();
            const_node.name = const_node.value; // For literals, name = value
            return const_node;
        }
        default: {
            DuckDBASTNode expr_node("expression", SemanticTypes::COMPUTATION_EXPRESSION);
            expr_node.value = expr.ToString();
            return expr_node;
        }
    }
}

ASTResult DuckDBAdapter::ParseSQL(const string& sql_content) const {
    try {
        InitializeParser();
        
        vector<unique_ptr<SQLStatement>> statements;
        try {
            parser_->ParseQuery(sql_content);
            statements = std::move(parser_->statements);
        } catch (const ParserException& e) {
            return CreateErrorResult("Parse error: " + string(e.what()));
        } catch (const Exception& e) {
            return CreateErrorResult("DuckDB error: " + string(e.what()));
        }
        
        return ProcessStatements(statements, sql_content);
        
    } catch (const Exception& e) {
        return CreateErrorResult("DuckDB parse error: " + string(e.what()));
    }
}

ASTResult DuckDBAdapter::ProcessStatements(const vector<unique_ptr<SQLStatement>>& statements, const string& content) const {
    if (statements.empty()) {
        return CreateErrorResult("No statements found");
    }
    
    // For now, handle only the first statement
    const auto& first_stmt = statements[0];
    
    DuckDBASTNode root("program", SemanticTypes::DEFINITION_MODULE);
    
    switch (first_stmt->type) {
        case StatementType::SELECT_STATEMENT: {
            const auto& select_stmt = first_stmt->Cast<SelectStatement>();
            auto select_node = ConvertSelectStatement(select_stmt);
            root.children.push_back(std::move(select_node));
            break;
        }
        case StatementType::INSERT_STATEMENT: {
            DuckDBASTNode stmt_node("insert_statement", SQLSemanticTypes::EXECUTION_MUTATION_INSERT);
            stmt_node.value = content;
            root.children.push_back(std::move(stmt_node));
            break;
        }
        case StatementType::UPDATE_STATEMENT: {
            DuckDBASTNode stmt_node("update_statement", SQLSemanticTypes::EXECUTION_MUTATION_UPDATE);
            stmt_node.value = content;
            root.children.push_back(std::move(stmt_node));
            break;
        }
        case StatementType::DELETE_STATEMENT: {
            DuckDBASTNode stmt_node("delete_statement", SQLSemanticTypes::EXECUTION_MUTATION_DELETE);
            stmt_node.value = content;
            root.children.push_back(std::move(stmt_node));
            break;
        }
        case StatementType::CREATE_STATEMENT: {
            DuckDBASTNode stmt_node("create_statement", SQLSemanticTypes::DEFINITION_TABLE);
            stmt_node.value = content;
            root.children.push_back(std::move(stmt_node));
            break;
        }
        default: {
            DuckDBASTNode stmt_node("sql_statement", SemanticTypes::EXECUTION_STATEMENT);
            stmt_node.value = content;
            root.children.push_back(std::move(stmt_node));
            break;
        }
    }
    
    return GenerateASTResult(root, content);
}

ASTResult DuckDBAdapter::CreateErrorResult(const string& error_message) const {
    ASTResult result;
    // Note: ASTResult doesn't have success/error_message fields in this system
    // We create an empty result to indicate error
    result.source.language = "duckdb";
    result.node_count = 0;
    return result;
}

ASTResult DuckDBAdapter::GenerateASTResult(const DuckDBASTNode& root, const string& content) const {
    ASTResult result;
    // ASTResult doesn't have success field - just populate the result
    result.source.file_path = "";
    result.source.language = "duckdb";
    result.parse_time = std::chrono::system_clock::now();
    
    // Flatten the AST into our standard format
    vector<ASTNode> nodes;
    int64_t node_counter = 0;
    FlattenNode(root, nodes, -1, 0, node_counter);
    
    result.nodes = std::move(nodes);
    result.node_count = result.nodes.size();
    result.max_depth = 0;
    for (const auto& node : result.nodes) {
        result.max_depth = std::max(result.max_depth, static_cast<uint32_t>(node.tree_position.node_depth));
    }
    
    return result;
}

void DuckDBAdapter::FlattenNode(const DuckDBASTNode& node, vector<ASTNode>& nodes, 
                                int64_t parent_id, uint32_t depth, int64_t& node_counter) const {
    int64_t current_id = node_counter++;
    
    ASTNode ast_node;
    
    // Basic information
    ast_node.node_id = current_id;
    ast_node.type.raw = node.type;
    ast_node.type.normalized = node.type;
    
    // Names and values
    ast_node.name.raw = node.name;
    ast_node.name.qualified = node.name;
    ast_node.peek = node.value;
    
    // Position information
    ast_node.file_position.start_line = node.start_line;
    ast_node.file_position.end_line = node.end_line;
    ast_node.file_position.start_column = node.start_col;
    ast_node.file_position.end_column = node.end_col;
    
    // Tree position
    ast_node.tree_position.node_index = current_id;
    ast_node.tree_position.parent_index = parent_id;
    ast_node.tree_position.sibling_index = 0; // TODO: Calculate actual sibling index
    ast_node.tree_position.node_depth = depth;
    
    // Semantic information
    ast_node.semantic_type = node.semantic_type;
    ast_node.universal_flags = node.flags;
    
    // Subtree information
    ast_node.subtree.children_count = node.children.size();
    ast_node.subtree.descendant_count = 0; // Will be calculated
    
    // Legacy fields
    ast_node.UpdateLegacyFields();
    
    nodes.push_back(ast_node);
    
    // Process children
    uint32_t sibling_index = 0;
    for (const auto& child : node.children) {
        FlattenNode(child, nodes, current_id, depth + 1, node_counter);
        sibling_index++;
    }
    
    // Calculate descendant count
    int32_t descendant_count = node_counter - current_id - 1;
    nodes[current_id].subtree.descendant_count = descendant_count;
}

ParsingFunction DuckDBAdapter::GetParsingFunction() const {
    // Return a lambda that uses our DuckDB parser instead of tree-sitter
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const DuckDBAdapter*>(adapter);
        return typed_adapter->ParseSQL(content);
    };
}

} // namespace duckdb