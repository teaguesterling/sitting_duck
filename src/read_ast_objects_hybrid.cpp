#include "read_ast_objects_hybrid.hpp"
#include "ast_parser.hpp"
#include "ast_type.hpp"
#include "language_handler.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>
#include <iomanip>
#include <cctype>

namespace duckdb {


// Helper function to detect language from file extension
static string DetectLanguageFromExtension(const string &file_path) {
    auto dot_pos = file_path.find_last_of('.');
    if (dot_pos == string::npos) {
        return "auto";  // No extension, can't detect
    }
    
    string ext = StringUtil::Lower(file_path.substr(dot_pos + 1));
    
    // Try to find a handler that has this extension as an alias
    auto& registry = LanguageHandlerRegistry::GetInstance();
    
    // Check if the extension itself is registered as an alias
    const LanguageHandler* handler = registry.GetHandler(ext);
    if (handler) {
        return handler->GetLanguageName();
    }
    
    // Special cases for extensions that aren't direct aliases
    // TypeScript uses JavaScript parser
    if (ext == "ts" || ext == "tsx") {
        return "javascript";
    }
    
    // Common JavaScript module extensions
    if (ext == "mjs" || ext == "cjs") {
        return "javascript";
    }
    
    return "auto";  // Unknown extension
}


TableFunction ReadASTObjectsHybridFunction::GetFunctionOneArg() {
    TableFunction function("read_ast_objects", {LogicalType::VARCHAR}, Execute, BindOneArg);
    function.name = "read_ast_objects";
    
    // Add named parameters for filtering (same as the two-arg version)
    function.named_parameters["exclude_types"] = LogicalType::LIST(LogicalType::VARCHAR);
    function.named_parameters["include_types"] = LogicalType::LIST(LogicalType::VARCHAR);
    
    return function;
}

TableFunction ReadASTObjectsHybridFunction::GetFunctionWithFilters() {
    TableFunction function("read_ast_objects", {LogicalType::VARCHAR, LogicalType::VARCHAR}, Execute, BindWithFilters);
    function.name = "read_ast_objects";
    
    // Set named parameters for filtering (all optional)
    function.named_parameters["exclude_types"] = LogicalType::LIST(LogicalType::VARCHAR);
    function.named_parameters["include_types"] = LogicalType::LIST(LogicalType::VARCHAR);
    
    return function;
}


unique_ptr<FunctionData> ReadASTObjectsHybridFunction::BindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                                vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 1) {
        throw BinderException("read_ast_objects with one argument requires exactly 1 argument: file_pattern");
    }
    
    auto file_pattern = input.inputs[0].GetValue<string>();
    
    // Detect language from file extension
    string language = DetectLanguageFromExtension(file_pattern);
    if (language == "auto") {
        throw BinderException("Could not detect language from file extension. Please specify language explicitly.");
    }
    
    // Parse named filter parameters (for future use - currently ignored)
    // This preserves the API for when we implement proper filtering
    
    FilterConfig filter_config; // Empty config - no filtering applied
    
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
    
    // Define output columns - single "ast" column containing AST struct
    names = {"ast"};
    
    // Define struct type for AST nodes
    child_list_t<LogicalType> node_struct_children;
    node_struct_children.push_back(make_pair("node_id", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("type", LogicalType::VARCHAR));
    node_struct_children.push_back(make_pair("name", LogicalType::VARCHAR));
    node_struct_children.push_back(make_pair("start_line", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("end_line", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("start_column", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("end_column", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("parent_id", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("depth", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("sibling_index", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("children_count", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("descendant_count", LogicalType::INTEGER));
    
    auto node_struct_type = LogicalType::STRUCT(node_struct_children);
    auto nodes_array_type = LogicalType::LIST(node_struct_type);
    
    return_types = {
        LogicalType::VARCHAR,       // file_path
        LogicalType::VARCHAR,       // language  
        LogicalType::TIMESTAMP,     // parse_time
        LogicalType::INTEGER,       // node_count
        LogicalType::INTEGER,       // max_depth
        nodes_array_type            // nodes (struct array)
    };
    
    return make_uniq<ReadASTObjectsHybridData>(std::move(files), std::move(language), std::move(filter_config));
}

unique_ptr<FunctionData> ReadASTObjectsHybridFunction::BindWithFilters(ClientContext &context, TableFunctionBindInput &input,
                                                                      vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("read_ast_objects with filters requires 2 positional arguments: file_pattern, language");
    }
    
    auto file_pattern = input.inputs[0].GetValue<string>();
    auto language = input.inputs[1].GetValue<string>();
    
    // Parse named filter parameters (for future use - currently ignored)
    // This preserves the API for when we implement proper filtering
    
    FilterConfig filter_config; // Empty config - no filtering applied
    
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
    
    // Define output columns - single "ast" column containing AST struct
    names = {"ast"};
    
    // Define struct type for AST nodes
    child_list_t<LogicalType> node_struct_children;
    node_struct_children.push_back(make_pair("node_id", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("type", LogicalType::VARCHAR));
    node_struct_children.push_back(make_pair("name", LogicalType::VARCHAR));
    node_struct_children.push_back(make_pair("start_line", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("end_line", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("start_column", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("end_column", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("parent_id", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("depth", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("sibling_index", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("children_count", LogicalType::INTEGER));
    node_struct_children.push_back(make_pair("descendant_count", LogicalType::INTEGER));
    
    auto node_struct_type = LogicalType::STRUCT(node_struct_children);
    auto nodes_array_type = LogicalType::LIST(node_struct_type);
    
    return_types = {
        LogicalType::VARCHAR,       // file_path
        LogicalType::VARCHAR,       // language  
        LogicalType::TIMESTAMP,     // parse_time
        LogicalType::INTEGER,       // node_count
        LogicalType::INTEGER,       // max_depth
        nodes_array_type            // nodes (struct array)
    };
    
    return make_uniq<ReadASTObjectsHybridData>(std::move(files), std::move(language), std::move(filter_config));
}

Value ReadASTObjectsHybridFunction::ParseFileToStructs(ClientContext &context, const string &file_path, const string &language, LogicalType &nodes_type, const FilterConfig &filter_config) {
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
    
    // Collect all nodes into a vector first
    struct NodeInfo {
        int64_t node_id;
        string type;
        string name;
        int32_t start_line;
        int32_t end_line;
        int32_t start_column;
        int32_t end_column;
        int64_t parent_id;
        int32_t depth;
        int32_t sibling_index;
        int32_t children_count;
        int32_t descendant_count;
    };
    
    vector<NodeInfo> nodes;
    
    TSNode root = ts_tree_root_node(tree);
    int64_t node_counter = 0;
    
    struct StackEntry {
        TSNode node;
        int64_t parent_id;
        int32_t depth;
        int32_t sibling_index;
        bool processed; // Track if node has been processed for descendant counting
        idx_t node_index; // Index in nodes array for this node
    };
    
    vector<StackEntry> stack;
    stack.push_back({root, -1, 0, 0, false, 0});
    
    while (!stack.empty()) {
        auto entry = stack.back();
        
        if (!entry.processed) {
            // First time processing this node - create NodeInfo and add children
            stack.back().processed = true;
            stack.back().node_index = nodes.size();
            
            // Extract name using language handler
            const LanguageHandler* handler = parser.GetLanguageHandler(language);
            string name = handler ? handler->ExtractNodeName(entry.node, content) : "";
            
            // Position
            TSPoint start = ts_node_start_point(entry.node);
            TSPoint end = ts_node_end_point(entry.node);
            
            // Create node info
            NodeInfo node_info;
            node_info.node_id = node_counter++;
            node_info.type = ts_node_type(entry.node);
            node_info.name = name;
            node_info.start_line = start.row + 1;
            node_info.end_line = end.row + 1;
            node_info.start_column = start.column + 1;
            node_info.end_column = end.column + 1;
            node_info.parent_id = entry.parent_id;
            node_info.depth = entry.depth;
            node_info.sibling_index = entry.sibling_index;
            
            // Set children_count directly
            uint32_t child_count = ts_node_child_count(entry.node);
            node_info.children_count = child_count;
            node_info.descendant_count = 0; // Will be calculated later
            
            nodes.push_back(node_info);
            
            // Add children to stack in reverse order for correct processing
            int64_t current_id = node_info.node_id;
            for (int32_t i = child_count - 1; i >= 0; i--) {
                TSNode child = ts_node_child(entry.node, i);
                stack.push_back({child, current_id, entry.depth + 1, i, false, 0});
            }
        } else {
            // Second time - all children have been processed, calculate descendant count
            stack.pop_back();
            
            // Calculate descendant count by summing children + their descendants
            int32_t descendant_count = 0;
            int64_t current_node_id = nodes[entry.node_index].node_id;
            
            for (const auto &node : nodes) {
                if (node.parent_id == current_node_id) {
                    descendant_count += 1 + node.descendant_count;
                }
            }
            
            nodes[entry.node_index].descendant_count = descendant_count;
        }
    }
    
    ts_tree_delete(tree);
    ts_parser_delete(ts_parser);
    
    // Create list of struct values
    vector<Value> struct_values;
    struct_values.reserve(nodes.size());
    
    // Get struct type from list type
    auto struct_type = ListType::GetChildType(nodes_type);
    
    for (const auto &node : nodes) {
        // Create struct value for each node
        child_list_t<Value> struct_children;
        struct_children.push_back(make_pair("node_id", Value::INTEGER(node.node_id)));
        struct_children.push_back(make_pair("type", Value(node.type)));
        struct_children.push_back(make_pair("name", Value(node.name)));
        struct_children.push_back(make_pair("start_line", Value::INTEGER(node.start_line)));
        struct_children.push_back(make_pair("end_line", Value::INTEGER(node.end_line)));
        struct_children.push_back(make_pair("start_column", Value::INTEGER(node.start_column)));
        struct_children.push_back(make_pair("end_column", Value::INTEGER(node.end_column)));
        struct_children.push_back(make_pair("parent_id", Value::INTEGER(node.parent_id)));
        struct_children.push_back(make_pair("depth", Value::INTEGER(node.depth)));
        struct_children.push_back(make_pair("sibling_index", Value::INTEGER(node.sibling_index)));
        struct_children.push_back(make_pair("children_count", Value::INTEGER(node.children_count)));
        struct_children.push_back(make_pair("descendant_count", Value::INTEGER(node.descendant_count)));
        
        struct_values.push_back(Value::STRUCT(struct_children));
    }
    
    // Create list value
    return Value::LIST(struct_type, struct_values);
}

void ReadASTObjectsHybridFunction::Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &data = data_p.bind_data->CastNoConst<ReadASTObjectsHybridData>();
    
    idx_t count = 0;
    auto file_path_data = FlatVector::GetData<string_t>(output.data[0]);
    auto language_data = FlatVector::GetData<string_t>(output.data[1]);
    auto parse_time_data = FlatVector::GetData<timestamp_t>(output.data[2]);
    auto node_count_data = FlatVector::GetData<int32_t>(output.data[3]);
    auto max_depth_data = FlatVector::GetData<int32_t>(output.data[4]);
    
    while (data.current_file_idx < data.files.size() && count < STANDARD_VECTOR_SIZE) {
        const auto &file_path = data.files[data.current_file_idx];
        
        try {
            // Record parse time
            auto start_time = std::chrono::system_clock::now();
            
            // Parse file to structs
            auto nodes_type = output.data[5].GetType();
            Value struct_nodes_value = ParseFileToStructs(context, file_path, data.language, nodes_type, data.filter_config);
            
            auto end_time = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            // Get metadata from struct value
            auto &list_children = ListValue::GetChildren(struct_nodes_value);
            int32_t node_count = list_children.size();
            
            // Find max depth from struct data
            int32_t max_depth = 0;
            for (const auto &struct_val : list_children) {
                auto &struct_children = StructValue::GetChildren(struct_val);
                int32_t depth = struct_children[9].GetValue<int32_t>(); // depth field is at index 9 (0-based)
                max_depth = std::max(max_depth, depth);
            }
            
            // Set values
            file_path_data[count] = StringVector::AddString(output.data[0], file_path);
            language_data[count] = StringVector::AddString(output.data[1], data.language);
            parse_time_data[count] = Timestamp::FromEpochMs(std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count());
            node_count_data[count] = node_count;
            max_depth_data[count] = max_depth;
            
            // Set the list value directly
            output.data[5].SetValue(count, struct_nodes_value);
            
            count++;
        } catch (const Exception &e) {
            // Propagate parsing errors instead of silently skipping
            throw IOException("Failed to parse file '" + file_path + "': " + e.what());
        }
        
        data.current_file_idx++;
    }
    
    output.SetCardinality(count);
}

void RegisterReadASTObjectsHybridFunction(DatabaseInstance &instance) {
    // Create a function set with just the two functions that support named parameters
    TableFunctionSet read_ast_objects_set("read_ast_objects");
    read_ast_objects_set.AddFunction(ReadASTObjectsHybridFunction::GetFunctionOneArg());      // 1 arg + named params
    read_ast_objects_set.AddFunction(ReadASTObjectsHybridFunction::GetFunctionWithFilters()); // 2 args + named params
    
    ExtensionUtil::RegisterFunction(instance, read_ast_objects_set);
}



} // namespace duckdb