#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "unified_ast_backend.hpp"
#include "read_ast_objects_hybrid.hpp"

namespace duckdb {

// Simple row structure for flattened output
struct ASTRow {
	string file_path;
	ASTNode node;
};

struct ReadASTBatchData : public TableFunctionData {
	Value file_path_value;
	string language;
	bool ignore_errors;
	int32_t peek_size;
	string peek_mode;
	vector<ASTRow> all_rows;  // Flattened: all nodes from all files
	idx_t current_index = 0;
	bool parsed = false;
	
	ReadASTBatchData(Value file_path_value, string language, bool ignore_errors = false, int32_t peek_size = 120, string peek_mode = "auto") 
		: file_path_value(std::move(file_path_value)), language(std::move(language)), ignore_errors(ignore_errors), 
		  peek_size(peek_size), peek_mode(std::move(peek_mode)) {}
};

// Bind function for two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTBatchBindTwoArg(ClientContext &context, TableFunctionBindInput &input,
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
	
	int32_t peek_size = 120;  // Default 120 characters
	if (input.named_parameters.find("peek_size") != input.named_parameters.end()) {
		peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
	}
	
	string peek_mode = "auto";  // Default auto mode
	if (input.named_parameters.find("peek_mode") != input.named_parameters.end()) {
		peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
	}
	
	// Use unified backend schema (includes taxonomy fields)
	return_types = UnifiedASTBackend::GetFlatTableSchema();
	names = UnifiedASTBackend::GetFlatTableColumnNames();
	
	auto result = make_uniq<ReadASTBatchData>(file_path_value, language, ignore_errors, peek_size, peek_mode);
	return std::move(result);
}

// Bind function for one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTBatchBindOneArg(ClientContext &context, TableFunctionBindInput &input,
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
	
	int32_t peek_size = 120;  // Default 120 characters
	if (input.named_parameters.find("peek_size") != input.named_parameters.end()) {
		peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
	}
	
	string peek_mode = "auto";  // Default auto mode
	if (input.named_parameters.find("peek_mode") != input.named_parameters.end()) {
		peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
	}
	
	// Use auto-detect for language
	string language = "auto";
	
	// Use unified backend schema (includes taxonomy fields)
	return_types = UnifiedASTBackend::GetFlatTableSchema();
	names = UnifiedASTBackend::GetFlatTableColumnNames();
	
	auto result = make_uniq<ReadASTBatchData>(file_path_value, language, ignore_errors, peek_size, peek_mode);
	return std::move(result);
}

static void ReadASTBatchFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->CastNoConst<ReadASTBatchData>();
	
	// Parse the file(s) if not already done
	if (!data.parsed) {
		try {
			// Parse all files and flatten into simple row structure
			auto collection = UnifiedASTBackend::ParseFilesToASTCollection(context, data.file_path_value, data.language, data.ignore_errors, data.peek_size, data.peek_mode);
			
			// Flatten all results into independent rows
			data.all_rows.clear();
			for (const auto& result : collection.results) {
				for (const auto& node : result.nodes) {
					data.all_rows.push_back({result.source.file_path, node});
				}
			}
			
			data.parsed = true;
		} catch (const Exception &e) {
			throw IOException("Failed to parse files: " + string(e.what()));
		}
	}
	
	// Simple linear iteration through all rows
	idx_t count = 0;
	idx_t max_count = STANDARD_VECTOR_SIZE;
	
	// Get output vectors once
	auto node_id_vec = FlatVector::GetData<int64_t>(output.data[0]);
	auto type_vec = FlatVector::GetData<string_t>(output.data[1]);
	auto name_vec = FlatVector::GetData<string_t>(output.data[2]);
	auto file_path_vec = FlatVector::GetData<string_t>(output.data[3]);
	auto start_line_vec = FlatVector::GetData<int32_t>(output.data[4]);
	auto start_column_vec = FlatVector::GetData<int32_t>(output.data[5]);
	auto end_line_vec = FlatVector::GetData<int32_t>(output.data[6]);
	auto end_column_vec = FlatVector::GetData<int32_t>(output.data[7]);
	auto parent_id_vec = FlatVector::GetData<int64_t>(output.data[8]);
	auto depth_vec = FlatVector::GetData<int32_t>(output.data[9]);
	auto sibling_index_vec = FlatVector::GetData<int32_t>(output.data[10]);
	auto children_count_vec = FlatVector::GetData<int32_t>(output.data[11]);
	auto descendant_count_vec = FlatVector::GetData<int32_t>(output.data[12]);
	auto peek_vec = FlatVector::GetData<string_t>(output.data[13]);
	auto semantic_type_vec = FlatVector::GetData<int8_t>(output.data[14]);
	auto universal_flags_vec = FlatVector::GetData<int8_t>(output.data[15]);
	auto arity_bin_vec = FlatVector::GetData<int8_t>(output.data[16]);
	
	// Get validity masks
	auto &name_validity = FlatVector::Validity(output.data[2]);
	auto &parent_validity = FlatVector::Validity(output.data[8]);
	auto &peek_validity = FlatVector::Validity(output.data[13]);
	
	// Process rows starting from current_index
	while (data.current_index < data.all_rows.size() && count < max_count) {
		const auto& row = data.all_rows[data.current_index];
		const auto& node = row.node;
		
		// Fill output vectors directly
		node_id_vec[count] = node.node_id;
		type_vec[count] = StringVector::AddString(output.data[1], node.type.raw);
		
		if (node.name.raw.empty()) {
			name_validity.SetInvalid(count);
		} else {
			name_vec[count] = StringVector::AddString(output.data[2], node.name.raw);
		}
		
		file_path_vec[count] = StringVector::AddString(output.data[3], row.file_path);
		start_line_vec[count] = node.file_position.start_line;
		start_column_vec[count] = node.file_position.start_column;
		end_line_vec[count] = node.file_position.end_line;
		end_column_vec[count] = node.file_position.end_column;
		
		if (node.tree_position.parent_index < 0) {
			parent_validity.SetInvalid(count);
		} else {
			parent_id_vec[count] = node.tree_position.parent_index;
		}
		
		depth_vec[count] = node.tree_position.node_depth;
		sibling_index_vec[count] = node.tree_position.sibling_index;
		children_count_vec[count] = node.subtree.children_count;
		descendant_count_vec[count] = node.subtree.descendant_count;
		
		// Handle peek with NULL support
		if (node.peek.empty()) {
			peek_validity.SetInvalid(count);
		} else {
			peek_vec[count] = StringVector::AddString(output.data[13], node.peek);
		}
		
		semantic_type_vec[count] = node.semantic_type;
		universal_flags_vec[count] = node.universal_flags;
		arity_bin_vec[count] = node.arity_bin;
		
		count++;
		data.current_index++;
	}
	
	output.SetCardinality(count);
}

static TableFunction GetReadASTBatchFunctionTwoArg() {
	TableFunction read_ast_batch("read_ast_batch", {LogicalType::ANY, LogicalType::VARCHAR}, 
	                            ReadASTBatchFunction, ReadASTBatchBindTwoArg);
	read_ast_batch.name = "read_ast_batch";
	read_ast_batch.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_batch.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_batch.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast_batch;
}

static TableFunction GetReadASTBatchFunctionOneArg() {
	TableFunction read_ast_batch("read_ast_batch", {LogicalType::ANY}, 
	                            ReadASTBatchFunction, ReadASTBatchBindOneArg);
	read_ast_batch.name = "read_ast_batch";
	read_ast_batch.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_batch.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_batch.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast_batch;
}

void RegisterReadASTBatchFunction(DatabaseInstance &instance) {
	// Register both one-argument and two-argument versions
	ExtensionUtil::RegisterFunction(instance, GetReadASTBatchFunctionOneArg());
	ExtensionUtil::RegisterFunction(instance, GetReadASTBatchFunctionTwoArg());
}

} // namespace duckdb