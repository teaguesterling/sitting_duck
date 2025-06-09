#include "unified_ast_backend.hpp"
#include "language_adapter.hpp"
#include "semantic_types.hpp"
#include "ast_file_utils.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"
#include <stack>

namespace duckdb {

ASTResult UnifiedASTBackend::ParseToASTResult(const string& content, 
                                            const string& language, 
                                            const string& file_path) {
    
    ASTResult result;
    result.source.file_path = file_path;
    result.source.language = language;
    
    auto start_time = std::chrono::system_clock::now();
    
    // Get language adapter
    auto& registry = LanguageAdapterRegistry::GetInstance();
    const LanguageAdapter* adapter = registry.GetAdapter(language);
    if (!adapter) {
        throw InvalidInputException("Unsupported language: " + language);
    }
    
    // Parse the content using the adapter's smart pointer wrapper
    TSTreePtr tree = adapter->ParseContent(content);
    if (!tree) {
        throw InternalException("Failed to parse content");
    }
    
    // Process tree into ASTNodes using DFS ordering with O(1) descendant counting
    TSNode root = ts_tree_root_node(tree.get());
    int64_t node_counter = 0;
    uint32_t max_depth = 0;
    
    struct StackEntry {
        TSNode node;
        int64_t parent_id;
        uint32_t depth;
        uint32_t sibling_index;
        bool processed;        // Track if node has been processed
        idx_t node_index;      // Index in nodes array for this node
    };
    
    vector<StackEntry> stack;
    stack.push_back({root, -1, 0, 0, false, 0});
    
    while (!stack.empty()) {
        // Check if the top entry is processed before copying
        if (!stack.back().processed) {
            // Copy the entry to avoid reference invalidation when stack reallocates
            auto entry = stack.back();
            // First visit - create node and add children
            entry.processed = true;
            entry.node_index = result.nodes.size();
            
            // Update the stack entry with the processed flag and node_index
            stack.back().processed = true;
            stack.back().node_index = entry.node_index;
            
            // Track max depth
            max_depth = std::max(max_depth, entry.depth);
            
            // Create ASTNode
            ASTNode ast_node;
            
            // Basic information - use node_index as node_id
            ast_node.node_id = entry.node_index;
            ast_node.type.raw = ts_node_type(entry.node);
            
            // Position information
            TSPoint start = ts_node_start_point(entry.node);
            TSPoint end = ts_node_end_point(entry.node);
            ast_node.file_position.start_line = start.row + 1;
            ast_node.file_position.end_line = end.row + 1;
            ast_node.file_position.start_column = start.column + 1;
            ast_node.file_position.end_column = end.column + 1;
            
            // Tree position
            ast_node.tree_position.node_index = entry.node_index;
            ast_node.tree_position.parent_index = entry.parent_id;
            ast_node.tree_position.sibling_index = entry.sibling_index;
            ast_node.tree_position.node_depth = entry.depth;
            
            // Subtree information
            uint32_t child_count = ts_node_child_count(entry.node);
            ast_node.subtree.children_count = child_count;
            ast_node.subtree.descendant_count = 0; // Will be calculated on second visit
            
            // Extract name using language adapter
            ast_node.name.raw = adapter->ExtractNodeName(entry.node, content);
            ast_node.name.qualified = ast_node.name.raw; // TODO: Implement qualified name logic
            
            // Extract source text (peek)
            uint32_t start_byte = ts_node_start_byte(entry.node);
            uint32_t end_byte = ts_node_end_byte(entry.node);
            if (start_byte < content.size() && end_byte <= content.size() && end_byte > start_byte) {
                string source_text = content.substr(start_byte, end_byte - start_byte);
                ast_node.peek = source_text.length() > 120 ? source_text.substr(0, 120) : source_text;
            }
            
            // Populate semantic type and other fields
            PopulateSemanticFields(ast_node, adapter, entry.node);
            
            result.nodes.push_back(ast_node);
            
            // Add children to stack in reverse order for correct processing
            int64_t current_id = ast_node.tree_position.node_index;
            for (int32_t i = child_count - 1; i >= 0; i--) {
                TSNode child = ts_node_child(entry.node, i);
                stack.push_back({child, current_id, entry.depth + 1, static_cast<uint32_t>(i), false, 0});
            }
        } else {
            // Second visit - get the processed entry for descendant calculation
            auto entry = stack.back();
            
            // O(1) descendant count calculation!
            // All nodes between entry.node_index+1 and nodes.size() are descendants
            // due to DFS ordering
            int32_t descendant_count = result.nodes.size() - entry.node_index - 1;
            result.nodes[entry.node_index].subtree.descendant_count = descendant_count;
            
            stack.pop_back();
        }
    }
    
    // Smart pointer automatically cleans up the tree
    
    // Set metadata
    result.parse_time = start_time;
    result.node_count = result.nodes.size();
    result.max_depth = max_depth;
    
    return result;
}

void UnifiedASTBackend::PopulateSemanticFields(ASTNode& node, const LanguageAdapter* adapter, TSNode ts_node) {
    // Get node configuration from adapter
    const NodeConfig* config = adapter->GetNodeConfig(node.type.raw);
    
    if (config) {
        // Set semantic type from configuration
        node.semantic_type = config->semantic_type;
        // Set universal flags from configuration
        node.universal_flags = config->flags;
    } else {
        // Fallback: use PARSER_CONSTRUCT for unknown types
        node.semantic_type = SemanticTypes::PARSER_CONSTRUCT;
        node.universal_flags = 0;
    }
    
    // Set normalized type for display/compatibility
    node.type.normalized = SemanticTypes::GetSemanticTypeName(node.semantic_type);
    
    // Calculate arity binning
    node.arity_bin = ASTNode::BinArityFibonacci(ts_node_child_count(ts_node));
    
    // Update legacy fields from semantic_type
    node.UpdateLegacyFields();
}


vector<LogicalType> UnifiedASTBackend::GetFlatTableSchema() {
    return {
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
        LogicalType::INTEGER,     // children_count
        LogicalType::INTEGER,     // descendant_count
        LogicalType::VARCHAR,     // peek (source_text)
        // Semantic type fields
        LogicalType::TINYINT,     // semantic_type
        LogicalType::TINYINT,     // universal_flags
        LogicalType::TINYINT      // arity_bin
    };
}

vector<string> UnifiedASTBackend::GetFlatTableColumnNames() {
    return {
        "node_id", "type", "name", "file_path",
        "start_line", "start_column", "end_line", "end_column", 
        "parent_id", "depth", "sibling_index", "children_count", "descendant_count",
        "peek",
        // Semantic type fields  
        "semantic_type", "universal_flags", "arity_bin"
    };
}

LogicalType UnifiedASTBackend::GetASTStructSchema() {
    // Create the complete AST struct schema with taxonomy fields
    child_list_t<LogicalType> source_children;
    source_children.push_back(make_pair("file_path", LogicalType::VARCHAR));
    source_children.push_back(make_pair("language", LogicalType::VARCHAR));
    
    child_list_t<LogicalType> node_children;
    node_children.push_back(make_pair("node_id", LogicalType::BIGINT));
    node_children.push_back(make_pair("type", LogicalType::VARCHAR));
    node_children.push_back(make_pair("name", LogicalType::VARCHAR));
    node_children.push_back(make_pair("start_line", LogicalType::INTEGER));
    node_children.push_back(make_pair("end_line", LogicalType::INTEGER));
    node_children.push_back(make_pair("start_column", LogicalType::INTEGER));
    node_children.push_back(make_pair("end_column", LogicalType::INTEGER));
    node_children.push_back(make_pair("parent_id", LogicalType::BIGINT));
    node_children.push_back(make_pair("depth", LogicalType::INTEGER));
    node_children.push_back(make_pair("sibling_index", LogicalType::INTEGER));
    node_children.push_back(make_pair("children_count", LogicalType::INTEGER));
    node_children.push_back(make_pair("descendant_count", LogicalType::INTEGER));
    node_children.push_back(make_pair("peek", LogicalType::VARCHAR));
    // Semantic type fields
    node_children.push_back(make_pair("semantic_type", LogicalType::TINYINT));
    node_children.push_back(make_pair("universal_flags", LogicalType::TINYINT));
    node_children.push_back(make_pair("arity_bin", LogicalType::TINYINT));
    
    child_list_t<LogicalType> ast_children;
    ast_children.push_back(make_pair("nodes", LogicalType::LIST(LogicalType::STRUCT(node_children))));
    ast_children.push_back(make_pair("source", LogicalType::STRUCT(source_children)));
    
    return LogicalType::STRUCT(ast_children);
}

void UnifiedASTBackend::ProjectToTable(const ASTResult& result, DataChunk& output, idx_t& current_row, idx_t& output_index) {
    // Verify output chunk has correct number of columns
    if (output.ColumnCount() != 17) {
        throw InternalException("Output chunk has " + to_string(output.ColumnCount()) + " columns, expected 17");
    }
    
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
    auto children_count_vec = FlatVector::GetData<int32_t>(output.data[11]);
    auto descendant_count_vec = FlatVector::GetData<int32_t>(output.data[12]);
    auto peek_vec = FlatVector::GetData<string_t>(output.data[13]);
    // Semantic type fields
    auto semantic_type_vec = FlatVector::GetData<int8_t>(output.data[14]);
    auto universal_flags_vec = FlatVector::GetData<int8_t>(output.data[15]);
    auto arity_bin_vec = FlatVector::GetData<int8_t>(output.data[16]);
    
    // Get validity masks for nullable fields
    auto &name_validity = FlatVector::Validity(output.data[2]);
    auto &parent_validity = FlatVector::Validity(output.data[9]);
    
    idx_t count = 0;
    idx_t max_count = STANDARD_VECTOR_SIZE;
    
    // Start from current_row and process up to STANDARD_VECTOR_SIZE rows
    for (idx_t i = current_row; i < result.nodes.size() && count < max_count; i++) {
        const auto& node = result.nodes[i];
        
        // Basic fields
        node_id_vec[output_index + count] = node.node_id;
        type_vec[output_index + count] = StringVector::AddString(output.data[1], node.type.raw);
        
        if (node.name.raw.empty()) {
            name_validity.SetInvalid(output_index + count);
        } else {
            name_vec[output_index + count] = StringVector::AddString(output.data[2], node.name.raw);
        }
        
        // Now that we process each file separately, each result has the correct file_path
        file_path_vec[output_index + count] = StringVector::AddString(output.data[3], result.source.file_path);
        start_line_vec[output_index + count] = node.file_position.start_line;
        start_column_vec[output_index + count] = node.file_position.start_column;
        end_line_vec[output_index + count] = node.file_position.end_line;
        end_column_vec[output_index + count] = node.file_position.end_column;
        
        if (node.tree_position.parent_index < 0) {
            parent_validity.SetInvalid(output_index + count);
        } else {
            parent_id_vec[output_index + count] = node.tree_position.parent_index;
        }
        
        depth_vec[output_index + count] = node.tree_position.node_depth;
        sibling_index_vec[output_index + count] = node.tree_position.sibling_index;
        children_count_vec[output_index + count] = node.subtree.children_count;
        descendant_count_vec[output_index + count] = node.subtree.descendant_count;
        peek_vec[output_index + count] = StringVector::AddString(output.data[13], node.peek);
        
        // Semantic type fields
        semantic_type_vec[output_index + count] = node.semantic_type;
        universal_flags_vec[output_index + count] = node.universal_flags;
        arity_bin_vec[output_index + count] = node.arity_bin;
        
        count++;
        current_row++;  // Track which row we're on
    }
    
    output_index += count;
}

Value UnifiedASTBackend::CreateASTStruct(const ASTResult& result) {
    // Create source struct
    child_list_t<Value> source_children;
    source_children.push_back(make_pair("file_path", Value(result.source.file_path)));
    source_children.push_back(make_pair("language", Value(result.source.language)));
    Value source_value = Value::STRUCT(source_children);
    
    // Create nodes array
    vector<Value> node_values;
    node_values.reserve(result.nodes.size());
    
    for (const auto& node : result.nodes) {
        child_list_t<Value> node_children;
        node_children.push_back(make_pair("node_id", Value::BIGINT(node.node_id)));
        node_children.push_back(make_pair("type", Value(node.type.raw)));
        node_children.push_back(make_pair("name", Value(node.name.raw)));
        node_children.push_back(make_pair("start_line", Value::INTEGER(node.file_position.start_line)));
        node_children.push_back(make_pair("end_line", Value::INTEGER(node.file_position.end_line)));
        node_children.push_back(make_pair("start_column", Value::INTEGER(node.file_position.start_column)));
        node_children.push_back(make_pair("end_column", Value::INTEGER(node.file_position.end_column)));
        node_children.push_back(make_pair("parent_id", node.tree_position.parent_index >= 0 ? 
                                         Value::BIGINT(node.tree_position.parent_index) : Value()));
        node_children.push_back(make_pair("depth", Value::INTEGER(node.tree_position.node_depth)));
        node_children.push_back(make_pair("sibling_index", Value::INTEGER(node.tree_position.sibling_index)));
        node_children.push_back(make_pair("children_count", Value::INTEGER(node.subtree.children_count)));
        node_children.push_back(make_pair("descendant_count", Value::INTEGER(node.subtree.descendant_count)));
        node_children.push_back(make_pair("peek", Value(node.peek)));
        // Semantic type fields
        node_children.push_back(make_pair("semantic_type", Value::TINYINT(node.semantic_type)));
        node_children.push_back(make_pair("universal_flags", Value::TINYINT(node.universal_flags)));
        node_children.push_back(make_pair("arity_bin", Value::TINYINT(node.arity_bin)));
        
        node_values.push_back(Value::STRUCT(node_children));
    }
    
    // Create AST struct with proper node schema
    child_list_t<LogicalType> node_schema;
    node_schema.push_back(make_pair("node_id", LogicalType::BIGINT));
    node_schema.push_back(make_pair("type", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("name", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("start_line", LogicalType::INTEGER));
    node_schema.push_back(make_pair("end_line", LogicalType::INTEGER));
    node_schema.push_back(make_pair("start_column", LogicalType::INTEGER));
    node_schema.push_back(make_pair("end_column", LogicalType::INTEGER));
    node_schema.push_back(make_pair("parent_id", LogicalType::BIGINT));
    node_schema.push_back(make_pair("depth", LogicalType::INTEGER));
    node_schema.push_back(make_pair("sibling_index", LogicalType::INTEGER));
    node_schema.push_back(make_pair("children_count", LogicalType::INTEGER));
    node_schema.push_back(make_pair("descendant_count", LogicalType::INTEGER));
    node_schema.push_back(make_pair("peek", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("semantic_type", LogicalType::TINYINT));
    node_schema.push_back(make_pair("universal_flags", LogicalType::TINYINT));
    node_schema.push_back(make_pair("arity_bin", LogicalType::TINYINT));
    
    child_list_t<Value> ast_children;
    ast_children.push_back(make_pair("nodes", Value::LIST(LogicalType::STRUCT(node_schema), node_values)));
    ast_children.push_back(make_pair("source", source_value));
    
    return Value::STRUCT(ast_children);
}

Value UnifiedASTBackend::CreateASTStructValue(const ASTResult& result) {
    // Same as CreateASTStruct for now - both return a single struct value
    return CreateASTStruct(result);
}

ASTResultCollection UnifiedASTBackend::ParseFilesToASTCollection(ClientContext &context,
                                                               const Value &file_path_value,
                                                               const string& language,
                                                               bool ignore_errors) {
    // Get all files that match the input pattern(s)
    vector<string> supported_extensions;
    if (language != "auto") {
        supported_extensions = ASTFileUtils::GetSupportedExtensions(language);
    }
    
    auto file_paths = ASTFileUtils::GetFiles(context, file_path_value, ignore_errors, supported_extensions);
    
    if (file_paths.empty() && !ignore_errors) {
        throw IOException("No files found matching the input pattern");
    }
    
    // Parse all files as separate results
    ASTResultCollection collection;
    
    for (const auto& file_path : file_paths) {
        try {
            // Auto-detect language if needed
            string file_language = language;
            if (language == "auto") {
                file_language = ASTFileUtils::DetectLanguageFromPath(file_path);
                if (file_language == "auto") {
                    if (!ignore_errors) {
                        throw BinderException("Could not detect language for file: " + file_path);
                    }
                    continue; // Skip this file
                }
            }
            
            // Read file content
            auto &fs = FileSystem::GetFileSystem(context);
            if (!fs.FileExists(file_path)) {
                if (!ignore_errors) {
                    throw IOException("File does not exist: " + file_path);
                }
                continue; // Skip missing files
            }
            
            auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
            auto file_size = fs.GetFileSize(*handle);
            
            string content;
            content.resize(file_size);
            fs.Read(*handle, (void*)content.data(), file_size);
            
            // Parse this file as a separate result
            auto file_result = ParseToASTResult(content, file_language, file_path);
            
            // Add this individual result to the collection
            collection.results.push_back(std::move(file_result));
            
        } catch (const Exception &e) {
            if (!ignore_errors) {
                throw IOException("Failed to parse file '" + file_path + "': " + string(e.what()));
            }
            // With ignore_errors=true, continue processing other files
        }
    }
    
    return collection;
}

} // namespace duckdb