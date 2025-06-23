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
#include "duckdb/parser/expression/window_expression.hpp"
#include "duckdb/parser/expression/comparison_expression.hpp"
#include "duckdb/parser/expression/conjunction_expression.hpp"
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

const unordered_map<string, NodeConfig>& DuckDBAdapter::GetNodeConfigs() const {
    // DuckDB adapter handles semantic types dynamically during conversion
    // Return empty static map since we don't use static configuration
    static const unordered_map<string, NodeConfig> empty_configs;
    return empty_configs;
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
        
        try {
            auto stmt_nodes = ConvertStatement(*stmt, node_counter);
            for (auto& node : stmt_nodes) {
                // Set parent to program node
                if (node.structure.parent_id == -1) {
                    node.structure.parent_id = 1; // program node (id=1)
                    node.UpdateComputedLegacyFields();
                }
                nodes.push_back(node);
            }
        } catch (const Exception& e) {
            // If statement conversion fails, create an error node
            auto error_node = CreateASTNode("statement_error", "error", string(e.what()),
                                           SemanticTypes::PARSER_SYNTAX,
                                           node_counter++, 1, 1);
            nodes.push_back(error_node);
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
                                     SemanticTypes::EXECUTION_MUTATION,
                                     node_counter++, -1, 1);
            nodes.push_back(node);
            break;
        }
        case StatementType::UPDATE_STATEMENT: {
            auto node = CreateASTNode("update_statement", "", stmt.ToString(),
                                     SemanticTypes::EXECUTION_MUTATION,
                                     node_counter++, -1, 1);
            nodes.push_back(node);
            break;
        }
        case StatementType::DELETE_STATEMENT: {
            auto node = CreateASTNode("delete_statement", "", stmt.ToString(),
                                     SemanticTypes::EXECUTION_MUTATION,
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
                                    SemanticTypes::TRANSFORM_QUERY,
                                    node_counter++, -1, 1);
    uint32_t select_node_id = select_node.node_id;
    nodes.push_back(select_node);
    
    // Process the query node if it exists
    if (stmt.node) {
        try {
            if (stmt.node->type == QueryNodeType::SELECT_NODE) {
                const auto& query_node = stmt.node->Cast<SelectNode>();
                auto query_nodes = ConvertSelectNode(query_node, node_counter);
                
                // Set parent for all query nodes
                for (auto& node : query_nodes) {
                    if (node.structure.parent_id == -1) {
                        node.structure.parent_id = select_node_id;
                        node.structure.depth = 2;
                        node.UpdateComputedLegacyFields();
                    }
                    nodes.push_back(node);
                }
            } else {
                // Handle other query node types
                auto generic_node = CreateASTNode("query_node", "", stmt.node->ToString(),
                                                 SemanticTypes::COMPUTATION_EXPRESSION,
                                                 node_counter++, select_node_id, 2);
                nodes.push_back(generic_node);
            }
        } catch (const Exception& e) {
            // If query node processing fails, create an error node
            auto error_node = CreateASTNode("query_error", "error", string(e.what()),
                                           SemanticTypes::PARSER_SYNTAX,
                                           node_counter++, select_node_id, 2);
            nodes.push_back(error_node);
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
                                    SemanticTypes::TRANSFORM_QUERY,
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
            try {
                auto expr_nodes = ConvertExpression(*expr, node_counter);
                for (auto& en : expr_nodes) {
                    if (en.structure.parent_id == -1) {
                        en.structure.parent_id = list_node.node_id;
                        en.structure.depth = 4;
                        en.UpdateComputedLegacyFields();
                    }
                    nodes.push_back(en);
                }
            } catch (const Exception& e) {
                // If expression conversion fails, create an error node
                auto error_node = CreateASTNode("expression_error", "error", string(e.what()),
                                               SemanticTypes::PARSER_SYNTAX,
                                               node_counter++, list_node.node_id, 4);
                nodes.push_back(error_node);
            }
        }
    }
    
    // Process FROM clause
    if (node.from_table) {
        try {
            auto from_nodes = ConvertTableRef(*node.from_table, node_counter);
            for (auto& fn : from_nodes) {
                if (fn.structure.parent_id == -1) {
                    fn.structure.parent_id = select_node_id;
                    fn.structure.depth = 3;
                    fn.UpdateComputedLegacyFields();
                }
                nodes.push_back(fn);
            }
        } catch (const Exception& e) {
            // If table reference conversion fails, create an error node
            auto error_node = CreateASTNode("table_error", "error", string(e.what()),
                                           SemanticTypes::PARSER_SYNTAX,
                                           node_counter++, select_node_id, 3);
            nodes.push_back(error_node);
        }
    }
    
    // Process WHERE clause
    if (node.where_clause) {
        auto where_node = CreateASTNode("where_clause", "", "",
                                       SemanticTypes::FLOW_CONDITIONAL,
                                       node_counter++, select_node_id, 3);
        nodes.push_back(where_node);
        
        try {
            auto expr_nodes = ConvertExpression(*node.where_clause, node_counter);
            for (auto& en : expr_nodes) {
                if (en.structure.parent_id == -1) {
                    en.structure.parent_id = where_node.node_id;
                    en.structure.depth = 4;
                    en.UpdateComputedLegacyFields();
                }
                nodes.push_back(en);
            }
        } catch (const Exception& e) {
            // If WHERE expression conversion fails, create an error node
            auto error_node = CreateASTNode("where_error", "error", string(e.what()),
                                           SemanticTypes::PARSER_SYNTAX,
                                           node_counter++, where_node.node_id, 4);
            nodes.push_back(error_node);
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
                    if (gen.structure.parent_id == -1) {
                        gen.structure.parent_id = group_by_node.node_id;
                        gen.structure.depth = 4;
                        gen.UpdateComputedLegacyFields();
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
            
            // Skip internal constructor functions that users don't typically write explicitly
            if (func_expr.function_name == "list_value" || func_expr.function_name == "struct_pack_internal") {
                // Process arguments but don't create the function call node
                for (const auto& arg : func_expr.children) {
                    if (!arg) {
                        continue;
                    }
                    auto arg_nodes = ConvertExpression(*arg, node_counter);
                    for (auto& an : arg_nodes) {
                        nodes.push_back(an);
                    }
                }
            } else {
                // Use basic function call semantic type for now
                uint8_t semantic_type = SemanticTypes::COMPUTATION_CALL;
                
                // Create the function call node for user-visible functions
                auto node = CreateASTNode("function_call", normalized_name, normalized_name,
                                         semantic_type, node_counter++, -1, 0);
                nodes.push_back(node);
                
                // Process function arguments
                for (const auto& arg : func_expr.children) {
                    if (!arg) {
                        continue; // Skip NULL function arguments
                    }
                    auto arg_nodes = ConvertExpression(*arg, node_counter);
                    for (auto& an : arg_nodes) {
                        if (an.structure.parent_id == -1) {
                            an.structure.parent_id = node.node_id;
                            an.UpdateComputedLegacyFields();
                        }
                        nodes.push_back(an);
                    }
                }
            }
            break;
        }
        case ExpressionType::VALUE_CONSTANT: {
            const auto& const_expr = expr.Cast<ConstantExpression>();
            string value = const_expr.value.ToString();
            
            // Determine appropriate semantic type based on value type
            uint8_t semantic_type;
            auto value_type = const_expr.value.type();
            if (value_type == LogicalType::VARCHAR) {
                semantic_type = SemanticTypes::LITERAL_STRING;
            } else if (value_type == LogicalType::INTEGER || 
                       value_type == LogicalType::BIGINT ||
                       value_type == LogicalType::DOUBLE ||
                       value_type.id() == LogicalTypeId::DECIMAL) {
                semantic_type = SemanticTypes::LITERAL_NUMBER;
            } else if (value_type == LogicalType::BOOLEAN) {
                semantic_type = SemanticTypes::LITERAL_ATOMIC;
            } else {
                semantic_type = SemanticTypes::LITERAL_ATOMIC;
            }
            
            auto node = CreateASTNode("literal", value, value, semantic_type,
                                     node_counter++, -1, 0);
            nodes.push_back(node);
            break;
        }
        case ExpressionType::COMPARE_EQUAL:
        case ExpressionType::COMPARE_NOTEQUAL:
        case ExpressionType::COMPARE_LESSTHAN:
        case ExpressionType::COMPARE_GREATERTHAN:
        case ExpressionType::COMPARE_LESSTHANOREQUALTO:
        case ExpressionType::COMPARE_GREATERTHANOREQUALTO: {
            const auto& comp_expr = expr.Cast<ComparisonExpression>();
            
            // Create comparison operator node
            auto comp_node = CreateASTNode("comparison", "", expr.ToString(),
                                          SemanticTypes::OPERATOR_COMPARISON,
                                          node_counter++, -1, 0);
            nodes.push_back(comp_node);
            
            // Process left and right operands
            if (comp_expr.left) {
                auto left_nodes = ConvertExpression(*comp_expr.left, node_counter);
                for (auto& ln : left_nodes) {
                    if (ln.structure.parent_id == -1) {
                        ln.structure.parent_id = comp_node.node_id;
                        ln.UpdateComputedLegacyFields();
                    }
                    nodes.push_back(ln);
                }
            }
            if (comp_expr.right) {
                auto right_nodes = ConvertExpression(*comp_expr.right, node_counter);
                for (auto& rn : right_nodes) {
                    if (rn.structure.parent_id == -1) {
                        rn.structure.parent_id = comp_node.node_id;
                        rn.UpdateComputedLegacyFields();
                    }
                    nodes.push_back(rn);
                }
            }
            break;
        }
        case ExpressionType::CONJUNCTION_AND:
        case ExpressionType::CONJUNCTION_OR: {
            const auto& conj_expr = expr.Cast<ConjunctionExpression>();
            
            // Create conjunction operator node
            auto conj_node = CreateASTNode("conjunction", "", expr.ToString(),
                                          SemanticTypes::OPERATOR_LOGICAL,
                                          node_counter++, -1, 0);
            nodes.push_back(conj_node);
            
            // Process all child expressions
            for (const auto& child : conj_expr.children) {
                if (!child) continue;
                auto child_nodes = ConvertExpression(*child, node_counter);
                for (auto& cn : child_nodes) {
                    if (cn.structure.parent_id == -1) {
                        cn.structure.parent_id = conj_node.node_id;
                        cn.UpdateComputedLegacyFields();
                    }
                    nodes.push_back(cn);
                }
            }
            break;
        }
        default: {
            // Fallback to generic expression node - avoid re-parsing for now to prevent infinite loops
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
                if (ln.structure.parent_id == -1) {
                    ln.structure.parent_id = node.node_id;
                    ln.UpdateComputedLegacyFields();
                }
                nodes.push_back(ln);
            }
            for (auto& rn : right_nodes) {
                if (rn.structure.parent_id == -1) {
                    rn.structure.parent_id = node.node_id;
                    rn.UpdateComputedLegacyFields();
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
    
    // STRUCTURED FIELDS (Primary data)
    // Context information
    node.context.name = name;
    node.context.normalized.semantic_type = semantic_type;
    node.context.normalized.universal_flags = 0;
    node.context.normalized.arity_bin = 0;
    
    // Source location (placeholder values for now)
    node.source.start_line = 1;
    node.source.end_line = 1;
    node.source.start_column = 1;
    node.source.end_column = 1;
    
    // Tree structure
    node.structure.parent_id = parent_id;
    node.structure.depth = depth;
    node.structure.sibling_index = 0;
    node.structure.children_count = 0;
    node.structure.descendant_count = 0;
    
    // Content preview
    node.peek = value;
    
    // Compute legacy fields from structured fields for backward compatibility
    node.UpdateComputedLegacyFields();
    
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
            if (other.structure.parent_id == static_cast<int64_t>(node.node_id)) {
                count += 1 + other.structure.descendant_count;
            }
        }
        node.structure.descendant_count = count;
        
        // Update legacy fields after descendant count change
        node.UpdateComputedLegacyFields();
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
    // Use DuckDB's internal function names as-is - they are the authoritative source
    return internal_name;
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