#include "duckdb_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/tableref/basetableref.hpp"
#include "duckdb/parser/tableref/joinref.hpp"
#include "duckdb/parser/tableref/subqueryref.hpp"
#include "duckdb/parser/expression/columnref_expression.hpp"
#include "duckdb/parser/expression/function_expression.hpp"
#include "duckdb/parser/expression/constant_expression.hpp"
#include <cstring>
#include <mutex>
#include <numeric>  // for std::iota
#include <algorithm>  // for std::sort

namespace duckdb {

//==============================================================================
// DuckDB Native Parser Adapter - Architecture Plan Implementation
// 
// Credit: Inspired by zacMode's duckdb_extension_parser_tools
// This implementation follows the technical architecture plan for proper
// integration with the existing AST extension infrastructure.
//==============================================================================

// SQL-specific semantic types using language-specific bits (bits 0-1)
namespace SQLSemanticTypes {
    // TRANSFORM_QUERY variants (1110 00xx)
    constexpr uint8_t TRANSFORM_QUERY_SELECT = SemanticTypes::TRANSFORM_QUERY | 0x00;
    constexpr uint8_t TRANSFORM_QUERY_CTE = SemanticTypes::TRANSFORM_QUERY | 0x01;
    constexpr uint8_t TRANSFORM_QUERY_WINDOW = SemanticTypes::TRANSFORM_QUERY | 0x02;
    constexpr uint8_t TRANSFORM_QUERY_SUBQUERY = SemanticTypes::TRANSFORM_QUERY | 0x03;

    // COMPUTATION_CALL variants (1101 00xx)
    constexpr uint8_t COMPUTATION_CALL_FUNCTION = SemanticTypes::COMPUTATION_CALL | 0x00;
    constexpr uint8_t COMPUTATION_CALL_AGGREGATE = SemanticTypes::COMPUTATION_CALL | 0x01;
    constexpr uint8_t COMPUTATION_CALL_WINDOW = SemanticTypes::COMPUTATION_CALL | 0x02;
    constexpr uint8_t COMPUTATION_CALL_CAST = SemanticTypes::COMPUTATION_CALL | 0x03;

    // EXECUTION_MUTATION variants (1000 11xx)
    constexpr uint8_t EXECUTION_MUTATION_INSERT = SemanticTypes::EXECUTION_MUTATION | 0x00;
    constexpr uint8_t EXECUTION_MUTATION_UPDATE = SemanticTypes::EXECUTION_MUTATION | 0x01;
    constexpr uint8_t EXECUTION_MUTATION_DELETE = SemanticTypes::EXECUTION_MUTATION | 0x02;
    constexpr uint8_t EXECUTION_MUTATION_ALTER = SemanticTypes::EXECUTION_MUTATION | 0x03;
}

//==============================================================================
// Thread-Safe Parser Manager (Architecture Plan Section 5.2)
//==============================================================================
class DuckDBParserManager {
private:
    mutable std::mutex mutex_;
    mutable unique_ptr<Parser> parser_;
    mutable bool initialized_ = false;

public:
    // Get parser instance with proper lifecycle management
    unique_ptr<Parser>& GetParser() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            parser_ = make_uniq<Parser>();
            initialized_ = true;
        }
        return parser_;
    }
    
    // Parse SQL with fresh parser for each call to avoid state issues
    ASTResult ParseSQL(const string& sql_content, const DuckDBAdapter* adapter) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Create a fresh parser for each parse operation to avoid state contamination
        auto fresh_parser = make_uniq<Parser>();
        
        if (!fresh_parser) {
            return adapter->CreateErrorResult("Failed to create DuckDB parser");
        }
        
        try {
            fresh_parser->ParseQuery(sql_content);
            
            // Validate statements were parsed
            if (fresh_parser->statements.empty()) {
                return adapter->CreateErrorResult("No statements parsed from SQL");
            }
            
            return adapter->ConvertStatementsToAST(fresh_parser->statements, sql_content);
        } catch (const ParserException& e) {
            return adapter->CreateErrorResult("Parse error: " + string(e.what()));
        } catch (const Exception& e) {
            return adapter->CreateErrorResult("DuckDB error: " + string(e.what()));
        }
    }
    
    void ResetParser() const {
        std::lock_guard<std::mutex> lock(mutex_);
        parser_.reset();
        initialized_ = false;
    }
};

// Global parser manager following DuckDB singleton patterns
static DuckDBParserManager g_parser_manager;

//==============================================================================
// DuckDBAdapter Implementation
//==============================================================================

string DuckDBAdapter::GetLanguageName() const {
    return "duckdb";
}

vector<string> DuckDBAdapter::GetAliases() const {
    return {"duckdb", "duckdb-sql"};
}

void DuckDBAdapter::InitializeParser() const {
    // DuckDB adapter doesn't use tree-sitter parsers
    // Parser management is handled by DuckDBParserManager using DuckDB's native parser
    // This method is not used for DuckDB parsing workflow
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

//==============================================================================
// Core Parsing Function (Architecture Plan Section 3.2)
//==============================================================================
ASTResult DuckDBAdapter::ParseSQL(const string& sql_content) const {
    try {
        // Use global parser manager with proper lifecycle
        return g_parser_manager.ParseSQL(sql_content, this);
    } catch (const Exception& e) {
        return CreateErrorResult("Internal error: " + string(e.what()));
    }
}

//==============================================================================
// Statement Processing (Architecture Plan Section 4.2)
//==============================================================================
ASTResult DuckDBAdapter::ConvertStatementsToAST(const vector<unique_ptr<SQLStatement>>& statements, 
                                                 const string& content) const {
    if (statements.empty()) {
        return CreateErrorResult("No statements found");
    }
    
    ASTResult result;
    result.source.file_path = "";
    result.source.language = "duckdb";
    result.parse_time = std::chrono::system_clock::now();
    
    // Convert statements to AST nodes
    vector<ASTNode> nodes;
    uint32_t node_counter = 1;  // Start from 1 to match expected test output
    
    // Root program node (node_id = 1, parent = 0 for root)
    ASTNode program_node = CreateASTNode("program", "", "", 
                                        SemanticTypes::DEFINITION_MODULE, 
                                        node_counter++, 0, 0);
    nodes.push_back(program_node);
    
    // Process each statement
    for (const auto& stmt : statements) {
        if (!stmt) {
            // Skip NULL statements
            continue;
        }
        auto stmt_nodes = ConvertStatement(*stmt, node_counter);
        for (auto& node : stmt_nodes) {
            // Set parent to program node
            if (node.tree_position.parent_index == -1) {
                node.tree_position.parent_index = 1; // program node (id=1)
            }
            nodes.push_back(node);
        }
    }
    
    // Update descendant counts
    UpdateDescendantCounts(nodes);
    
    result.nodes = std::move(nodes);
    result.node_count = result.nodes.size();
    result.max_depth = CalculateMaxDepth(result.nodes);
    
    return result;
}

//==============================================================================
// Statement Conversion (Architecture Plan Section 5.3)
//==============================================================================
vector<ASTNode> DuckDBAdapter::ConvertStatement(const SQLStatement& stmt, uint32_t& node_counter) const {
    vector<ASTNode> nodes;
    
    switch (stmt.type) {
        case StatementType::SELECT_STATEMENT: {
            const auto& select_stmt = stmt.Cast<SelectStatement>();
            nodes = ConvertSelectStatement(select_stmt, node_counter);
            break;
        }
        case StatementType::INSERT_STATEMENT: {
            auto node = CreateASTNode("insert_statement", "", stmt.ToString(),
                                     SQLSemanticTypes::EXECUTION_MUTATION_INSERT,
                                     node_counter++, -1, 1);
            nodes.push_back(node);
            break;
        }
        case StatementType::UPDATE_STATEMENT: {
            auto node = CreateASTNode("update_statement", "", stmt.ToString(),
                                     SQLSemanticTypes::EXECUTION_MUTATION_UPDATE,
                                     node_counter++, -1, 1);
            nodes.push_back(node);
            break;
        }
        case StatementType::DELETE_STATEMENT: {
            auto node = CreateASTNode("delete_statement", "", stmt.ToString(),
                                     SQLSemanticTypes::EXECUTION_MUTATION_DELETE,
                                     node_counter++, -1, 1);
            nodes.push_back(node);
            break;
        }
        default: {
            auto node = CreateASTNode("sql_statement", "", stmt.ToString(),
                                     SemanticTypes::EXECUTION_STATEMENT,
                                     node_counter++, -1, 1);
            nodes.push_back(node);
            break;
        }
    }
    
    return nodes;
}

//==============================================================================
// SELECT Statement Processing
//==============================================================================
vector<ASTNode> DuckDBAdapter::ConvertSelectStatement(const SelectStatement& stmt, uint32_t& node_counter) const {
    vector<ASTNode> nodes;
    
    // Main SELECT statement node
    auto select_node = CreateASTNode("select_statement", "", stmt.ToString(),
                                    SQLSemanticTypes::TRANSFORM_QUERY_SELECT,
                                    node_counter++, -1, 1);
    uint32_t select_node_id = select_node.node_id;
    nodes.push_back(select_node);
    
    // Process the query node if it exists
    if (stmt.node && stmt.node->type == QueryNodeType::SELECT_NODE) {
        const auto& query_node = stmt.node->Cast<SelectNode>();
        auto query_nodes = ConvertSelectNode(query_node, node_counter);
        
        // Set parent for all query nodes
        for (auto& node : query_nodes) {
            if (node.tree_position.parent_index == -1) {
                node.tree_position.parent_index = select_node_id;
                node.tree_position.node_depth = 2;
            }
            nodes.push_back(node);
        }
    }
    
    return nodes;
}

//==============================================================================
// SELECT Node Processing  
//==============================================================================
vector<ASTNode> DuckDBAdapter::ConvertSelectNode(const SelectNode& node, uint32_t& node_counter) const {
    vector<ASTNode> nodes;
    
    // SELECT node
    auto select_node = CreateASTNode("select_node", "", "",
                                    SQLSemanticTypes::TRANSFORM_QUERY_SELECT,
                                    node_counter++, -1, 2);
    uint32_t select_node_id = select_node.node_id;
    nodes.push_back(select_node);
    
    // Process SELECT list
    if (!node.select_list.empty()) {
        auto list_node = CreateASTNode("select_list", "", "",
                                      SemanticTypes::ORGANIZATION_CONTAINER,
                                      node_counter++, select_node_id, 3);
        nodes.push_back(list_node);
        
        for (const auto& expr : node.select_list) {
            if (!expr) {
                continue; // Skip NULL expressions
            }
            auto expr_nodes = ConvertExpression(*expr, node_counter);
            for (auto& en : expr_nodes) {
                if (en.tree_position.parent_index == -1) {
                    en.tree_position.parent_index = list_node.node_id;
                    en.tree_position.node_depth = 4;
                }
                nodes.push_back(en);
            }
        }
    }
    
    // Process FROM clause
    if (node.from_table) {
        auto from_nodes = ConvertTableRef(*node.from_table, node_counter);
        for (auto& fn : from_nodes) {
            if (fn.tree_position.parent_index == -1) {
                fn.tree_position.parent_index = select_node_id;
                fn.tree_position.node_depth = 3;
            }
            nodes.push_back(fn);
        }
    }
    
    // Process WHERE clause
    if (node.where_clause) {
        auto where_node = CreateASTNode("where_clause", "", "",
                                       SemanticTypes::FLOW_CONDITIONAL,
                                       node_counter++, select_node_id, 3);
        nodes.push_back(where_node);
        
        auto expr_nodes = ConvertExpression(*node.where_clause, node_counter);
        for (auto& en : expr_nodes) {
            if (en.tree_position.parent_index == -1) {
                en.tree_position.parent_index = where_node.node_id;
                en.tree_position.node_depth = 4;
            }
            nodes.push_back(en);
        }
    }
    
    // Process GROUP BY clause
    if (!node.groups.group_expressions.empty()) {
        auto group_by_node = CreateASTNode("group_by_clause", "", "",
                                          SemanticTypes::TRANSFORM_AGGREGATION,
                                          node_counter++, select_node_id, 3);
        nodes.push_back(group_by_node);
        
        // Process each group expression directly
        for (const auto& expr : node.groups.group_expressions) {
            if (expr) {
                auto group_expr_nodes = ConvertExpression(*expr, node_counter);
                for (auto& gen : group_expr_nodes) {
                    if (gen.tree_position.parent_index == -1) {
                        gen.tree_position.parent_index = group_by_node.node_id;
                        gen.tree_position.node_depth = 4;
                    }
                    nodes.push_back(gen);
                }
            }
        }
    }
    
    return nodes;
}

//==============================================================================
// Expression Processing
//==============================================================================
vector<ASTNode> DuckDBAdapter::ConvertExpression(const ParsedExpression& expr, uint32_t& node_counter) const {
    vector<ASTNode> nodes;
    
    switch (expr.type) {
        case ExpressionType::COLUMN_REF: {
            const auto& col_ref = expr.Cast<ColumnRefExpression>();
            auto node = CreateASTNode("column_reference", col_ref.GetColumnName(), col_ref.ToString(),
                                     SemanticTypes::NAME_IDENTIFIER,
                                     node_counter++, -1, 0);
            nodes.push_back(node);
            break;
        }
        case ExpressionType::FUNCTION: {
            const auto& func_expr = expr.Cast<FunctionExpression>();
            string normalized_name = NormalizeFunctionName(func_expr.function_name);
            auto node = CreateASTNode("function_call", normalized_name, normalized_name,
                                     SQLSemanticTypes::COMPUTATION_CALL_FUNCTION,
                                     node_counter++, -1, 0);
            nodes.push_back(node);
            
            // Process function arguments
            for (const auto& arg : func_expr.children) {
                if (!arg) {
                    continue; // Skip NULL function arguments
                }
                auto arg_nodes = ConvertExpression(*arg, node_counter);
                for (auto& an : arg_nodes) {
                    if (an.tree_position.parent_index == -1) {
                        an.tree_position.parent_index = node.node_id;
                    }
                    nodes.push_back(an);
                }
            }
            break;
        }
        case ExpressionType::VALUE_CONSTANT: {
            const auto& const_expr = expr.Cast<ConstantExpression>();
            string value = const_expr.value.ToString();
            auto node = CreateASTNode("literal", value, value,
                                     SemanticTypes::LITERAL_ATOMIC,
                                     node_counter++, -1, 0);
            nodes.push_back(node);
            break;
        }
        default: {
            auto node = CreateASTNode("expression", "", expr.ToString(),
                                     SemanticTypes::COMPUTATION_EXPRESSION,
                                     node_counter++, -1, 0);
            nodes.push_back(node);
            break;
        }
    }
    
    return nodes;
}

//==============================================================================
// Table Reference Processing
//==============================================================================
vector<ASTNode> DuckDBAdapter::ConvertTableRef(const TableRef& table_ref, uint32_t& node_counter) const {
    vector<ASTNode> nodes;
    
    switch (table_ref.type) {
        case TableReferenceType::BASE_TABLE: {
            const auto& base_table = table_ref.Cast<BaseTableRef>();
            auto node = CreateASTNode("table_reference", base_table.table_name, base_table.table_name,
                                     SemanticTypes::NAME_QUALIFIED,
                                     node_counter++, -1, 0);
            nodes.push_back(node);
            break;
        }
        case TableReferenceType::JOIN: {
            const auto& join_ref = table_ref.Cast<JoinRef>();
            auto node = CreateASTNode("join", "", "",
                                     SemanticTypes::TRANSFORM_ITERATION,
                                     node_counter++, -1, 0);
            nodes.push_back(node);
            
            // Process left and right table references
            vector<ASTNode> left_nodes, right_nodes;
            if (join_ref.left) {
                left_nodes = ConvertTableRef(*join_ref.left, node_counter);
            }
            if (join_ref.right) {
                right_nodes = ConvertTableRef(*join_ref.right, node_counter);
            }
            
            for (auto& ln : left_nodes) {
                if (ln.tree_position.parent_index == -1) {
                    ln.tree_position.parent_index = node.node_id;
                }
                nodes.push_back(ln);
            }
            for (auto& rn : right_nodes) {
                if (rn.tree_position.parent_index == -1) {
                    rn.tree_position.parent_index = node.node_id;
                }
                nodes.push_back(rn);
            }
            break;
        }
        default: {
            auto node = CreateASTNode("unknown_table_ref", "", table_ref.ToString(),
                                     SemanticTypes::NAME_QUALIFIED,
                                     node_counter++, -1, 0);
            nodes.push_back(node);
            break;
        }
    }
    
    return nodes;
}

//==============================================================================
// Utility Functions
//==============================================================================
ASTNode DuckDBAdapter::CreateASTNode(const string& type, const string& name, const string& value,
                                     uint8_t semantic_type, uint32_t node_id, int64_t parent_id, 
                                     uint32_t depth) const {
    ASTNode node;
    
    // Basic information
    node.node_id = node_id;
    node.type.raw = type;
    node.type.normalized = type;
    
    // Names and values - CRITICAL: Fix schema compliance
    node.name.raw = name;
    node.name.qualified = name;
    node.peek = value;  // This provides the content preview and "value" data
    
    // Position information (placeholder values for now)
    node.file_position.start_line = 1;
    node.file_position.end_line = 1;
    node.file_position.start_column = 1;
    node.file_position.end_column = 1;
    
    // Tree position
    node.tree_position.node_index = node_id;
    node.tree_position.parent_index = parent_id;
    node.tree_position.sibling_index = 0;
    node.tree_position.node_depth = depth;
    
    // Semantic information
    node.semantic_type = semantic_type;
    node.universal_flags = 0;
    
    // Subtree information (will be updated later)
    node.subtree.children_count = 0;
    node.subtree.descendant_count = 0;
    
    // Legacy fields
    node.UpdateLegacyFields();
    
    return node;
}

ASTResult DuckDBAdapter::CreateErrorResult(const string& error_message) const {
    ASTResult result;
    result.source.language = "duckdb";
    result.node_count = 0;
    
    // Create an error node to provide some information
    ASTNode error_node = CreateASTNode("parse_error", "error", error_message,
                                      SemanticTypes::PARSER_SYNTAX, 0, -1, 0);
    result.nodes.push_back(error_node);
    result.node_count = 1;
    
    return result;
}

void DuckDBAdapter::UpdateDescendantCounts(vector<ASTNode>& nodes) const {
    // Calculate descendant counts in bottom-up order (highest depth first)
    // Sort by depth descending to ensure we process children before parents
    vector<size_t> indices(nodes.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    std::sort(indices.begin(), indices.end(), [&nodes](size_t a, size_t b) {
        return nodes[a].tree_position.node_depth > nodes[b].tree_position.node_depth;
    });
    
    // Process nodes in depth order (deepest first)
    for (size_t idx : indices) {
        auto& node = nodes[idx];
        uint32_t count = 0;
        
        // Count direct children and their descendants
        for (const auto& other : nodes) {
            if (other.tree_position.parent_index == static_cast<int64_t>(node.node_id)) {
                count += 1 + other.subtree.descendant_count;
            }
        }
        node.subtree.descendant_count = count;
    }
}

uint32_t DuckDBAdapter::CalculateMaxDepth(const vector<ASTNode>& nodes) const {
    uint32_t max_depth = 0;
    for (const auto& node : nodes) {
        max_depth = std::max(max_depth, static_cast<uint32_t>(node.tree_position.node_depth));
    }
    return max_depth;
}

string DuckDBAdapter::NormalizeFunctionName(const string& internal_name) const {
    // Map DuckDB internal function names to user-friendly names
    if (internal_name == "count_star") {
        return "COUNT";
    }
    if (internal_name == "sum") {
        return "SUM"; 
    }
    if (internal_name == "avg") {
        return "AVG";
    }
    if (internal_name == "min") {
        return "MIN";
    }
    if (internal_name == "max") {
        return "MAX";
    }
    if (internal_name == "list_transform") {
        return "list_transform";  // Keep DuckDB-specific functions as-is
    }
    if (internal_name == "struct_pack") {
        return "struct_pack";
    }
    
    // For unknown functions, convert to uppercase (SQL convention)
    string result = internal_name;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

//==============================================================================
// Parsing Function Integration
//==============================================================================
ParsingFunction DuckDBAdapter::GetParsingFunction() const {
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const DuckDBAdapter*>(adapter);
        return typed_adapter->ParseSQL(content);
    };
}

} // namespace duckdb