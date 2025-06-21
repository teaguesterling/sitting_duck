#pragma once

#include "language_adapter.hpp"
#include "ast_type.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/parser/query_node/select_node.hpp"
#include "duckdb/parser/tableref.hpp"
#include "duckdb/parser/parsed_expression.hpp"
#include <mutex>

namespace duckdb {

//==============================================================================
// DuckDB Native Parser Adapter
// 
// This adapter provides SQL parsing using DuckDB's native parser instead of
// tree-sitter, enabling database-native accuracy for SQL AST analysis.
// 
// Credit: Inspired by zacMode's duckdb_extension_parser_tools
// (https://github.com/zacMode/duckdb_extension_parser_tools)
//==============================================================================

class DuckDBAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    uint8_t GetNodeFlags(const string &node_type) const override;
    const NodeConfig* GetNodeConfig(const string &node_type) const override;
    ParsingFunction GetParsingFunction() const override;
    
    // DuckDB-specific parsing function
    ASTResult ParseSQL(const string& sql_content) const;

private:
    mutable unique_ptr<Parser> parser_;
    mutable std::once_flag parser_init_flag_;
    
    // Synthetic AST node for DuckDB integration
    struct DuckDBASTNode {
        string type;
        string name;
        string value;
        uint8_t semantic_type;
        uint8_t flags;
        vector<DuckDBASTNode> children;
        uint32_t start_line;
        uint32_t start_col;
        uint32_t end_line;
        uint32_t end_col;
        
        DuckDBASTNode(const string& node_type, uint8_t sem_type, uint8_t node_flags = 0)
            : type(node_type), semantic_type(sem_type), flags(node_flags), 
              start_line(1), start_col(1), end_line(1), end_col(1) {}
    };
    
    // Convert DuckDB AST to our synthetic AST
    DuckDBASTNode ConvertSelectStatement(const SelectStatement& stmt) const;
    DuckDBASTNode ConvertSelectNode(const SelectNode& node) const;
    DuckDBASTNode ConvertTableRef(const TableRef& table_ref) const;
    DuckDBASTNode ConvertExpression(const ParsedExpression& expr) const;
    
    // Generate flattened AST result compatible with our interface
    ASTResult GenerateASTResult(const DuckDBASTNode& root, const string& content) const;
    void FlattenNode(const DuckDBASTNode& node, vector<ASTNode>& nodes, 
                     int64_t parent_id, uint32_t depth, int64_t& node_counter) const;
    
    // SQL statement type converters
    ASTResult ProcessStatements(const vector<unique_ptr<SQLStatement>>& statements, const string& content) const;
    ASTResult CreateErrorResult(const string& error_message) const;
};

} // namespace duckdb