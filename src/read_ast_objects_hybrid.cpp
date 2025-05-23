#include "read_ast_objects_hybrid.hpp"
#include "ast_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>

namespace duckdb {

TableFunction ReadASTObjectsHybridFunction::GetFunction() {
    TableFunction function("read_ast_objects", {LogicalType::VARCHAR, LogicalType::VARCHAR}, Execute, Bind);
    function.name = "read_ast_objects";
    return function;
}

unique_ptr<FunctionData> ReadASTObjectsHybridFunction::Bind(ClientContext &context, TableFunctionBindInput &input,
                                                          vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("read_ast_objects requires exactly 2 arguments: file_pattern and language");
    }
    
    auto file_pattern = input.inputs[0].GetValue<string>();
    auto language = input.inputs[1].GetValue<string>();
    
    // Get list of files matching pattern
    auto &fs = FileSystem::GetFileSystem(context);
    vector<string> files;
    
    // Simple file pattern expansion - for now handle single files or basic wildcards
    if (file_pattern.find('*') != string::npos) {
        // For now, just throw an error - we'll implement proper globbing later
        throw NotImplementedException("File patterns not yet implemented. Please specify a single file.");
    } else {
        // Single file
        if (!fs.FileExists(file_pattern)) {
            throw IOException("File not found: " + file_pattern);
        }
        files.push_back(file_pattern);
    }
    
    // Define output columns - structured metadata + JSON nodes
    names = {"file_path", "language", "parse_time", "node_count", "max_depth", "nodes"};
    return_types = {
        LogicalType::VARCHAR,       // file_path
        LogicalType::VARCHAR,       // language  
        LogicalType::TIMESTAMP,     // parse_time
        LogicalType::INTEGER,       // node_count
        LogicalType::INTEGER,       // max_depth
        LogicalType::VARCHAR        // nodes (JSON as VARCHAR for now)
    };
    
    return make_uniq<ReadASTObjectsHybridData>(std::move(files), std::move(language));
}

string ReadASTObjectsHybridFunction::ParseFileToJSON(ClientContext &context, const string &file_path, const string &language) {
    auto &fs = FileSystem::GetFileSystem(context);
    
    // Read file content
    auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
    auto file_size = fs.GetFileSize(*handle);
    
    string content;
    content.resize(file_size);
    fs.Read(*handle, (void*)content.data(), file_size);
    
    // Parse using ASTParser
    ASTParser parser;
    TSParser *ts_parser = parser.CreateParser(language);
    if (!ts_parser) {
        throw IOException("Failed to create parser for language: " + language);
    }
    
    TSTree *tree = parser.ParseString(content, ts_parser);
    if (!tree) {
        ts_parser_delete(ts_parser);
        throw IOException("Failed to parse file");
    }
    
    // Convert tree to JSON
    std::ostringstream json;
    json << "[";
    
    TSNode root = ts_tree_root_node(tree);
    int64_t node_counter = 0;
    int32_t max_depth = 0;
    
    struct StackEntry {
        TSNode node;
        int64_t parent_id;
        int32_t depth;
        int32_t sibling_index;
    };
    
    vector<StackEntry> stack;
    stack.push_back({root, -1, 0, 0});
    
    bool first = true;
    while (!stack.empty()) {
        auto entry = stack.back();
        stack.pop_back();
        
        if (!first) json << ",";
        first = false;
        
        max_depth = std::max(max_depth, entry.depth);
        
        // Create JSON object for this node
        json << "{";
        json << "\"id\":" << node_counter << ",";
        json << "\"type\":\"" << ts_node_type(entry.node) << "\",";
        
        // Extract name
        string name = parser.ExtractNodeName(entry.node, content);
        if (!name.empty()) {
            json << "\"name\":\"" << StringUtil::Replace(name, "\"", "\\\"") << "\",";
        }
        
        // Position
        TSPoint start = ts_node_start_point(entry.node);
        TSPoint end = ts_node_end_point(entry.node);
        json << "\"start\":{\"line\":" << (start.row + 1) << ",\"column\":" << (start.column + 1) << "},";
        json << "\"end\":{\"line\":" << (end.row + 1) << ",\"column\":" << (end.column + 1) << "},";
        
        // Relationships
        if (entry.parent_id >= 0) {
            json << "\"parent_id\":" << entry.parent_id << ",";
        }
        json << "\"depth\":" << entry.depth << ",";
        json << "\"sibling_index\":" << entry.sibling_index;
        
        // Children array
        uint32_t child_count = ts_node_child_count(entry.node);
        if (child_count > 0) {
            json << ",\"children\":[";
            for (uint32_t i = 0; i < child_count; i++) {
                if (i > 0) json << ",";
                json << (node_counter + 1 + i);  // Child IDs will be sequential
            }
            json << "]";
        }
        
        json << "}";
        
        // Add children to stack in reverse order for correct processing
        int64_t current_id = node_counter++;
        for (int32_t i = child_count - 1; i >= 0; i--) {
            TSNode child = ts_node_child(entry.node, i);
            stack.push_back({child, current_id, entry.depth + 1, i});
        }
    }
    
    json << "]";
    
    ts_tree_delete(tree);
    ts_parser_delete(ts_parser);
    
    return json.str();
}

void ReadASTObjectsHybridFunction::Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &data = data_p.bind_data->CastNoConst<ReadASTObjectsHybridData>();
    
    idx_t count = 0;
    auto file_path_data = FlatVector::GetData<string_t>(output.data[0]);
    auto language_data = FlatVector::GetData<string_t>(output.data[1]);
    auto parse_time_data = FlatVector::GetData<timestamp_t>(output.data[2]);
    auto node_count_data = FlatVector::GetData<int32_t>(output.data[3]);
    auto max_depth_data = FlatVector::GetData<int32_t>(output.data[4]);
    auto nodes_data = FlatVector::GetData<string_t>(output.data[5]);
    
    while (data.current_file_idx < data.files.size() && count < STANDARD_VECTOR_SIZE) {
        const auto &file_path = data.files[data.current_file_idx];
        
        try {
            // Record parse time
            auto start_time = std::chrono::system_clock::now();
            
            // Parse file to JSON
            string json_nodes = ParseFileToJSON(context, file_path, data.language);
            
            auto end_time = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            // Count nodes and find max depth from JSON (simplified)
            int32_t node_count = 0;
            int32_t max_depth = 0;
            
            // Simple counting - count occurrences of "\"id\":"
            size_t pos = 0;
            while ((pos = json_nodes.find("\"id\":", pos)) != string::npos) {
                node_count++;
                pos += 5;
            }
            
            // Find max depth - look for highest "depth": value
            pos = 0;
            while ((pos = json_nodes.find("\"depth\":", pos)) != string::npos) {
                pos += 8;
                size_t end_pos = json_nodes.find_first_of(",}", pos);
                if (end_pos != string::npos) {
                    string depth_str = json_nodes.substr(pos, end_pos - pos);
                    int32_t depth = std::stoi(depth_str);
                    max_depth = std::max(max_depth, depth);
                }
            }
            
            // Set values
            file_path_data[count] = StringVector::AddString(output.data[0], file_path);
            language_data[count] = StringVector::AddString(output.data[1], data.language);
            parse_time_data[count] = Timestamp::FromEpochMs(std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count());
            node_count_data[count] = node_count;
            max_depth_data[count] = max_depth;
            nodes_data[count] = StringVector::AddStringOrBlob(output.data[5], json_nodes);
            
            count++;
        } catch (const Exception &e) {
            // Skip files that can't be parsed and continue
            // In production, might want to return error info
        }
        
        data.current_file_idx++;
    }
    
    output.SetCardinality(count);
}

void RegisterReadASTObjectsHybridFunction(DatabaseInstance &instance) {
    ExtensionUtil::RegisterFunction(instance, ReadASTObjectsHybridFunction::GetFunction());
}

} // namespace duckdb