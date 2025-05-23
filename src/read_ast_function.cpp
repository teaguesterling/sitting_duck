#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "ast_parser.hpp"

namespace duckdb {

struct ReadASTData : public TableFunctionData {
	string file_path;
	string language;
	vector<ASTNode> nodes;
	idx_t current_index;
	
	ReadASTData() : current_index(0) {}
};

static unique_ptr<FunctionData> ReadASTBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<string> &names) {
	auto result = make_uniq<ReadASTData>();
	
	// Validate arguments
	if (input.inputs.size() != 2) {
		throw BinderException("read_ast requires exactly 2 arguments: file_path and language");
	}
	
	// Get file path
	if (input.inputs[0].IsNull()) {
		throw BinderException("file_path cannot be NULL");
	}
	result->file_path = StringValue::Get(input.inputs[0]);
	
	// Get language
	if (input.inputs[1].IsNull()) {
		throw BinderException("language cannot be NULL");
	}
	result->language = StringUtil::Lower(StringValue::Get(input.inputs[1]));
	
	// Define output schema
	names = {"node_id", "type", "name", "file_path", "start_line", "start_column", 
	         "end_line", "end_column", "parent_id", "depth", "sibling_index", "source_text"};
	
	return_types = {
		LogicalType::BIGINT,      // node_id
		LogicalType::VARCHAR,     // type
		LogicalType::VARCHAR,     // name
		LogicalType::VARCHAR,     // file_path
		LogicalType::INTEGER,     // start_line
		LogicalType::INTEGER,     // start_column
		LogicalType::INTEGER,     // end_line
		LogicalType::INTEGER,     // end_column
		LogicalType::BIGINT,      // parent_id (nullable)
		LogicalType::INTEGER,     // depth
		LogicalType::INTEGER,     // sibling_index
		LogicalType::VARCHAR      // source_text
	};
	
	return std::move(result);
}

static void ReadASTFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->CastNoConst<ReadASTData>();
	
	// Parse the file if not already done
	if (data.nodes.empty() && data.current_index == 0) {
		try {
			TreeSitterParser parser(data.language);
			data.nodes = parser.ParseFile(data.file_path);
		} catch (std::exception &e) {
			throw IOException("Failed to parse file: %s", e.what());
		}
	}
	
	// Fill output chunk
	idx_t count = 0;
	idx_t max_count = STANDARD_VECTOR_SIZE;
	
	while (data.current_index < data.nodes.size() && count < max_count) {
		auto &node = data.nodes[data.current_index];
		
		// node_id
		output.SetValue(0, count, Value::BIGINT(node.node_id));
		
		// type
		output.SetValue(1, count, Value(node.type));
		
		// name
		output.SetValue(2, count, node.name.empty() ? Value(LogicalType::VARCHAR) : Value(node.name));
		
		// file_path
		output.SetValue(3, count, Value(node.file_path));
		
		// start_line
		output.SetValue(4, count, Value::INTEGER(node.start_line));
		
		// start_column
		output.SetValue(5, count, Value::INTEGER(node.start_column));
		
		// end_line
		output.SetValue(6, count, Value::INTEGER(node.end_line));
		
		// end_column
		output.SetValue(7, count, Value::INTEGER(node.end_column));
		
		// parent_id (nullable)
		if (node.parent_id == -1) {
			output.SetValue(8, count, Value(LogicalType::BIGINT));
		} else {
			output.SetValue(8, count, Value::BIGINT(node.parent_id));
		}
		
		// depth
		output.SetValue(9, count, Value::INTEGER(node.depth));
		
		// sibling_index
		output.SetValue(10, count, Value::INTEGER(node.sibling_index));
		
		// source_text
		output.SetValue(11, count, Value(node.source_text));
		
		data.current_index++;
		count++;
	}
	
	output.SetCardinality(count);
}

void RegisterReadASTFunction(DatabaseInstance &instance) {
	TableFunction read_ast_func("read_ast", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
	                           ReadASTFunction, ReadASTBind);
	
	ExtensionUtil::RegisterFunction(instance, read_ast_func);
}

} // namespace duckdb