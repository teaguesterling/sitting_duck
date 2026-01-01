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
// DuckDB Native Parser Adapter - Architecture Plan Implementation
//
// This adapter provides SQL parsing using DuckDB's native parser instead of
// tree-sitter, enabling database-native accuracy for SQL AST analysis.
//
// Credit: Inspired by zacMode's duckdb_extension_parser_tools
// (https://github.com/zacMode/duckdb_extension_parser_tools)
//==============================================================================

class DuckDBAdapter : public LanguageAdapter {
public:
	// LanguageAdapter interface implementation
	string GetLanguageName() const override;
	vector<string> GetAliases() const override;
	void InitializeParser() const override;
	unique_ptr<TSParserWrapper> CreateFreshParser() const override;
	string GetNormalizedType(const string &node_type) const override;
	string ExtractNodeName(TSNode node, const string &content) const override;
	string ExtractNodeValue(TSNode node, const string &content) const override;
	bool IsPublicNode(TSNode node, const string &content) const override;
	uint8_t GetNodeFlags(const string &node_type) const override;
	const NodeConfig *GetNodeConfig(const string &node_type) const override;
	ParsingFunction GetParsingFunction() const override;

	// DuckDB-specific parsing function
	ASTResult ParseSQL(const string &sql_content) const;

private:
	// Core conversion methods (Architecture Plan Section 4.2)
	ASTResult ConvertStatementsToAST(const vector<unique_ptr<SQLStatement>> &statements, const string &content) const;

	// Statement converters (Architecture Plan Section 5.1)
	vector<ASTNode> ConvertStatement(const SQLStatement &stmt, uint32_t &node_counter) const;
	vector<ASTNode> ConvertSelectStatement(const SelectStatement &stmt, uint32_t &node_counter) const;
	vector<ASTNode> ConvertSelectNode(const SelectNode &node, uint32_t &node_counter) const;

	// Expression and table reference converters
	vector<ASTNode> ConvertExpression(const ParsedExpression &expr, uint32_t &node_counter) const;
	vector<ASTNode> ConvertTableRef(const TableRef &table_ref, uint32_t &node_counter) const;

	// Utility functions (Architecture Plan Section 5.3)
	ASTNode CreateASTNode(const string &type, const string &name, const string &value, uint8_t semantic_type,
	                      uint32_t node_id, int64_t parent_id, uint32_t depth) const;

	ASTResult CreateErrorResult(const string &error_message) const;
	void UpdateDescendantCounts(vector<ASTNode> &nodes) const;
	uint32_t CalculateMaxDepth(const vector<ASTNode> &nodes) const;
};

} // namespace duckdb
