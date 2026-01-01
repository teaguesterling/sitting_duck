#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>
#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;

namespace duckdb {

// Forward declaration
class SQLAdapter;

//==============================================================================
// SQL Native Context Extraction
//==============================================================================

template <NativeExtractionStrategy Strategy>
struct SQLNativeExtractor {
	static NativeContext Extract(TSNode node, const string &content);
};

//==============================================================================
// SQL Function/Query Extraction (for CREATE statements, functions, views)
//==============================================================================

template <>
struct SQLNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		try {
			string node_type = ts_node_type(node);

			if (node_type == "create_table") {
				context.signature_type = "TABLE";
				context.parameters = ExtractTableColumns(node, content);
				context.modifiers = ExtractTableModifiers(node, content);
			} else if (node_type == "create_view") {
				context.signature_type = "VIEW";
				context.parameters = ExtractViewColumns(node, content);
				context.modifiers = ExtractViewModifiers(node, content);
			} else if (node_type == "function_call") {
				context.signature_type = "FUNCTION";
				context.parameters = ExtractFunctionArguments(node, content);
				context.modifiers.clear();
			} else if (node_type == "window_function") {
				context.signature_type = "WINDOW_FUNCTION";
				context.parameters = ExtractWindowFunctionArgs(node, content);
				context.modifiers = ExtractWindowModifiers(node, content);
			} else if (node_type == "select_statement") {
				context.signature_type = DetermineSelectType(node, content);
				context.parameters = ExtractSelectParameters(node, content);
				context.modifiers = ExtractSelectModifiers(node, content);
			} else if (node_type == "insert_statement") {
				context.signature_type = "INSERT";
				context.parameters = ExtractInsertParameters(node, content);
				context.modifiers.clear();
			} else if (node_type == "update_statement") {
				context.signature_type = "UPDATE";
				context.parameters = ExtractUpdateParameters(node, content);
				context.modifiers.clear();
			} else if (node_type == "delete_statement") {
				context.signature_type = "DELETE";
				context.parameters = ExtractDeleteParameters(node, content);
				context.modifiers.clear();
			} else if (node_type == "statement") {
				context.signature_type = "STATEMENT";
				context.parameters.clear();
				context.modifiers.clear();
			} else if (node_type == "binary_expression") {
				context.signature_type = "BINARY_EXPR";
				context.parameters = ExtractBinaryExpressionOperands(node, content);
				context.modifiers = ExtractBinaryExpressionModifiers(node, content);
			} else if (node_type == "term") {
				context.signature_type = "TERM";
				context.parameters = ExtractTermComponents(node, content);
				context.modifiers.clear();
			} else if (node_type == "list") {
				context.signature_type = "LIST";
				context.parameters = ExtractListItems(node, content);
				context.modifiers.clear();
			} else {
				// Generic SQL construct
				context.signature_type = "SQL";
				context.parameters.clear();
				context.modifiers.clear();
			}
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}
		return context;
	}

	// Public static methods for reuse by other specializations
	static vector<ParameterInfo> ExtractTableColumns(TSNode node, const string &content) {
		vector<ParameterInfo> columns;

		// Find column_definitions or column_definition nodes
		TSNode column_defs = {0};
		uint32_t child_count = ts_node_child_count(node);

		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "column_definitions" || child_type == "column_definition") {
				column_defs = child;
				break;
			}
		}

		if (ts_node_is_null(column_defs)) {
			return columns;
		}

		// Extract individual columns
		if (string(ts_node_type(column_defs)) == "column_definitions") {
			uint32_t col_count = ts_node_child_count(column_defs);
			for (uint32_t i = 0; i < col_count; i++) {
				TSNode col = ts_node_child(column_defs, i);
				if (string(ts_node_type(col)) == "column_definition") {
					ParameterInfo info = ExtractColumnInfo(col, content);
					if (!info.name.empty()) {
						columns.push_back(info);
					}
				}
			}
		} else {
			// Single column definition
			ParameterInfo info = ExtractColumnInfo(column_defs, content);
			if (!info.name.empty()) {
				columns.push_back(info);
			}
		}

		return columns;
	}

	static ParameterInfo ExtractColumnInfo(TSNode col_def, const string &content) {
		ParameterInfo info;
		info.is_optional = false;
		info.is_variadic = false;

		uint32_t child_count = ts_node_child_count(col_def);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(col_def, i);
			string child_type = ts_node_type(child);

			if (child_type == "identifier" || child_type == "column") {
				info.name = ExtractNodeText(child, content);
			} else if (child_type == "bigint" || child_type == "varchar" || child_type == "int" ||
			           child_type == "decimal" || child_type == "timestamp" || child_type == "text" ||
			           child_type.find("keyword_") == 0) {
				info.type = ExtractNodeText(child, content);
			}
		}

		return info;
	}

	static vector<ParameterInfo> ExtractViewColumns(TSNode node, const string &content) {
		vector<ParameterInfo> columns;

		// For views, we could extract the SELECT clause columns
		// This is a simplified implementation
		TSNode select_stmt = FindChildByType(node, "select_statement");
		if (!ts_node_is_null(select_stmt)) {
			// Extract columns from SELECT clause
			columns = ExtractSelectColumns(select_stmt, content);
		}

		return columns;
	}

	static vector<ParameterInfo> ExtractSelectColumns(TSNode select_stmt, const string &content) {
		vector<ParameterInfo> columns;

		// Find the select expression list
		TSNode select_expr = FindChildByType(select_stmt, "select_expression");
		if (!ts_node_is_null(select_expr)) {
			uint32_t child_count = ts_node_child_count(select_expr);
			for (uint32_t i = 0; i < child_count; i++) {
				TSNode child = ts_node_child(select_expr, i);
				string child_type = ts_node_type(child);

				if (child_type == "identifier" || child_type == "field" || child_type == "column_reference") {
					ParameterInfo info;
					info.name = ExtractNodeText(child, content);
					info.type = ""; // SQL columns don't have explicit types in SELECT
					info.is_optional = false;
					info.is_variadic = false;
					columns.push_back(info);
				}
			}
		}

		return columns;
	}

	static vector<ParameterInfo> ExtractFunctionArguments(TSNode node, const string &content) {
		vector<ParameterInfo> args;

		// Find function_arguments or argument_list
		TSNode arg_list = FindChildByType(node, "function_arguments");
		if (ts_node_is_null(arg_list)) {
			arg_list = FindChildByType(node, "argument_list");
		}

		if (!ts_node_is_null(arg_list)) {
			uint32_t child_count = ts_node_child_count(arg_list);
			for (uint32_t i = 0; i < child_count; i++) {
				TSNode child = ts_node_child(arg_list, i);
				string child_type = ts_node_type(child);

				if (child_type != "," && child_type != "(" && child_type != ")") {
					ParameterInfo info;
					info.name = ExtractNodeText(child, content);
					info.type = ""; // SQL function args don't have explicit types
					info.is_optional = false;
					info.is_variadic = false;
					args.push_back(info);
				}
			}
		}

		return args;
	}

	static vector<ParameterInfo> ExtractWindowFunctionArgs(TSNode node, const string &content) {
		vector<ParameterInfo> args;

		// Extract both function arguments and window specification
		args = ExtractFunctionArguments(node, content);

		// Add window specification details
		TSNode window_spec = FindChildByType(node, "window_specification");
		if (!ts_node_is_null(window_spec)) {
			ParameterInfo window_info;
			window_info.name = "window_spec";
			window_info.type = "WINDOW";
			window_info.is_optional = false;
			window_info.is_variadic = false;
			args.push_back(window_info);
		}

		return args;
	}

	static vector<string> ExtractTableModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Look for constraint keywords, temporary keywords, etc.
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "keyword_temporary" || child_type == "keyword_temp") {
				modifiers.push_back("TEMPORARY");
			} else if (child_type == "keyword_materialized") {
				modifiers.push_back("MATERIALIZED");
			}
		}

		return modifiers;
	}

	static vector<string> ExtractViewModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Look for view-specific modifiers
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "keyword_materialized") {
				modifiers.push_back("MATERIALIZED");
			} else if (child_type == "keyword_temporary") {
				modifiers.push_back("TEMPORARY");
			}
		}

		return modifiers;
	}

	static vector<string> ExtractWindowModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Look for window function modifiers
		TSNode window_spec = FindChildByType(node, "window_specification");
		if (!ts_node_is_null(window_spec)) {
			// Check for PARTITION BY, ORDER BY, etc.
			if (!ts_node_is_null(FindChildByType(window_spec, "partition_by"))) {
				modifiers.push_back("PARTITIONED");
			}
			if (!ts_node_is_null(FindChildByType(window_spec, "order_by"))) {
				modifiers.push_back("ORDERED");
			}
		}

		return modifiers;
	}

	static vector<ParameterInfo> ExtractSelectParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// Find SELECT clause columns
		TSNode select_expr = FindChildByType(node, "select_expression");
		if (!ts_node_is_null(select_expr)) {
			params = ExtractSelectColumns(select_expr, content);
		}

		return params;
	}

	static vector<string> ExtractSelectModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Check for DISTINCT, ORDER BY, LIMIT, etc.
		if (!ts_node_is_null(FindChildByType(node, "distinct"))) {
			modifiers.push_back("DISTINCT");
		}
		if (!ts_node_is_null(FindChildByType(node, "order_by"))) {
			modifiers.push_back("ORDERED");
		}
		if (!ts_node_is_null(FindChildByType(node, "limit"))) {
			modifiers.push_back("LIMITED");
		}
		if (!ts_node_is_null(FindChildByType(node, "where"))) {
			modifiers.push_back("FILTERED");
		}

		return modifiers;
	}

	static vector<ParameterInfo> ExtractInsertParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// For INSERT, we could extract the columns being inserted
		// This is a simplified implementation

		return params;
	}

	static vector<ParameterInfo> ExtractUpdateParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// For UPDATE, we could extract the columns being updated
		// This is a simplified implementation

		return params;
	}

	static vector<ParameterInfo> ExtractDeleteParameters(TSNode node, const string &content) {
		vector<ParameterInfo> params;

		// For DELETE, we could extract the conditions
		// This is a simplified implementation

		return params;
	}

	static vector<ParameterInfo> ExtractBinaryExpressionOperands(TSNode node, const string &content) {
		vector<ParameterInfo> operands;

		// Binary expressions typically have left and right operands
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			// Skip operators, focus on operands
			if (child_type != "=" && child_type != ">" && child_type != "<" && child_type != ">=" &&
			    child_type != "<=" && child_type != "!=" && child_type != "AND" && child_type != "OR") {
				ParameterInfo operand;
				operand.name = ExtractNodeText(child, content);
				operand.type = child_type;
				operand.is_optional = false;
				operand.is_variadic = false;
				operands.push_back(operand);
			}
		}

		return operands;
	}

	static vector<string> ExtractBinaryExpressionModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// Extract the operator type
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "=" || child_type == ">" || child_type == "<" || child_type == ">=" ||
			    child_type == "<=" || child_type == "!=") {
				modifiers.push_back("COMPARISON");
				break;
			} else if (child_type == "AND" || child_type == "OR") {
				modifiers.push_back("LOGICAL");
				break;
			}
		}

		return modifiers;
	}

	static vector<ParameterInfo> ExtractTermComponents(TSNode node, const string &content) {
		vector<ParameterInfo> components;

		// Terms can contain various SQL components
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type != "," && child_type != "(" && child_type != ")") {
				ParameterInfo component;
				component.name = ExtractNodeText(child, content);
				component.type = child_type;
				component.is_optional = false;
				component.is_variadic = false;
				components.push_back(component);
			}
		}

		return components;
	}

	static vector<ParameterInfo> ExtractListItems(TSNode node, const string &content) {
		vector<ParameterInfo> items;

		// Lists contain comma-separated items
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type != ",") {
				ParameterInfo item;
				item.name = ExtractNodeText(child, content);
				item.type = child_type;
				item.is_optional = false;
				item.is_variadic = false;
				items.push_back(item);
			}
		}

		return items;
	}

	static TSNode FindChildByType(TSNode parent, const string &type) {
		uint32_t child_count = ts_node_child_count(parent);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(parent, i);
			if (string(ts_node_type(child)) == type) {
				return child;
			}
		}
		return {0};
	}

	static string ExtractNodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node)) {
			return "";
		}

		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start >= content.length() || end > content.length() || start >= end) {
			return "";
		}

		return content.substr(start, end - start);
	}

	static string DetermineSelectType(TSNode node, const string &content) {
		// Analyze SELECT statement to determine its type/complexity
		string node_text = ExtractNodeText(node, content);

		// Check for complex query patterns
		if (node_text.find("GROUP BY") != string::npos) {
			return "SELECT_AGGREGATE";
		} else if (node_text.find("JOIN") != string::npos) {
			return "SELECT_JOIN";
		} else if (node_text.find("WINDOW") != string::npos || node_text.find("OVER") != string::npos) {
			return "SELECT_WINDOW";
		} else if (node_text.find("UNION") != string::npos) {
			return "SELECT_UNION";
		} else if (node_text.find("WITH") != string::npos) {
			return "SELECT_CTE";
		} else if (node_text.find("COUNT(") != string::npos || node_text.find("SUM(") != string::npos ||
		           node_text.find("AVG(") != string::npos || node_text.find("MAX(") != string::npos ||
		           node_text.find("MIN(") != string::npos) {
			return "SELECT_FUNCTION";
		} else {
			return "SELECT_SIMPLE";
		}
	}
};

//==============================================================================
// SQL Variable/Column Extraction
//==============================================================================

template <>
struct SQLNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		try {
			string node_type = ts_node_type(node);

			if (node_type == "column_definition") {
				context.signature_type = ExtractColumnType(node, content);
				context.parameters.clear();
				context.modifiers = ExtractColumnModifiers(node, content);
			} else if (node_type == "parameter") {
				context.signature_type = "PARAMETER";
				context.parameters.clear();
				context.modifiers.clear();
			} else if (node_type == "identifier") {
				context.signature_type = "IDENTIFIER";
				context.parameters.clear();
				context.modifiers.clear();
			} else if (node_type == "field") {
				context.signature_type = "FIELD";
				context.parameters.clear();
				context.modifiers.clear();
			} else if (node_type == "object_reference") {
				context.signature_type = "REFERENCE";
				context.parameters.clear();
				context.modifiers = ExtractReferenceModifiers(node, content);
			} else if (node_type == "column_reference") {
				context.signature_type = "COLUMN";
				context.parameters.clear();
				context.modifiers.clear();
			} else if (node_type == "table_reference") {
				context.signature_type = "TABLE";
				context.parameters.clear();
				context.modifiers.clear();
			} else if (node_type == "relation") {
				context.signature_type = "RELATION";
				context.parameters.clear();
				context.modifiers.clear();
			} else if (node_type == "literal" || node_type == "string_literal" || node_type == "number_literal" ||
			           node_type == "boolean_literal") {
				context.signature_type = "LITERAL";
				context.parameters.clear();
				context.modifiers =
				    SQLNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE>::ExtractLiteralModifiers(node,
				                                                                                              content);
			} else if (node_type == "column") {
				context.signature_type = "COLUMN";
				context.parameters.clear();
				context.modifiers.clear();
			} else if (node_type == "varchar" || node_type == "keyword_varchar" || node_type == "bigint" ||
			           node_type.find("keyword_") == 0) {
				context.signature_type = "TYPE";
				context.parameters.clear();
				context.modifiers =
				    SQLNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE>::ExtractTypeModifiers(node,
				                                                                                           content);
			} else {
				context.signature_type = "";
				context.parameters.clear();
				context.modifiers.clear();
			}
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}
		return context;
	}

	// Public static methods for reuse
	static string ExtractColumnType(TSNode node, const string &content) {
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "bigint" || child_type == "varchar" || child_type == "int" || child_type == "decimal" ||
			    child_type == "timestamp" || child_type == "text" || child_type == "smallint" ||
			    child_type == "double" || child_type == "float" || child_type == "char" || child_type == "time" ||
			    child_type == "interval") {
				return ExtractNodeText(child, content);
			}
		}
		return "";
	}

	static vector<string> ExtractColumnModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "keyword_primary" || child_type == "keyword_key") {
				modifiers.push_back("PRIMARY_KEY");
			} else if (child_type == "keyword_unique") {
				modifiers.push_back("UNIQUE");
			} else if (child_type == "keyword_not" || child_type == "keyword_null") {
				modifiers.push_back("NOT_NULL");
			} else if (child_type == "keyword_default") {
				modifiers.push_back("DEFAULT");
			}
		}

		return modifiers;
	}

	static vector<string> ExtractReferenceModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		// For object references like "table.column", we could analyze the structure
		string node_text = ExtractNodeText(node, content);
		if (node_text.find('.') != string::npos) {
			modifiers.push_back("QUALIFIED");
		}

		return modifiers;
	}

	static vector<string> ExtractLiteralModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		string node_type = ts_node_type(node);
		if (node_type == "string_literal") {
			modifiers.push_back("STRING");
		} else if (node_type == "number_literal") {
			modifiers.push_back("NUMBER");
		} else if (node_type == "boolean_literal") {
			modifiers.push_back("BOOLEAN");
		} else {
			modifiers.push_back("GENERIC");
		}

		return modifiers;
	}

	static vector<string> ExtractTypeModifiers(TSNode node, const string &content) {
		vector<string> modifiers;

		string node_type = ts_node_type(node);
		if (node_type.find("keyword_") == 0) {
			modifiers.push_back("KEYWORD");
		}

		string node_text = ExtractNodeText(node, content);
		if (node_text.find("VARCHAR") != string::npos) {
			modifiers.push_back("STRING_TYPE");
		} else if (node_text.find("INT") != string::npos) {
			modifiers.push_back("INTEGER_TYPE");
		}

		return modifiers;
	}

	static string ExtractNodeText(TSNode node, const string &content) {
		if (ts_node_is_null(node)) {
			return "";
		}

		uint32_t start = ts_node_start_byte(node);
		uint32_t end = ts_node_end_byte(node);

		if (start >= content.length() || end > content.length() || start >= end) {
			return "";
		}

		return content.substr(start, end - start);
	}
};

//==============================================================================
// SQL Class/Schema Extraction (for CREATE statements with schema definitions)
//==============================================================================

template <>
struct SQLNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		try {
			string node_type = ts_node_type(node);

			if (node_type == "create_table") {
				context.signature_type = "TABLE";
				context.parameters = ExtractTableColumns(node, content);
				context.modifiers = ExtractTableConstraints(node, content);
			} else if (node_type == "create_view") {
				context.signature_type = "VIEW";
				context.parameters = ExtractViewColumns(node, content);
				context.modifiers.clear();
			} else {
				context.signature_type = "";
				context.parameters.clear();
				context.modifiers.clear();
			}
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}
		return context;
	}

	// Reuse public methods from FUNCTION_WITH_PARAMS specialization
	static vector<ParameterInfo> ExtractTableColumns(TSNode node, const string &content) {
		return SQLNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractTableColumns(node, content);
	}

	static vector<ParameterInfo> ExtractViewColumns(TSNode node, const string &content) {
		return SQLNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractViewColumns(node, content);
	}

	static vector<string> ExtractTableConstraints(TSNode node, const string &content) {
		vector<string> constraints;

		// Look for constraint definitions
		uint32_t child_count = ts_node_child_count(node);
		for (uint32_t i = 0; i < child_count; i++) {
			TSNode child = ts_node_child(node, i);
			string child_type = ts_node_type(child);

			if (child_type == "constraint" || child_type == "constraints") {
				constraints.push_back("CONSTRAINT");
			} else if (child_type == "keyword_primary") {
				constraints.push_back("PRIMARY_KEY");
			} else if (child_type == "keyword_foreign") {
				constraints.push_back("FOREIGN_KEY");
			} else if (child_type == "keyword_unique") {
				constraints.push_back("UNIQUE");
			}
		}

		return constraints;
	}
};

//==============================================================================
// SQL Async/Stored Procedure Extraction
//==============================================================================

template <>
struct SQLNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		// SQL doesn't have async functions in the traditional sense
		// Return empty context for this strategy
		return NativeContext();
	}
};

//==============================================================================
// SQL Arrow Function Extraction (for lambda expressions or similar)
//==============================================================================

template <>
struct SQLNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
	static NativeContext Extract(TSNode node, const string &content) {
		// SQL doesn't have arrow functions
		// Return empty context for this strategy
		return NativeContext();
	}
};

//==============================================================================
// SQL Inheritance Extraction (for CREATE TYPE with inheritance)
//==============================================================================

template <>
struct SQLNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		try {
			string node_type = ts_node_type(node);

			if (node_type == "create_type") {
				context.signature_type = "TYPE";
				context.parameters.clear();
				context.modifiers.clear();
			} else {
				context.signature_type = "";
				context.parameters.clear();
				context.modifiers.clear();
			}
		} catch (...) {
			context.signature_type = "";
			context.parameters.clear();
			context.modifiers.clear();
		}
		return context;
	}
};

//==============================================================================
// SQL Function with Decorators (for stored procedures with attributes)
//==============================================================================

template <>
struct SQLNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_DECORATORS> {
	static NativeContext Extract(TSNode node, const string &content) {
		// SQL doesn't typically have decorators like Python
		// Return empty context for this strategy
		return NativeContext();
	}
};

// Specialization for FUNCTION_CALL (SQL function calls)
template <>
struct SQLNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "sql_function_call"; // Placeholder
		return context;
	}
};

// Specialization for CUSTOM (SQL custom extraction)
template <>
struct SQLNativeExtractor<NativeExtractionStrategy::CUSTOM> {
	static NativeContext Extract(TSNode node, const string &content) {
		NativeContext context;
		context.signature_type = "sql_custom"; // Placeholder
		return context;
	}
};

// Note: Template trait specialization for SQLAdapter is defined in native_context_extraction.hpp

} // namespace duckdb
