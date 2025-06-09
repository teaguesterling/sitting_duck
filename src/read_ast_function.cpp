#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "unified_ast_backend.hpp"
#include "read_ast_objects_hybrid.hpp"

namespace duckdb {

struct ReadASTData : public TableFunctionData {
	Value file_path_value;
	string language;
	bool ignore_errors;
	ASTResultCollection collection;
	idx_t current_result_index = 0;
	idx_t current_node_index = 0;
	bool parsed = false;
	
	ReadASTData(Value file_path_value, string language, bool ignore_errors = false) 
		: file_path_value(std::move(file_path_value)), language(std::move(language)), ignore_errors(ignore_errors) {}
};

// Bind function for two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTBindTwoArg(ClientContext &context, TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.size() != 2) {
		throw BinderException("read_ast requires exactly 2 arguments: file_path and language");
	}
	
	auto file_path_value = input.inputs[0];
	auto language = input.inputs[1].GetValue<string>();
	
	// Parse optional named parameters
	bool ignore_errors = false;
	if (input.named_parameters.find("ignore_errors") != input.named_parameters.end()) {
		ignore_errors = input.named_parameters.at("ignore_errors").GetValue<bool>();
	}
	
	// Use unified backend schema (includes taxonomy fields)
	return_types = UnifiedASTBackend::GetFlatTableSchema();
	names = UnifiedASTBackend::GetFlatTableColumnNames();
	
	auto result = make_uniq<ReadASTData>(file_path_value, language, ignore_errors);
	return std::move(result);
}

// Bind function for one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTBindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.size() != 1) {
		throw BinderException("read_ast requires exactly 1 argument: file_path");
	}
	
	auto file_path_value = input.inputs[0];
	
	// Parse optional named parameters
	bool ignore_errors = false;
	if (input.named_parameters.find("ignore_errors") != input.named_parameters.end()) {
		ignore_errors = input.named_parameters.at("ignore_errors").GetValue<bool>();
	}
	
	// Use auto-detect for language
	string language = "auto";
	
	// Use unified backend schema (includes taxonomy fields)
	return_types = UnifiedASTBackend::GetFlatTableSchema();
	names = UnifiedASTBackend::GetFlatTableColumnNames();
	
	auto result = make_uniq<ReadASTData>(file_path_value, language, ignore_errors);
	return std::move(result);
}

static void ReadASTFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->CastNoConst<ReadASTData>();
	
	// Parse the file(s) if not already done
	if (!data.parsed) {
		try {
			// Use unified parsing backend with glob support
			data.collection = UnifiedASTBackend::ParseFilesToASTCollection(context, data.file_path_value, data.language, data.ignore_errors);
			data.parsed = true;
		} catch (const Exception &e) {
			throw IOException("Failed to parse files: " + string(e.what()));
		}
	}
	
	// Project to table format by iterating through each result in the collection
	idx_t output_index = 0;
	
	while (data.current_result_index < data.collection.results.size() && output_index < STANDARD_VECTOR_SIZE) {
		auto &current_result = data.collection.results[data.current_result_index];
		
		// Project nodes from the current result
		UnifiedASTBackend::ProjectToTable(current_result, output, data.current_node_index, output_index);
		
		// Check if we've finished this result
		if (data.current_node_index >= current_result.nodes.size()) {
			// Move to next result
			data.current_result_index++;
			data.current_node_index = 0;
		}
		
		// If ProjectToTable didn't add any rows, we're done
		if (output_index == 0) {
			break;
		}
	}
	
	output.SetCardinality(output_index);
}

static TableFunction GetReadASTFunctionTwoArg() {
	TableFunction read_ast("read_ast", {LogicalType::ANY, LogicalType::VARCHAR}, 
	                      ReadASTFunction, ReadASTBindTwoArg);
	read_ast.name = "read_ast";
	read_ast.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	return read_ast;
}

static TableFunction GetReadASTFunctionOneArg() {
	TableFunction read_ast("read_ast", {LogicalType::ANY}, 
	                      ReadASTFunction, ReadASTBindOneArg);
	read_ast.name = "read_ast";
	read_ast.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	return read_ast;
}

void RegisterReadASTFunction(DatabaseInstance &instance) {
	// Register both one-argument and two-argument versions
	ExtensionUtil::RegisterFunction(instance, GetReadASTFunctionOneArg());
	ExtensionUtil::RegisterFunction(instance, GetReadASTFunctionTwoArg());
}

} // namespace duckdb