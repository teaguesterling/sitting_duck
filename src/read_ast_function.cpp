#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "ast_parser.hpp"
#include "ast_type.hpp"

namespace duckdb {

struct ReadASTData : public TableFunctionData {
	string file_path;
	string language;
	vector<ASTNode> nodes;
	idx_t current_index = 0;
	
	ReadASTData(string file_path, string language) 
		: file_path(std::move(file_path)), language(std::move(language)) {}
};

static unique_ptr<FunctionData> ReadASTBind(ClientContext &context, TableFunctionBindInput &input,
                                          vector<LogicalType> &return_types, vector<string> &names) {
	// Check arguments
	if (input.inputs.size() != 2) {
		throw BinderException("read_ast requires exactly 2 arguments: file_path and language");
	}
	
	auto file_path = input.inputs[0].GetValue<string>();
	auto language = input.inputs[1].GetValue<string>();
	
	// Define output columns
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
		LogicalType::BIGINT,      // parent_id
		LogicalType::INTEGER,     // depth
		LogicalType::INTEGER,     // sibling_index
		LogicalType::VARCHAR      // source_text
	};
	
	auto result = make_uniq<ReadASTData>(file_path, language);
	return std::move(result);
}

static void ReadASTFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->CastNoConst<ReadASTData>();
	
	// Parse the file if not already done
	if (data.nodes.empty() && data.current_index == 0) {
		try {
			// Read file content
			auto &fs = FileSystem::GetFileSystem(context);
			auto handle = fs.OpenFile(data.file_path, FileFlags::FILE_FLAGS_READ);
			auto file_size = fs.GetFileSize(*handle);
			
			string content;
			content.resize(file_size);
			fs.Read(*handle, (void*)content.data(), file_size);
			
			// Parse using ASTParser
			ASTParser parser;
			TSParser *ts_parser = parser.CreateParser(data.language);
			if (!ts_parser) {
				throw IOException("Failed to create parser for language: " + data.language);
			}
			
			TSTree *tree = parser.ParseString(content, ts_parser);
			if (!tree) {
				ts_parser_delete(ts_parser);
				throw IOException("Failed to parse file");
			}
			
			// Convert tree to nodes
			TSNode root = ts_tree_root_node(tree);
			int64_t node_counter = 0;
			
			struct StackEntry {
				TSNode node;
				int64_t parent_id;
				int32_t depth;
				int32_t sibling_index;
			};
			
			vector<StackEntry> stack;
			stack.push_back({root, -1, 0, 0});
			
			while (!stack.empty()) {
				auto entry = stack.back();
				stack.pop_back();
				
				// Create node
				ASTNode ast_node;
				ast_node.node_id = node_counter++;
				ast_node.type = ts_node_type(entry.node);
				ast_node.parent_id = entry.parent_id;
				ast_node.depth = entry.depth;
				ast_node.sibling_index = entry.sibling_index;
				// file_path is stored at the data level, not per node
				
				// Extract position
				TSPoint start = ts_node_start_point(entry.node);
				TSPoint end = ts_node_end_point(entry.node);
				ast_node.start_line = start.row + 1;
				ast_node.start_column = start.column + 1;
				ast_node.end_line = end.row + 1;
				ast_node.end_column = end.column + 1;
				
				// Extract name
				ast_node.name = parser.ExtractNodeName(entry.node, content);
				
				// Extract source text
				uint32_t start_byte = ts_node_start_byte(entry.node);
				uint32_t end_byte = ts_node_end_byte(entry.node);
				if (start_byte < content.size() && end_byte <= content.size()) {
					ast_node.source_text = content.substr(start_byte, end_byte - start_byte);
				}
				
				data.nodes.push_back(ast_node);
				
				// Add children in reverse order for correct processing
				uint32_t child_count = ts_node_child_count(entry.node);
				for (int32_t i = child_count - 1; i >= 0; i--) {
					TSNode child = ts_node_child(entry.node, i);
					stack.push_back({child, ast_node.node_id, entry.depth + 1, i});
				}
			}
			
			ts_tree_delete(tree);
			ts_parser_delete(ts_parser);
		} catch (std::exception &e) {
			throw IOException("Failed to parse file: %s", e.what());
		}
	}
	
	// Fill output chunk
	idx_t count = 0;
	idx_t max_count = STANDARD_VECTOR_SIZE;
	
	// Get output vectors
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
	auto source_text_vec = FlatVector::GetData<string_t>(output.data[11]);
	
	// Get validity masks
	auto &name_validity = FlatVector::Validity(output.data[2]);
	auto &parent_validity = FlatVector::Validity(output.data[8]);
	
	while (data.current_index < data.nodes.size() && count < max_count) {
		const auto &node = data.nodes[data.current_index];
		
		// Fill in data
		node_id_vec[count] = node.node_id;
		type_vec[count] = StringVector::AddString(output.data[1], node.type);
		
		if (node.name.empty()) {
			name_validity.SetInvalid(count);
		} else {
			name_vec[count] = StringVector::AddString(output.data[2], node.name);
		}
		
		file_path_vec[count] = StringVector::AddString(output.data[3], data.file_path);
		start_line_vec[count] = node.start_line;
		start_column_vec[count] = node.start_column;
		end_line_vec[count] = node.end_line;
		end_column_vec[count] = node.end_column;
		
		if (node.parent_id < 0) {
			parent_validity.SetInvalid(count);
		} else {
			parent_id_vec[count] = node.parent_id;
		}
		
		depth_vec[count] = node.depth;
		sibling_index_vec[count] = node.sibling_index;
		source_text_vec[count] = StringVector::AddStringOrBlob(output.data[11], node.source_text);
		
		count++;
		data.current_index++;
	}
	
	output.SetCardinality(count);
}

static TableFunction GetReadASTFunction() {
	TableFunction read_ast("read_ast", {LogicalType::VARCHAR, LogicalType::VARCHAR}, 
	                      ReadASTFunction, ReadASTBind);
	read_ast.name = "read_ast";
	return read_ast;
}

void RegisterReadASTFunction(DatabaseInstance &instance) {
	ExtensionUtil::RegisterFunction(instance, GetReadASTFunction());
}

} // namespace duckdb