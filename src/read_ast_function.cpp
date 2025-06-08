#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "unified_ast_backend.hpp"
#include "read_ast_objects_hybrid.hpp"

namespace duckdb {

struct ReadASTData : public TableFunctionData {
	string file_path;
	string language;
	ASTResult result;
	idx_t current_index = 0;
	bool parsed = false;
	
	ReadASTData(string file_path, string language) 
		: file_path(std::move(file_path)), language(std::move(language)) {}
};

// Bind function for two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTBindTwoArg(ClientContext &context, TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.size() != 2) {
		throw BinderException("read_ast requires exactly 2 arguments: file_path and language");
	}
	
	auto file_path = input.inputs[0].GetValue<string>();
	auto language = input.inputs[1].GetValue<string>();
	
	// Use unified backend schema (includes taxonomy fields)
	return_types = UnifiedASTBackend::GetFlatTableSchema();
	names = UnifiedASTBackend::GetFlatTableColumnNames();
	
	auto result = make_uniq<ReadASTData>(file_path, language);
	return std::move(result);
}

// Bind function for one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTBindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.size() != 1) {
		throw BinderException("read_ast requires exactly 1 argument: file_path");
	}
	
	auto file_path = input.inputs[0].GetValue<string>();
	
	// Auto-detect language from file extension
	string language = DetectLanguageFromExtension(file_path);
	if (language == "auto") {
		throw BinderException("Could not detect language from file extension for '" + file_path + "'. Please specify language explicitly.");
	}
	
	// Use unified backend schema (includes taxonomy fields)
	return_types = UnifiedASTBackend::GetFlatTableSchema();
	names = UnifiedASTBackend::GetFlatTableColumnNames();
	
	auto result = make_uniq<ReadASTData>(file_path, language);
	return std::move(result);
}

static void ReadASTFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->CastNoConst<ReadASTData>();
	
	// Parse the file if not already done
	if (!data.parsed) {
		try {
			// Read file content
			auto &fs = FileSystem::GetFileSystem(context);
			auto handle = fs.OpenFile(data.file_path, FileFlags::FILE_FLAGS_READ);
			auto file_size = fs.GetFileSize(*handle);
			
			string content;
			content.resize(file_size);
			fs.Read(*handle, (void*)content.data(), file_size);
			
			// Use unified parsing backend
			data.result = UnifiedASTBackend::ParseToASTResult(content, data.language, data.file_path);
			data.parsed = true;
		} catch (const Exception &e) {
			throw IOException("Failed to parse file '" + data.file_path + "': " + string(e.what()));
		}
	}
	
	// Project to table format using unified backend
	idx_t output_index = 0;
	UnifiedASTBackend::ProjectToTable(data.result, output, data.current_index, output_index);
	output.SetCardinality(output_index);
}

static TableFunction GetReadASTFunctionTwoArg() {
	TableFunction read_ast("read_ast", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
	                      ReadASTFunction, ReadASTBindTwoArg);
	read_ast.name = "read_ast";
	return read_ast;
}

static TableFunction GetReadASTFunctionOneArg() {
	TableFunction read_ast("read_ast", {LogicalType::VARCHAR}, 
	                      ReadASTFunction, ReadASTBindOneArg);
	read_ast.name = "read_ast";
	return read_ast;
}

void RegisterReadASTFunction(DatabaseInstance &instance) {
	// Register both one-argument and two-argument versions
	ExtensionUtil::RegisterFunction(instance, GetReadASTFunctionOneArg());
	ExtensionUtil::RegisterFunction(instance, GetReadASTFunctionTwoArg());
}

} // namespace duckdb