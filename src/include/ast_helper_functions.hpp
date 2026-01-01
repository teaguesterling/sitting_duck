#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "ast_type.hpp"

namespace duckdb {

// Base class for AST helper functions
class ASTHelperFunction {
protected:
	static unique_ptr<ASTType> ParseASTFromJSON(const string &json);
};

// Extract functions from AST
class ASTFunctionsFunction : public ASTHelperFunction {
public:
	static TableFunction GetFunction();

private:
	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names);
	static void Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);
};

// Extract classes from AST
class ASTClassesFunction : public ASTHelperFunction {
public:
	static TableFunction GetFunction();

private:
	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names);
	static void Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);
};

// Extract imports from AST
class ASTImportsFunction : public ASTHelperFunction {
public:
	static TableFunction GetFunction();

private:
	static unique_ptr<FunctionData> Bind(ClientContext &context, TableFunctionBindInput &input,
	                                     vector<LogicalType> &return_types, vector<string> &names);
	static void Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output);
};

} // namespace duckdb
