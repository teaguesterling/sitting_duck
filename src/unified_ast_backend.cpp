#include "unified_ast_backend.hpp"
#include "unified_ast_backend_impl.hpp"
#include "language_adapter.hpp"
#include "semantic_types.hpp"
#include "ast_file_utils.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "utf8proc_wrapper.hpp"
#include <stack>

namespace duckdb {

// SanitizeUTF8 is defined in unified_ast_backend_impl.hpp

ASTResult UnifiedASTBackend::ParseToASTResult(const string& content, 
                                            const string& language, 
                                            const string& file_path,
                                            int32_t peek_size,
                                            const string& peek_mode) {
    
    // Get language adapter
    auto& registry = LanguageAdapterRegistry::GetInstance();
    const LanguageAdapter* adapter = registry.GetAdapter(language);
    if (!adapter) {
        throw InvalidInputException("Unsupported language: " + language);
    }
    
    // Get the optimized parsing function (single virtual call)
    ParsingFunction parsing_fn = adapter->GetParsingFunction();
    
    // Call the parsing function with the adapter as context
    return parsing_fn(adapter, content, language, file_path, peek_size, peek_mode);
}

void UnifiedASTBackend::PopulateSemanticFields(ASTNode& node, const LanguageAdapter* adapter, TSNode ts_node, const string& content) {
    // Get node configuration (virtual call)
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
        LogicalType::VARCHAR,     // language
        LogicalType::UINTEGER,    // start_line
        LogicalType::UINTEGER,    // start_column
        LogicalType::UINTEGER,    // end_line
        LogicalType::UINTEGER,    // end_column
        LogicalType::BIGINT,      // parent_id (stays signed for -1)
        LogicalType::UINTEGER,    // depth
        LogicalType::UINTEGER,    // sibling_index
        LogicalType::UINTEGER,    // children_count
        LogicalType::UINTEGER,    // descendant_count
        LogicalType::VARCHAR,     // peek (source_text)
        // Semantic type fields
        LogicalType::UTINYINT,    // semantic_type
        LogicalType::UTINYINT,    // universal_flags
        LogicalType::UTINYINT     // arity_bin
    };
}

vector<string> UnifiedASTBackend::GetFlatTableColumnNames() {
    return {
        "node_id", "type", "name", "file_path", "language",
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
    node_children.push_back(make_pair("start_line", LogicalType::UINTEGER));
    node_children.push_back(make_pair("end_line", LogicalType::UINTEGER));
    node_children.push_back(make_pair("start_column", LogicalType::UINTEGER));
    node_children.push_back(make_pair("end_column", LogicalType::UINTEGER));
    node_children.push_back(make_pair("parent_id", LogicalType::BIGINT));
    node_children.push_back(make_pair("depth", LogicalType::UINTEGER));
    node_children.push_back(make_pair("sibling_index", LogicalType::UINTEGER));
    node_children.push_back(make_pair("children_count", LogicalType::UINTEGER));
    node_children.push_back(make_pair("descendant_count", LogicalType::UINTEGER));
    node_children.push_back(make_pair("peek", LogicalType::VARCHAR));
    // Semantic type fields
    node_children.push_back(make_pair("semantic_type", LogicalType::UTINYINT));
    node_children.push_back(make_pair("universal_flags", LogicalType::UTINYINT));
    node_children.push_back(make_pair("arity_bin", LogicalType::UTINYINT));
    
    child_list_t<LogicalType> ast_children;
    ast_children.push_back(make_pair("nodes", LogicalType::LIST(LogicalType::STRUCT(node_children))));
    ast_children.push_back(make_pair("source", LogicalType::STRUCT(source_children)));
    
    return LogicalType::STRUCT(ast_children);
}

void UnifiedASTBackend::ProjectToTable(const ASTResult& result, DataChunk& output, idx_t& current_row, idx_t& output_index) {
    // Verify output chunk has correct number of columns (18 with language)
    if (output.ColumnCount() != 18) {
        throw InternalException("Output chunk has " + to_string(output.ColumnCount()) + " columns, expected 18");
    }
    
    // Get output vectors
    auto node_id_vec = FlatVector::GetData<int64_t>(output.data[0]);
    auto type_vec = FlatVector::GetData<string_t>(output.data[1]);
    auto name_vec = FlatVector::GetData<string_t>(output.data[2]);
    auto file_path_vec = FlatVector::GetData<string_t>(output.data[3]);
    auto language_vec = FlatVector::GetData<string_t>(output.data[4]);
    auto start_line_vec = FlatVector::GetData<uint32_t>(output.data[5]);
    auto start_column_vec = FlatVector::GetData<uint32_t>(output.data[6]);
    auto end_line_vec = FlatVector::GetData<uint32_t>(output.data[7]);
    auto end_column_vec = FlatVector::GetData<uint32_t>(output.data[8]);
    auto parent_id_vec = FlatVector::GetData<int64_t>(output.data[9]);
    auto depth_vec = FlatVector::GetData<uint32_t>(output.data[10]);
    auto sibling_index_vec = FlatVector::GetData<uint32_t>(output.data[11]);
    auto children_count_vec = FlatVector::GetData<uint32_t>(output.data[12]);
    auto descendant_count_vec = FlatVector::GetData<uint32_t>(output.data[13]);
    auto peek_vec = FlatVector::GetData<string_t>(output.data[14]);
    // Semantic type fields
    auto semantic_type_vec = FlatVector::GetData<uint8_t>(output.data[15]);
    auto universal_flags_vec = FlatVector::GetData<uint8_t>(output.data[16]);
    auto arity_bin_vec = FlatVector::GetData<uint8_t>(output.data[17]);
    
    // Get validity masks for nullable fields
    auto &name_validity = FlatVector::Validity(output.data[2]);
    auto &parent_validity = FlatVector::Validity(output.data[9]);
    auto &peek_validity = FlatVector::Validity(output.data[14]);
    
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
        language_vec[output_index + count] = StringVector::AddString(output.data[4], result.source.language);
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
        if (node.peek.empty()) {
            peek_validity.SetInvalid(output_index + count);
        } else {
            peek_vec[output_index + count] = StringVector::AddString(output.data[14], node.peek);
        }
        
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
        node_children.push_back(make_pair("start_line", Value::UINTEGER(node.file_position.start_line)));
        node_children.push_back(make_pair("end_line", Value::UINTEGER(node.file_position.end_line)));
        node_children.push_back(make_pair("start_column", Value::UINTEGER(node.file_position.start_column)));
        node_children.push_back(make_pair("end_column", Value::UINTEGER(node.file_position.end_column)));
        node_children.push_back(make_pair("parent_id", node.tree_position.parent_index >= 0 ? 
                                         Value::BIGINT(node.tree_position.parent_index) : Value()));
        node_children.push_back(make_pair("depth", Value::UINTEGER(node.tree_position.node_depth)));
        node_children.push_back(make_pair("sibling_index", Value::UINTEGER(node.tree_position.sibling_index)));
        node_children.push_back(make_pair("children_count", Value::UINTEGER(node.subtree.children_count)));
        node_children.push_back(make_pair("descendant_count", Value::UINTEGER(node.subtree.descendant_count)));
        node_children.push_back(make_pair("peek", Value(node.peek)));
        // Semantic type fields
        node_children.push_back(make_pair("semantic_type", Value::UTINYINT(node.semantic_type)));
        node_children.push_back(make_pair("universal_flags", Value::UTINYINT(node.universal_flags)));
        node_children.push_back(make_pair("arity_bin", Value::UTINYINT(node.arity_bin)));
        
        node_values.push_back(Value::STRUCT(node_children));
    }
    
    // Create AST struct with proper node schema
    child_list_t<LogicalType> node_schema;
    node_schema.push_back(make_pair("node_id", LogicalType::BIGINT));
    node_schema.push_back(make_pair("type", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("name", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("start_line", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("end_line", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("start_column", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("end_column", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("parent_id", LogicalType::BIGINT));
    node_schema.push_back(make_pair("depth", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("sibling_index", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("children_count", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("descendant_count", LogicalType::UINTEGER));
    node_schema.push_back(make_pair("peek", LogicalType::VARCHAR));
    node_schema.push_back(make_pair("semantic_type", LogicalType::UTINYINT));
    node_schema.push_back(make_pair("universal_flags", LogicalType::UTINYINT));
    node_schema.push_back(make_pair("arity_bin", LogicalType::UTINYINT));
    
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
                                                               bool ignore_errors,
                                                               int32_t peek_size,
                                                               const string& peek_mode) {
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
            if (language == "auto" || language.empty()) {
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
            auto file_result = ParseToASTResult(content, file_language, file_path, peek_size, peek_mode);
            
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

unique_ptr<ASTResult> UnifiedASTBackend::ParseSingleFileToASTResult(ClientContext &context,
                                                                   const string& file_path,
                                                                   const string& language,
                                                                   bool ignore_errors,
                                                                   int32_t peek_size,
                                                                   const string& peek_mode) {
    try {
        // Auto-detect language if needed
        string file_language = language;
        if (language == "auto") {
            file_language = ASTFileUtils::DetectLanguageFromPath(file_path);
            if (file_language == "auto") {
                if (!ignore_errors) {
                    throw BinderException("Could not detect language for file: " + file_path);
                }
                return nullptr; // Skip this file
            }
        }
        
        // Read file content
        auto &fs = FileSystem::GetFileSystem(context);
        if (!fs.FileExists(file_path)) {
            if (!ignore_errors) {
                throw IOException("File does not exist: " + file_path);
            }
            return nullptr; // Skip missing files
        }
        
        auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
        auto file_size = fs.GetFileSize(*handle);
        
        string content;
        content.resize(file_size);
        fs.Read(*handle, (void*)content.data(), file_size);
        
        // Parse this file
        auto result = make_uniq<ASTResult>(ParseToASTResult(content, file_language, file_path, peek_size, peek_mode));
        return result;
        
    } catch (const Exception &e) {
        if (!ignore_errors) {
            throw IOException("Failed to parse file '" + file_path + "': " + string(e.what()));
        }
        return nullptr; // With ignore_errors=true, skip this file
    }
}

} // namespace duckdb
