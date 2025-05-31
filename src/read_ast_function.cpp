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
	names = {"node_id", "type", "normalized_type", "name", "file_path", "start_line", "start_column", 
	         "end_line", "end_column", "parent_id", "depth", "sibling_index", "source_text"};
	
	return_types = {
		LogicalType::BIGINT,      // node_id
		LogicalType::VARCHAR,     // type
		LogicalType::VARCHAR,     // normalized_type
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
				ast_node.tree_position.node_index = node_counter++;
				ast_node.node_id = node_counter - 1; // For now, use simple counter as ID
				ast_node.type.raw = ts_node_type(entry.node);
				ast_node.tree_position.parent_index = entry.parent_id;
				ast_node.tree_position.node_depth = entry.depth;
				ast_node.tree_position.sibling_index = entry.sibling_index;
				// file_path is stored at the data level, not per node
				
				// Extract position
				TSPoint start = ts_node_start_point(entry.node);
				TSPoint end = ts_node_end_point(entry.node);
				ast_node.file_position.start_line = start.row + 1;
				ast_node.file_position.start_column = start.column + 1;
				ast_node.file_position.end_line = end.row + 1;
				ast_node.file_position.end_column = end.column + 1;
				
				// Get language handler for name extraction and normalization
				const LanguageHandler* handler = parser.GetLanguageHandler(data.language);
				
				// Extract name
				ast_node.name.raw = handler ? handler->ExtractNodeName(entry.node, content) : "";
				
				// Extract source text
				uint32_t start_byte = ts_node_start_byte(entry.node);
				uint32_t end_byte = ts_node_end_byte(entry.node);
				if (start_byte < content.size() && end_byte <= content.size()) {
					string source_text = content.substr(start_byte, end_byte - start_byte);
					ast_node.peek = source_text.length() > 120 ? source_text.substr(0, 120) : source_text;
				}
				
				// Generate taxonomy information
				if (handler) {
					// Get taxonomy configuration from language handler
					const NodeTypeConfig* config = handler->GetNodeTypeConfig(ast_node.type.raw);
					if (config) {
						ast_node.kind = static_cast<uint8_t>(config->kind);
						ast_node.universal_flags = config->universal_flags;
						ast_node.super_type = config->super_type;
					} else {
						// Fallback
						ast_node.kind = static_cast<uint8_t>(ASTKind::PARSER_SPECIFIC);
						ast_node.universal_flags = 0;
						ast_node.super_type = 0;
					}
					
					ast_node.arity_bin = ASTNode::BinArityFibonacci(ts_node_child_count(entry.node));
					ast_node.type.normalized = handler->GetNormalizedType(ast_node.type.raw);
					ast_node.type.kind = ASTNode::GetKindName(static_cast<ASTKind>(ast_node.kind));
				}
				
				data.nodes.push_back(ast_node);
				
				// Add children in reverse order for correct processing
				uint32_t child_count = ts_node_child_count(entry.node);
				for (int32_t i = child_count - 1; i >= 0; i--) {
					TSNode child = ts_node_child(entry.node, i);
					stack.push_back({child, ast_node.tree_position.node_index, entry.depth + 1, i});
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
	auto normalized_type_vec = FlatVector::GetData<string_t>(output.data[2]);
	auto name_vec = FlatVector::GetData<string_t>(output.data[3]);
	auto file_path_vec = FlatVector::GetData<string_t>(output.data[4]);
	auto start_line_vec = FlatVector::GetData<int32_t>(output.data[5]);
	auto start_column_vec = FlatVector::GetData<int32_t>(output.data[6]);
	auto end_line_vec = FlatVector::GetData<int32_t>(output.data[7]);
	auto end_column_vec = FlatVector::GetData<int32_t>(output.data[8]);
	auto parent_id_vec = FlatVector::GetData<int64_t>(output.data[9]);
	auto depth_vec = FlatVector::GetData<int32_t>(output.data[10]);
	auto sibling_index_vec = FlatVector::GetData<int32_t>(output.data[11]);
	auto source_text_vec = FlatVector::GetData<string_t>(output.data[12]);
	
	// Get validity masks
	auto &name_validity = FlatVector::Validity(output.data[3]);
	auto &parent_validity = FlatVector::Validity(output.data[9]);
	
	while (data.current_index < data.nodes.size() && count < max_count) {
		const auto &node = data.nodes[data.current_index];
		
		// Fill in data
		node_id_vec[count] = node.tree_position.node_index;
		type_vec[count] = StringVector::AddString(output.data[1], node.type.raw);
		
		// Add normalized type using language handler
		ASTParser parser;
		const LanguageHandler* handler = parser.GetLanguageHandler(data.language);
		string normalized = handler ? handler->GetNormalizedType(node.type.raw) : node.type.raw;
		normalized_type_vec[count] = StringVector::AddString(output.data[2], normalized);
		
		if (node.name.raw.empty()) {
			name_validity.SetInvalid(count);
		} else {
			name_vec[count] = StringVector::AddString(output.data[3], node.name.raw);
		}
		
		file_path_vec[count] = StringVector::AddString(output.data[4], data.file_path);
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
		source_text_vec[count] = StringVector::AddStringOrBlob(output.data[12], node.peek);
		
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