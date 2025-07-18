#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/multi_file/multi_file_reader.hpp"
#include "duckdb/parallel/task_executor.hpp"
#include "duckdb/parallel/task_scheduler.hpp"
#include "unified_ast_backend.hpp"
#include "ast_file_utils.hpp"
#include "ast_parsing_task.hpp"
#include "read_ast_streaming_state.hpp"
#include "language_adapter.hpp"
#include <unordered_set>

namespace duckdb {

// Forward declarations for flat streaming functions
static void ReadASTFlatStreamingFunctionSequential(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output);
static void ReadASTFlatStreamingFunctionParallel(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output);
static void ProcessBatchOfFiles(ClientContext &context, ReadASTStreamingGlobalState &global_state, const vector<string> &batch_files);
static void PopulateDynamicColumns(DataChunk &output, idx_t output_idx, const ASTNode &node, const ExtractionConfig &config);

// Bind function for flat streaming two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTFlatStreamingBindTwoArg(ClientContext &context, TableFunctionBindInput &input,
                                                              vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("read_ast requires exactly 2 arguments: file_path and language");
    }
    
    auto file_path_value = input.inputs[0];
    auto language = input.inputs[1].GetValue<string>();
    
    // Handle both VARCHAR and LIST(VARCHAR) inputs (DuckDB-consistent pattern)
    vector<string> file_patterns;
    if (file_path_value.type().id() == LogicalTypeId::VARCHAR) {
        file_patterns.push_back(file_path_value.ToString());
    } else if (file_path_value.type().id() == LogicalTypeId::LIST) {
        auto &pattern_list = ListValue::GetChildren(file_path_value);
        if (pattern_list.empty()) {
            throw BinderException("File pattern list cannot be empty");
        }
        for (auto &pattern : pattern_list) {
            if (pattern.IsNull()) {
                throw BinderException("File pattern list cannot contain NULL values");
            }
            file_patterns.push_back(pattern.ToString());
        }
    } else {
        throw BinderException("File patterns must be VARCHAR or LIST(VARCHAR)");
    }
    
    // Check for duplicate parameters (following DuckDB YAML extension pattern)
    std::unordered_set<std::string> seen_parameters;
    for (auto &param : input.named_parameters) {
        if (seen_parameters.find(param.first) != seen_parameters.end()) {
            throw BinderException("Duplicate parameter name: " + param.first);
        }
        seen_parameters.insert(param.first);
    }
    
    // Parse optional named parameters
    bool ignore_errors = false;
    if (seen_parameters.find("ignore_errors") != seen_parameters.end()) {
        ignore_errors = input.named_parameters.at("ignore_errors").GetValue<bool>();
    }
    
    // Parse extraction config parameters
    string context_str = "native";  // Default to native - memory issues fixed with flat schema
    if (seen_parameters.find("context") != seen_parameters.end()) {
        context_str = input.named_parameters.at("context").GetValue<string>();
    }
    
    string source_str = "lines";  // Default
    if (seen_parameters.find("source") != seen_parameters.end()) {
        source_str = input.named_parameters.at("source").GetValue<string>();
    }
    
    string structure_str = "full";  // Default
    if (seen_parameters.find("structure") != seen_parameters.end()) {
        structure_str = input.named_parameters.at("structure").GetValue<string>();
    }
    
    // Parse unified peek parameter (can be INTEGER or VARCHAR)
    int32_t peek_size = 120;
    string peek_mode = "smart";
    if (seen_parameters.find("peek") != seen_parameters.end()) {
        auto& peek_value = input.named_parameters.at("peek");
        if (peek_value.type().id() == LogicalTypeId::INTEGER || 
            peek_value.type().id() == LogicalTypeId::BIGINT) {
            // INTEGER: custom size
            peek_size = peek_value.GetValue<int32_t>();
            peek_mode = "custom";
        } else {
            // VARCHAR: named mode
            string peek_str = peek_value.GetValue<string>();
            string peek_lower = StringUtil::Lower(peek_str);
            if (peek_lower == "none" || peek_lower == "smart" || peek_lower == "full") {
                peek_mode = peek_lower;
            } else {
                throw BinderException("Invalid peek parameter: " + peek_str + 
                                    ". Must be integer or one of: none, smart, full");
            }
        }
    }
    
    // Legacy parameter support (override if provided)
    if (seen_parameters.find("peek_size") != seen_parameters.end()) {
        peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
    }
    if (seen_parameters.find("peek_mode") != seen_parameters.end()) {
        peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
    }
    
    int32_t batch_size = 1;  // Default = current streaming behavior
    if (seen_parameters.find("batch_size") != seen_parameters.end()) {
        batch_size = input.named_parameters.at("batch_size").GetValue<int32_t>();
        if (batch_size < 1) {
            throw BinderException("batch_size must be positive");
        }
    }
    
    // Create ExtractionConfig from parsed parameters
    ExtractionConfig extraction_config = ParseExtractionConfig(context_str, source_str, structure_str, peek_mode, peek_size);
    
    // Use flat dynamic schema based on extraction config
    return_types = UnifiedASTBackend::GetFlatDynamicTableSchema(extraction_config);
    names = UnifiedASTBackend::GetFlatDynamicTableColumnNames(extraction_config);
    
    // Use the new ExtractionConfig constructor
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, extraction_config, batch_size);
}

// Bind function for flat streaming one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTFlatStreamingBindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                              vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 1) {
        throw BinderException("read_ast requires exactly 1 argument: file_path");
    }
    
    auto file_path_value = input.inputs[0];
    
    // Handle both VARCHAR and LIST(VARCHAR) inputs (DuckDB-consistent pattern)
    vector<string> file_patterns;
    if (file_path_value.type().id() == LogicalTypeId::VARCHAR) {
        file_patterns.push_back(file_path_value.ToString());
    } else if (file_path_value.type().id() == LogicalTypeId::LIST) {
        auto &pattern_list = ListValue::GetChildren(file_path_value);
        if (pattern_list.empty()) {
            throw BinderException("File pattern list cannot be empty");
        }
        for (auto &pattern : pattern_list) {
            if (pattern.IsNull()) {
                throw BinderException("File pattern list cannot contain NULL values");
            }
            file_patterns.push_back(pattern.ToString());
        }
    } else {
        throw BinderException("File patterns must be VARCHAR or LIST(VARCHAR)");
    }
    
    // Check for duplicate parameters (following DuckDB YAML extension pattern)
    std::unordered_set<std::string> seen_parameters;
    for (auto &param : input.named_parameters) {
        if (seen_parameters.find(param.first) != seen_parameters.end()) {
            throw BinderException("Duplicate parameter name: " + param.first);
        }
        seen_parameters.insert(param.first);
    }
    
    // Parse optional named parameters
    bool ignore_errors = false;
    if (seen_parameters.find("ignore_errors") != seen_parameters.end()) {
        ignore_errors = input.named_parameters.at("ignore_errors").GetValue<bool>();
    }
    
    // Parse extraction config parameters
    string context_str = "native";  // Default to native - memory issues fixed with flat schema
    if (seen_parameters.find("context") != seen_parameters.end()) {
        context_str = input.named_parameters.at("context").GetValue<string>();
    }
    
    string source_str = "lines";  // Default
    if (seen_parameters.find("source") != seen_parameters.end()) {
        source_str = input.named_parameters.at("source").GetValue<string>();
    }
    
    string structure_str = "full";  // Default
    if (seen_parameters.find("structure") != seen_parameters.end()) {
        structure_str = input.named_parameters.at("structure").GetValue<string>();
    }
    
    // Parse unified peek parameter (can be INTEGER or VARCHAR)
    int32_t peek_size = 120;
    string peek_mode = "smart";
    if (seen_parameters.find("peek") != seen_parameters.end()) {
        auto& peek_value = input.named_parameters.at("peek");
        if (peek_value.type().id() == LogicalTypeId::INTEGER || 
            peek_value.type().id() == LogicalTypeId::BIGINT) {
            // INTEGER: custom size
            peek_size = peek_value.GetValue<int32_t>();
            peek_mode = "custom";
        } else {
            // VARCHAR: named mode
            string peek_str = peek_value.GetValue<string>();
            string peek_lower = StringUtil::Lower(peek_str);
            if (peek_lower == "none" || peek_lower == "smart" || peek_lower == "full") {
                peek_mode = peek_lower;
            } else {
                throw BinderException("Invalid peek parameter: " + peek_str + 
                                    ". Must be integer or one of: none, smart, full");
            }
        }
    }
    
    // Legacy parameter support (override if provided)
    if (seen_parameters.find("peek_size") != seen_parameters.end()) {
        peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
    }
    if (seen_parameters.find("peek_mode") != seen_parameters.end()) {
        peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
    }
    
    int32_t batch_size = 1;  // Default = current streaming behavior
    if (seen_parameters.find("batch_size") != seen_parameters.end()) {
        batch_size = input.named_parameters.at("batch_size").GetValue<int32_t>();
        if (batch_size < 1) {
            throw BinderException("batch_size must be positive");
        }
    }
    
    // Use auto-detect for language
    string language = "auto";
    
    // Create ExtractionConfig from parsed parameters
    ExtractionConfig extraction_config = ParseExtractionConfig(context_str, source_str, structure_str, peek_mode, peek_size);
    
    // Use flat dynamic schema based on extraction config
    return_types = UnifiedASTBackend::GetFlatDynamicTableSchema(extraction_config);
    names = UnifiedASTBackend::GetFlatDynamicTableColumnNames(extraction_config);
    
    // Use the new ExtractionConfig constructor
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, extraction_config, batch_size);
}

// Initialize global state for streaming with parallel processing
static unique_ptr<GlobalTableFunctionState> ReadASTStreamingInit(ClientContext &context, TableFunctionInitInput &input) {
    auto &bind_data = input.bind_data->Cast<ReadASTStreamingBindData>();
    auto result = make_uniq<ReadASTStreamingGlobalState>();
    
    // Store configuration
    result->language = bind_data.language;
    result->ignore_errors = bind_data.ignore_errors;
    result->peek_size = bind_data.peek_size;
    result->peek_mode = bind_data.peek_mode;
    result->batch_size = bind_data.batch_size;
    result->extraction_config = bind_data.extraction_config;
    
    try {
        // Use our reliable ASTFileUtils for pattern expansion and deduplication
        vector<string> supported_extensions;
        if (bind_data.language != "auto") {
            supported_extensions = ASTFileUtils::GetSupportedExtensions(bind_data.language);
        }
        
        auto expanded_files = ASTFileUtils::GetFiles(context, bind_data.file_patterns, 
                                                    bind_data.ignore_errors, supported_extensions);
        
        if (expanded_files.empty()) {
            if (!bind_data.ignore_errors) {
                throw IOException("read_ast needs at least one file to read");
            }
            result->files_exhausted = true;
            return std::move(result);
        }
        
        // ADAPTIVE PARALLEL PROCESSING based on file count
        const auto total_files = expanded_files.size();
        const auto num_threads = NumericCast<idx_t>(TaskScheduler::GetScheduler(context).NumberOfThreads());
        
        if (total_files >= 4) {  // Use parallel processing for 4+ files
            // Store file list for parallel processing (no batching!)
            result->all_file_paths = std::move(expanded_files);
            result->use_parallel_batching = true;  // Reuse flag but no actual batching
            result->files_exhausted = false;
            
            // Pre-resolve languages for all files to avoid repeated work
            result->resolved_languages.reserve(result->all_file_paths.size());
            std::unordered_set<string> unique_languages;
            
            for (const auto& file_path : result->all_file_paths) {
                string file_language = bind_data.language;
                if (bind_data.language == "auto" || bind_data.language.empty()) {
                    file_language = ASTFileUtils::DetectLanguageFromPath(file_path);
                    if (file_language == "auto") {
                        if (!bind_data.ignore_errors) {
                            throw BinderException("Could not detect language for file: " + file_path);
                        }
                        file_language = "unknown"; // Will be skipped during processing
                    }
                }
                result->resolved_languages.push_back(file_language);
                if (file_language != "unknown") {
                    unique_languages.insert(file_language);
                }
            }
            
            // MEMORY SAFETY FIX: Validate language support without caching adapters
            // Fresh adapters will be created for each file to prevent state accumulation
            auto& registry = LanguageAdapterRegistry::GetInstance();
            for (const auto& language : unique_languages) {
                auto adapter = registry.CreateAdapter(language);
                if (!adapter && !bind_data.ignore_errors) {
                    throw InvalidInputException("Unsupported language: " + language);
                }
                // Don't store adapter - fresh ones will be created per file
            }
        } else {
            // Use traditional single-threaded streaming for small file sets
            result->use_parallel_batching = false;
            
            // Create file list using DuckDB's pattern for backward compatibility
            auto multi_file_reader = MultiFileReader::CreateDefault("read_ast");
            vector<Value> file_values;
            for (const auto &file_path : expanded_files) {
                file_values.push_back(Value(file_path));
            }
            
            auto files_list_value = Value::LIST(LogicalType::VARCHAR, file_values);
            result->file_list = multi_file_reader->CreateFileList(context, files_list_value);
            if (result->file_list) {
                result->file_list->InitializeScan(result->file_scan_state);
            } else {
                throw InternalException("Failed to create file list");
            }
        }
        
    } catch (const Exception &e) {
        if (!bind_data.ignore_errors) {
            throw IOException("Failed to initialize file processing: " + string(e.what()));
        }
        result->files_exhausted = true;
    }
    
    return std::move(result);
}

// Streaming execution function with parallel batch processing
// Helper function to populate columns dynamically based on ExtractionConfig - DIRECT FIELD ACCESS
static void PopulateDynamicColumns(DataChunk &output, idx_t output_idx, const ASTNode &node, const ExtractionConfig &config) {
    idx_t column_idx = 0;
    
    // DIRECT FIELD ACCESS: No Value struct conversion, no hardcoded indices
    
    // Always include core columns
    output.SetValue(column_idx++, output_idx, Value::UBIGINT(node.node_id));
    output.SetValue(column_idx++, output_idx, Value(node.type_raw));
    
    // Conditionally include columns based on config
    if (config.source != SourceLevel::NONE) {
        if (config.source >= SourceLevel::PATH) {
            output.SetValue(column_idx++, output_idx, node.file_path.empty() ? Value(LogicalType::VARCHAR) : Value(node.file_path));
            output.SetValue(column_idx++, output_idx, node.language.empty() ? Value(LogicalType::VARCHAR) : Value(node.language));
        }
        if (config.source >= SourceLevel::LINES_ONLY) {
            output.SetValue(column_idx++, output_idx, Value::UINTEGER(node.source_start_line));
            output.SetValue(column_idx++, output_idx, Value::UINTEGER(node.source_end_line));
        }
        if (config.source >= SourceLevel::FULL) {
            output.SetValue(column_idx++, output_idx, Value::UINTEGER(node.source_start_column));
            output.SetValue(column_idx++, output_idx, Value::UINTEGER(node.source_end_column));
        }
    }
    
    if (config.structure != StructureLevel::NONE) {
        if (config.structure >= StructureLevel::MINIMAL) {
            output.SetValue(column_idx++, output_idx, node.parent_id < 0 ? Value(LogicalType::BIGINT) : Value::BIGINT(node.parent_id));
            output.SetValue(column_idx++, output_idx, Value::UINTEGER(node.depth));
            output.SetValue(column_idx++, output_idx, Value::UINTEGER(node.sibling_index));
        }
        if (config.structure >= StructureLevel::FULL) {
            output.SetValue(column_idx++, output_idx, Value::UINTEGER(node.children_count));
            output.SetValue(column_idx++, output_idx, Value::UINTEGER(node.descendant_count));
        }
    }
    
    if (config.context != ContextLevel::NONE) {
        if (config.context >= ContextLevel::NORMALIZED) {
            output.SetValue(column_idx++, output_idx, node.name_raw.empty() ? Value(LogicalType::VARCHAR) : Value(node.name_raw));
        }
        if (config.context >= ContextLevel::NODE_TYPES_ONLY) {
            output.SetValue(column_idx++, output_idx, Value::UTINYINT(node.semantic_type));
            output.SetValue(column_idx++, output_idx, Value::UTINYINT(node.universal_flags));
        }
        if (config.context >= ContextLevel::NATIVE) {
            // Only create native Value struct if extraction was attempted and data exists
            if (node.native_extraction_attempted && !node.native.signature_type.empty()) {
                child_list_t<Value> native_values;
                native_values.emplace_back("signature_type", Value(node.native.signature_type));
                
                // Create parameters list
                vector<Value> parameter_values;
                for (const auto& param : node.native.parameters) {
                    child_list_t<Value> param_struct;
                    param_struct.emplace_back("name", Value(param.name));
                    param_struct.emplace_back("type", Value(param.type));
                    param_struct.emplace_back("default_value", Value(param.default_value));
                    param_struct.emplace_back("is_optional", Value::BOOLEAN(param.is_optional));
                    param_struct.emplace_back("is_variadic", Value::BOOLEAN(param.is_variadic));
                    param_struct.emplace_back("annotations", Value(param.annotations));
                    parameter_values.push_back(Value::STRUCT(param_struct));
                }
                native_values.emplace_back("parameters", Value::LIST(LogicalType::STRUCT({
                    {"name", LogicalType::VARCHAR},
                    {"type", LogicalType::VARCHAR},
                    {"default_value", LogicalType::VARCHAR},
                    {"is_optional", LogicalType::BOOLEAN},
                    {"is_variadic", LogicalType::BOOLEAN},
                    {"annotations", LogicalType::VARCHAR}
                }), parameter_values));
                
                // Create modifiers list
                vector<Value> modifier_values;
                for (const auto& modifier : node.native.modifiers) {
                    modifier_values.push_back(Value(modifier));
                }
                native_values.emplace_back("modifiers", Value::LIST(LogicalType::VARCHAR, modifier_values));
                
                native_values.emplace_back("qualified_name", Value(node.native.qualified_name));
                native_values.emplace_back("annotations", Value(node.native.annotations));
                
                output.SetValue(column_idx++, output_idx, Value::STRUCT(native_values));
            } else {
                // No native context available - use NULL struct
                child_list_t<LogicalType> native_schema;
                native_schema.push_back(make_pair("signature_type", LogicalType::VARCHAR));
                native_schema.push_back(make_pair("parameters", LogicalType::LIST(LogicalType::STRUCT({
                    {"name", LogicalType::VARCHAR},
                    {"type", LogicalType::VARCHAR},
                    {"default_value", LogicalType::VARCHAR},
                    {"is_optional", LogicalType::BOOLEAN},
                    {"is_variadic", LogicalType::BOOLEAN},
                    {"annotations", LogicalType::VARCHAR}
                }))));
                native_schema.push_back(make_pair("modifiers", LogicalType::LIST(LogicalType::VARCHAR)));
                native_schema.push_back(make_pair("qualified_name", LogicalType::VARCHAR));
                native_schema.push_back(make_pair("annotations", LogicalType::VARCHAR));
                output.SetValue(column_idx++, output_idx, Value(LogicalType::STRUCT(native_schema)));
            }
        }
    }
    
    if (config.peek != PeekLevel::NONE) {
        output.SetValue(column_idx++, output_idx, Value(node.peek));
    }
}

static void ReadASTFlatStreamingFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &global_state = data_p.global_state->Cast<ReadASTStreamingGlobalState>();
    
    if (global_state.files_exhausted) {
        output.SetCardinality(0);
        return;
    }
    
    // Route to appropriate processing mode
    if (global_state.use_parallel_batching) {
        ReadASTFlatStreamingFunctionParallel(context, global_state, output);
    } else {
        ReadASTFlatStreamingFunctionSequential(context, global_state, output);
    }
}

// Process a batch of files with shared parser context for memory efficiency
static void ProcessBatchOfFiles(ClientContext &context, ReadASTStreamingGlobalState &global_state, const vector<string> &batch_files) {
    if (batch_files.empty()) {
        return;
    }
    
    // Clear any existing batch results
    global_state.current_batch_results.clear();
    global_state.current_batch_result_index = 0;
    global_state.current_batch_row_index = 0;
    
    auto& registry = LanguageAdapterRegistry::GetInstance();
    
    // Process each file in the batch (auto-detect language per file if needed)
    for (const auto& file_path : batch_files) {
        try {
            // Determine language for this specific file
            string file_language = global_state.language;
            if (file_language == "auto") {
                file_language = ASTFileUtils::DetectLanguageFromPath(file_path);
                if (file_language == "auto") {
                    if (!global_state.ignore_errors) {
                        throw BinderException("Could not detect language for file: " + file_path);
                    }
                    continue; // Skip this file
                }
            }
            
            // Read file content using DuckDB conventions
            auto& fs = FileSystem::GetFileSystem(context);
            if (!fs.FileExists(file_path)) {
                if (!global_state.ignore_errors) {
                    throw IOException("File does not exist: " + file_path);
                }
                continue; // Skip missing files
            }
            
            auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
            auto file_size = fs.GetFileSize(*handle);
            
            string content;
            content.resize(file_size);
            handle->Read((void*)content.data(), file_size);
            
            // Use ParseSingleFileToASTResult with ExtractionConfig
            auto result_ptr = UnifiedASTBackend::ParseSingleFileToASTResult(
                context, file_path, file_language, global_state.ignore_errors,
                global_state.extraction_config
            );
            
            if (result_ptr) {
                global_state.current_batch_results.emplace_back(std::move(*result_ptr));
            }
            // If result_ptr is null, file was skipped due to errors (when ignore_errors=true)
            
        } catch (const std::exception& e) {
            if (!global_state.ignore_errors) {
                // This should rarely happen since ParseSingleFileToASTResult handles errors
                throw IOException("Failed to process " + file_path + ": " + e.what());
            }
            // Continue with next file on error when ignore_errors is true
        }
    }
}

// Sequential processing for small file sets (backward compatibility)
static void ReadASTFlatStreamingFunctionSequential(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output) {
    idx_t output_index = 0;
    
    // Use flat projection for individual columns
    // Columns: node_id, type, name, file_path, language, start_line, start_column, end_line, end_column, parent_id, depth, sibling_index, children_count, descendant_count, peek, semantic_type, flags
    
    while (output_index < STANDARD_VECTOR_SIZE) {
        // Handle batch processing if enabled
        if (global_state.batch_size > 1) {
            // Check if we need to process a new batch
            if (global_state.current_batch_results.empty() || 
                global_state.current_batch_result_index >= global_state.current_batch_results.size()) {
                
                // Collect next batch of files
                vector<string> batch_files;
                batch_files.reserve(global_state.batch_size);
                
                OpenFileInfo file;
                for (int32_t i = 0; i < global_state.batch_size; i++) {
                    if (!global_state.file_list->Scan(global_state.file_scan_state, file)) {
                        global_state.files_exhausted = true;
                        break;
                    }
                    batch_files.push_back(file.path);
                }
                
                if (batch_files.empty()) {
                    break; // No more files to process
                }
                
                // Process the batch
                ProcessBatchOfFiles(context, global_state, batch_files);
            }
            
            // Output nodes from current batch results using flat projection
            while (output_index < STANDARD_VECTOR_SIZE && 
                   global_state.current_batch_result_index < global_state.current_batch_results.size()) {
                
                auto& result = global_state.current_batch_results[global_state.current_batch_result_index];
                
                if (global_state.current_batch_row_index < result.nodes.size()) {
                    // Use dynamic projection to populate individual columns based on extraction config
                    UnifiedASTBackend::ProjectToDynamicTable(result, output, global_state.current_batch_row_index, output_index, global_state.extraction_config);
                    
                    global_state.current_batch_row_index++;
                } else {
                    // Move to next result in batch
                    global_state.current_batch_result_index++;
                    global_state.current_batch_row_index = 0;
                }
            }
            
            // If we're in batch mode, continue to next iteration
            continue;
        }
        
        // Original single-file processing logic (when batch_size == 1)
        // Check if we need to parse a new file
        if (!global_state.current_file_parsed || 
            !global_state.current_file_result ||
            global_state.current_file_row_index >= global_state.current_file_result->nodes.size()) {
            
            // Try to get next file that matches our language criteria
            OpenFileInfo file;
            bool found_valid_file = false;
            
            while (!found_valid_file) {
                if (!global_state.file_list->Scan(global_state.file_scan_state, file)) {
                    // No more files
                    global_state.files_exhausted = true;
                    break;
                }
                
                // Only check file extensions when using auto-detection
                // If a language is explicitly provided, allow parsing any file
                // (even if it might result in 0 nodes due to parse failure)
                
                found_valid_file = true;
            }
            
            if (!found_valid_file) {
                break; // No more valid files
            }
            
            // Parse this single file with ExtractionConfig
            global_state.current_file_result = UnifiedASTBackend::ParseSingleFileToASTResult(
                context, file.path, global_state.language, global_state.ignore_errors, 
                global_state.extraction_config);
            
            if (!global_state.current_file_result) {
                // File was skipped due to errors, continue to next file
                continue;
            }
            
            global_state.current_file_row_index = 0;
            global_state.current_file_parsed = true;
        }
        
        // Emit rows from current file using flat projection
        if (!global_state.current_file_result) {
            // This shouldn't happen, but add safety check
            break;
        }
        
        // Use flat projection to populate individual columns
        UnifiedASTBackend::ProjectToDynamicTable(*global_state.current_file_result, output, global_state.current_file_row_index, output_index, global_state.extraction_config);
    }
    
    output.SetCardinality(output_index);
}

// PARALLEL PROCESSING without batching - let DuckDB handle task distribution
// Helper function to convert ExtractionConfig to legacy parameters for ParsingFunction compatibility
static pair<int32_t, string> ConvertExtractionConfigToLegacyParams(const ExtractionConfig &config) {
    int32_t peek_size;
    string peek_mode;
    
    switch (config.peek) {
        case PeekLevel::NONE:
            peek_size = 0;
            peek_mode = "none";
            break;
        case PeekLevel::SMART:
            peek_size = -1;  // Smart mode uses adaptive sizing
            peek_mode = "smart";
            break;
        case PeekLevel::FULL:
            peek_size = -2;  // Full mode means read entire file
            peek_mode = "full";
            break;
        case PeekLevel::CUSTOM:
            peek_size = config.peek_size;
            peek_mode = "custom";
            break;
        default:
            peek_size = -1;
            peek_mode = "smart";
            break;
    }
    
    return make_pair(peek_size, peek_mode);
}

static void ReadASTFlatStreamingFunctionParallel(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output) {
    // Check if we need to do the one-time parallel processing
    if (!global_state.parallel_processing_complete) {
        const auto num_threads = NumericCast<idx_t>(TaskScheduler::GetScheduler(context).NumberOfThreads());
        const auto files_per_task = MaxValue<idx_t>((global_state.all_file_paths.size() + num_threads - 1) / num_threads, 1);
        const auto num_tasks = (global_state.all_file_paths.size() + files_per_task - 1) / files_per_task;
        
        // Convert ExtractionConfig to legacy parameters for ParsingFunction compatibility
        auto legacy_params = ConvertExtractionConfigToLegacyParams(global_state.extraction_config);
        auto peek_size = legacy_params.first;
        auto peek_mode = legacy_params.second;
        
        // Create parsing state for ALL files at once using converted parameters
        ASTParsingState parsing_state(context, global_state.all_file_paths, global_state.resolved_languages, 
                                    global_state.ignore_errors, peek_size, peek_mode,
                                    global_state.pre_created_adapters, num_tasks);
        
        // Create tasks - let DuckDB's scheduler handle the distribution
        TaskExecutor executor(context);
        for (idx_t task_idx = 0; task_idx < num_tasks; task_idx++) {
            const auto file_idx_start = task_idx * files_per_task;
            const auto file_idx_end = MinValue<idx_t>(file_idx_start + files_per_task, global_state.all_file_paths.size());
            
            auto task = make_uniq<ASTParsingTask>(executor, parsing_state, file_idx_start, file_idx_end, task_idx);
            executor.ScheduleTask(std::move(task));
        }
        
        // Let DuckDB handle all the parallel work - no artificial batching!
        executor.WorkOnTasks();
        
        // Collect all results
        parsing_state.CollectResults();
        
        // Store all results for streaming
        global_state.current_batch_results = std::move(parsing_state.results);
        global_state.current_batch_result_index = 0;
        global_state.current_batch_row_index = 0;
        global_state.parallel_processing_complete = true;
    }
    
    // Now just stream the results using flat projection
    idx_t output_index = 0;
    
    // Use flat projection for individual columns
    // Columns: node_id, type, name, file_path, language, start_line, start_column, end_line, end_column, parent_id, depth, sibling_index, children_count, descendant_count, peek, semantic_type, flags

    // Stream results from all completed parsing
    while (output_index < STANDARD_VECTOR_SIZE && 
           global_state.current_batch_result_index < global_state.current_batch_results.size()) {
        
        const auto& current_result = global_state.current_batch_results[global_state.current_batch_result_index];
        
        // Check if we've exhausted this result
        if (global_state.current_batch_row_index >= current_result.nodes.size()) {
            global_state.current_batch_result_index++;
            global_state.current_batch_row_index = 0;
            continue;
        }
        
        // AGENT J FIX: Call ProjectToDynamicTable once per result, let it process multiple nodes
        // This function will process nodes starting from current_batch_row_index and increment output_index
        UnifiedASTBackend::ProjectToDynamicTable(current_result, output, global_state.current_batch_row_index, output_index, global_state.extraction_config);
        
        // AGENT J FIX: Don't increment manually - ProjectToDynamicTable handles row advancement
        // If ProjectToDynamicTable processed all remaining nodes, move to next result
        if (global_state.current_batch_row_index >= current_result.nodes.size()) {
            global_state.current_batch_result_index++;
            global_state.current_batch_row_index = 0;
        }
    }
    
    // Mark as exhausted when all results have been streamed
    if (global_state.current_batch_result_index >= global_state.current_batch_results.size()) {
        global_state.files_exhausted = true;
    }
    
    output.SetCardinality(output_index);
}

//==============================================================================
// NEW: Hierarchical Schema Bind Functions
//==============================================================================

// Hierarchical bind function for two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTHierarchicalBindTwoArg(ClientContext &context, TableFunctionBindInput &input,
                                                              vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("read_ast requires exactly 2 arguments: file_path and language");
    }
    
    auto file_path_value = input.inputs[0];
    auto language = input.inputs[1].GetValue<string>();
    
    // Handle both VARCHAR and LIST(VARCHAR) inputs (DuckDB-consistent pattern)
    vector<string> file_patterns;
    if (file_path_value.type().id() == LogicalTypeId::VARCHAR) {
        file_patterns.push_back(file_path_value.ToString());
    } else if (file_path_value.type().id() == LogicalTypeId::LIST) {
        auto &pattern_list = ListValue::GetChildren(file_path_value);
        if (pattern_list.empty()) {
            throw BinderException("File pattern list cannot be empty");
        }
        for (auto &pattern : pattern_list) {
            if (pattern.IsNull()) {
                throw BinderException("File pattern list cannot contain NULL values");
            }
            file_patterns.push_back(pattern.ToString());
        }
    } else {
        throw BinderException("File patterns must be VARCHAR or LIST(VARCHAR)");
    }
    
    // Extract named parameters
    bool ignore_errors = false;
    int32_t peek_size = 120;
    string peek_mode = "smart";
    int32_t batch_size = 100;
    
    for (auto &kv : input.named_parameters) {
        if (kv.first == "ignore_errors") {
            ignore_errors = kv.second.GetValue<bool>();
        } else if (kv.first == "peek_size") {
            peek_size = kv.second.GetValue<int32_t>();
        } else if (kv.first == "peek_mode") {
            peek_mode = kv.second.GetValue<string>();
        } else if (kv.first == "batch_size") {
            batch_size = kv.second.GetValue<int32_t>();
            if (batch_size <= 0) {
                throw BinderException("batch_size must be positive");
            }
        }
    }
    
    // Use hierarchical backend schema for structured access
    return_types = UnifiedASTBackend::GetHierarchicalTableSchema();
    names = UnifiedASTBackend::GetHierarchicalTableColumnNames();
    
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, peek_size, peek_mode, batch_size);
}

// Hierarchical bind function for one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTHierarchicalBindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                              vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 1) {
        throw BinderException("read_ast requires exactly 1 argument: file_path");
    }
    
    auto file_path_value = input.inputs[0];
    
    // Handle both VARCHAR and LIST(VARCHAR) inputs
    vector<string> file_patterns;
    if (file_path_value.type().id() == LogicalTypeId::VARCHAR) {
        file_patterns.push_back(file_path_value.ToString());
    } else if (file_path_value.type().id() == LogicalTypeId::LIST) {
        auto &pattern_list = ListValue::GetChildren(file_path_value);
        if (pattern_list.empty()) {
            throw BinderException("File pattern list cannot be empty");
        }
        for (auto &pattern : pattern_list) {
            if (pattern.IsNull()) {
                throw BinderException("File pattern list cannot contain NULL values");
            }
            file_patterns.push_back(pattern.ToString());
        }
    } else {
        throw BinderException("File patterns must be VARCHAR or LIST(VARCHAR)");
    }
    
    // Extract named parameters
    bool ignore_errors = false;
    int32_t peek_size = 120;
    string peek_mode = "smart";
    int32_t batch_size = 100;
    
    for (auto &kv : input.named_parameters) {
        if (kv.first == "ignore_errors") {
            ignore_errors = kv.second.GetValue<bool>();
        } else if (kv.first == "peek_size") {
            peek_size = kv.second.GetValue<int32_t>();
        } else if (kv.first == "peek_mode") {
            peek_mode = kv.second.GetValue<string>();
        } else if (kv.first == "batch_size") {
            batch_size = kv.second.GetValue<int32_t>();
            if (batch_size <= 0) {
                throw BinderException("batch_size must be positive");
            }
        }
    }
    
    // Use auto-detect for language
    string language = "auto";
    
    // Use hierarchical backend schema for structured access
    return_types = UnifiedASTBackend::GetHierarchicalTableSchema();
    names = UnifiedASTBackend::GetHierarchicalTableColumnNames();
    
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, peek_size, peek_mode, batch_size);
}

// Hierarchical streaming functions - use NEW structured fields and hierarchical layout
static void ReadASTHierarchicalFunctionSequential(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output) {
    idx_t output_index = 0;
    
    while (output_index < STANDARD_VECTOR_SIZE) {
        // Handle batch processing if enabled
        if (global_state.batch_size > 1) {
            // Process batches and use streaming projection
            if (global_state.current_batch_results.empty() || 
                global_state.current_batch_result_index >= global_state.current_batch_results.size()) {
                
                vector<string> batch_files;
                batch_files.reserve(global_state.batch_size);
                
                OpenFileInfo file;
                for (int32_t i = 0; i < global_state.batch_size; i++) {
                    if (!global_state.file_list->Scan(global_state.file_scan_state, file)) {
                        break;
                    }
                    batch_files.push_back(file.path);
                }
                
                if (batch_files.empty()) {
                    break;
                }
                
                ProcessBatchOfFiles(context, global_state, batch_files);
                global_state.current_batch_result_index = 0;
                global_state.current_batch_row_index = 0;
            }
            
            // Process current batch using hierarchical streaming projection
            if (global_state.current_batch_result_index < global_state.current_batch_results.size()) {
                const auto& result = global_state.current_batch_results[global_state.current_batch_result_index];
                
                if (global_state.current_batch_row_index < result.nodes.size()) {
                    // Use streaming projection for this batch result
                    idx_t old_output_index = output_index;
                    UnifiedASTBackend::ProjectToHierarchicalTableStreaming(
                        result.nodes, output, global_state.current_batch_row_index, output_index, result.source);
                    
                    // Update tracking based on how many rows were processed
                    idx_t rows_processed = output_index - old_output_index;
                    global_state.current_batch_row_index += rows_processed;
                    
                    if (global_state.current_batch_row_index >= result.nodes.size()) {
                        // Move to next result in batch
                        global_state.current_batch_result_index++;
                        global_state.current_batch_row_index = 0;
                    }
                } else {
                    // Move to next result in batch
                    global_state.current_batch_result_index++;
                    global_state.current_batch_row_index = 0;
                }
            } else {
                break;
            }
        } else {
            // Single file processing using hierarchical streaming projection
            // Check if we need to parse a new file
            if (!global_state.current_file_parsed || 
                !global_state.current_file_result ||
                global_state.current_file_row_index >= global_state.current_file_result->nodes.size()) {
                
                // Try to get next file
                OpenFileInfo file;
                bool found_valid_file = false;
                
                while (!found_valid_file) {
                    if (!global_state.file_list->Scan(global_state.file_scan_state, file)) {
                        // No more files
                        global_state.files_exhausted = true;
                        break;
                    }
                    found_valid_file = true;
                }
                
                if (!found_valid_file) {
                    break; // No more valid files
                }
                
                // Parse this single file with ExtractionConfig
                global_state.current_file_result = UnifiedASTBackend::ParseSingleFileToASTResult(
                    context, file.path, global_state.language, global_state.ignore_errors, 
                    global_state.extraction_config);
                
                if (!global_state.current_file_result) {
                    // File was skipped due to errors, continue to next file
                    continue;
                }
                
                global_state.current_file_row_index = 0;
                global_state.current_file_parsed = true;
            }
            
            // Emit rows from current file using hierarchical streaming projection
            if (!global_state.current_file_result) {
                break;
            }
            
            // Use streaming projection for current file
            idx_t old_output_index = output_index;
            UnifiedASTBackend::ProjectToHierarchicalTableStreaming(
                global_state.current_file_result->nodes, output, 
                global_state.current_file_row_index, output_index, 
                global_state.current_file_result->source);
            
            // Update tracking based on how many rows were processed
            idx_t rows_processed = output_index - old_output_index;
            global_state.current_file_row_index += rows_processed;
        }
    }
    
    output.SetCardinality(output_index);
}

static void ReadASTHierarchicalFunctionParallel(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output) {
    // Check if we need to do the one-time parallel processing
    if (!global_state.parallel_processing_complete) {
        const auto num_threads = NumericCast<idx_t>(TaskScheduler::GetScheduler(context).NumberOfThreads());
        const auto files_per_task = MaxValue<idx_t>((global_state.all_file_paths.size() + num_threads - 1) / num_threads, 1);
        const auto num_tasks = (global_state.all_file_paths.size() + files_per_task - 1) / files_per_task;
        
        // Create parsing state for ALL files at once
        ASTParsingState parsing_state(context, global_state.all_file_paths, global_state.resolved_languages, 
                                    global_state.ignore_errors, global_state.peek_size, global_state.peek_mode,
                                    global_state.pre_created_adapters, num_tasks);
        
        // Create tasks - let DuckDB's scheduler handle the distribution
        TaskExecutor executor(context);
        for (idx_t task_idx = 0; task_idx < num_tasks; task_idx++) {
            const auto file_idx_start = task_idx * files_per_task;
            const auto file_idx_end = MinValue<idx_t>(file_idx_start + files_per_task, global_state.all_file_paths.size());
            
            auto task = make_uniq<ASTParsingTask>(executor, parsing_state, file_idx_start, file_idx_end, task_idx);
            executor.ScheduleTask(std::move(task));
        }
        
        // Let DuckDB handle all the parallel work
        executor.WorkOnTasks();
        
        // Collect all results
        parsing_state.CollectResults();
        
        // Store all results for streaming
        global_state.current_batch_results = std::move(parsing_state.results);
        global_state.current_batch_result_index = 0;
        global_state.current_batch_row_index = 0;
        global_state.parallel_processing_complete = true;
    }
    
    // Stream the results using hierarchical streaming projection
    idx_t output_index = 0;
    
    // Stream results from all completed parsing using hierarchical streaming projection
    while (output_index < STANDARD_VECTOR_SIZE && 
           global_state.current_batch_result_index < global_state.current_batch_results.size()) {
        
        const auto& current_result = global_state.current_batch_results[global_state.current_batch_result_index];
        
        if (global_state.current_batch_row_index >= current_result.nodes.size()) {
            global_state.current_batch_result_index++;
            global_state.current_batch_row_index = 0;
            continue;
        }
        
        // Use streaming projection for this result
        idx_t old_output_index = output_index;
        
        UnifiedASTBackend::ProjectToHierarchicalTableStreaming(
            current_result.nodes, output, global_state.current_batch_row_index, output_index, current_result.source);
        
        // Update tracking based on how many rows were processed
        idx_t rows_processed = output_index - old_output_index;
        
        global_state.current_batch_row_index += rows_processed;
        
        if (global_state.current_batch_row_index >= current_result.nodes.size()) {
            global_state.current_batch_result_index++;
            global_state.current_batch_row_index = 0;
        }
    }
    
    if (global_state.current_batch_result_index >= global_state.current_batch_results.size()) {
        global_state.files_exhausted = true;
    }
    
    output.SetCardinality(output_index);
}

// Main hierarchical execute function - MATCH FLAT VERSION ROUTING LOGIC
static void ReadASTHierarchicalFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
    auto &global_state = data.global_state->Cast<ReadASTStreamingGlobalState>();
    
    if (global_state.files_exhausted) {
        output.SetCardinality(0);
        return;
    }
    
    // Route to appropriate processing mode - SAME AS FLAT VERSION
    if (global_state.use_parallel_batching) {
        ReadASTHierarchicalFunctionParallel(context, global_state, output);
    } else {
        ReadASTHierarchicalFunctionSequential(context, global_state, output);
    }
}

//==============================================================================
// Legacy Flat Schema Functions
//==============================================================================

// Flat schema read_ast functions using flat streaming implementation
static TableFunction GetReadASTFlatFunctionTwoArg() {
    TableFunction read_ast("read_ast", {LogicalType::ANY, LogicalType::VARCHAR}, 
                          ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindTwoArg, ReadASTStreamingInit);
    read_ast.name = "read_ast";
    read_ast.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast.named_parameters["context"] = LogicalType::VARCHAR;
    read_ast.named_parameters["source"] = LogicalType::VARCHAR;
    read_ast.named_parameters["structure"] = LogicalType::VARCHAR;
    read_ast.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    read_ast.named_parameters["batch_size"] = LogicalType::INTEGER;
    
    // Legacy parameters for backward compatibility
    read_ast.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    return read_ast;
}


// Legacy flat schema functions
static TableFunction GetReadASTFlatFunctionOneArg() {
    TableFunction read_ast("read_ast", {LogicalType::ANY}, 
                          ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindOneArg, ReadASTStreamingInit);
    read_ast.name = "read_ast";
    read_ast.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast.named_parameters["context"] = LogicalType::VARCHAR;
    read_ast.named_parameters["source"] = LogicalType::VARCHAR;
    read_ast.named_parameters["structure"] = LogicalType::VARCHAR;
    read_ast.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    read_ast.named_parameters["batch_size"] = LogicalType::INTEGER;
    
    // Legacy parameters for backward compatibility
    read_ast.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    return read_ast;
}


// Keep streaming functions as well for explicit access (these use flat schema)
static TableFunction GetReadASTStreamingFunctionTwoArg() {
    TableFunction read_ast_streaming("read_ast_streaming", {LogicalType::ANY, LogicalType::VARCHAR}, 
                                   ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindTwoArg, ReadASTStreamingInit);
    read_ast_streaming.name = "read_ast_streaming";
    read_ast_streaming.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_streaming.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_streaming.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    read_ast_streaming.named_parameters["batch_size"] = LogicalType::INTEGER;
    return read_ast_streaming;
}

static TableFunction GetReadASTStreamingFunctionOneArg() {
    TableFunction read_ast_streaming("read_ast_streaming", {LogicalType::ANY}, 
                                   ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindOneArg, ReadASTStreamingInit);
    read_ast_streaming.name = "read_ast_streaming";
    read_ast_streaming.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_streaming.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_streaming.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    read_ast_streaming.named_parameters["batch_size"] = LogicalType::INTEGER;
    return read_ast_streaming;
}

// Hierarchical streaming bind function for two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTHierarchicalStreamingBindTwoArg(ClientContext &context, TableFunctionBindInput &input,
                                                                       vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("read_ast requires exactly 2 arguments: file_path and language");
    }
    
    auto file_path_value = input.inputs[0];
    auto language = input.inputs[1].GetValue<string>();
    
    // Handle both VARCHAR and LIST(VARCHAR) inputs (DuckDB-consistent pattern)
    vector<string> file_patterns;
    if (file_path_value.type().id() == LogicalTypeId::VARCHAR) {
        file_patterns.push_back(file_path_value.ToString());
    } else if (file_path_value.type().id() == LogicalTypeId::LIST) {
        auto &pattern_list = ListValue::GetChildren(file_path_value);
        if (pattern_list.empty()) {
            throw BinderException("File pattern list cannot be empty");
        }
        for (auto &pattern : pattern_list) {
            if (pattern.IsNull()) {
                throw BinderException("File pattern list cannot contain NULL values");
            }
            file_patterns.push_back(pattern.ToString());
        }
    } else {
        throw BinderException("File patterns must be VARCHAR or LIST(VARCHAR)");
    }
    
    // Check for duplicate parameters (following DuckDB YAML extension pattern)
    std::unordered_set<std::string> seen_parameters;
    for (auto &param : input.named_parameters) {
        if (seen_parameters.find(param.first) != seen_parameters.end()) {
            throw BinderException("Duplicate parameter name: " + param.first);
        }
        seen_parameters.insert(param.first);
    }
    
    // Parse optional named parameters
    bool ignore_errors = false;
    if (seen_parameters.find("ignore_errors") != seen_parameters.end()) {
        ignore_errors = input.named_parameters.at("ignore_errors").GetValue<bool>();
    }
    
    // Parse extraction config parameters
    string context_str = "native";  // Default to native - memory issues fixed with flat schema
    if (seen_parameters.find("context") != seen_parameters.end()) {
        context_str = input.named_parameters.at("context").GetValue<string>();
    }
    
    string source_str = "lines";  // Default
    if (seen_parameters.find("source") != seen_parameters.end()) {
        source_str = input.named_parameters.at("source").GetValue<string>();
    }
    
    string structure_str = "full";  // Default
    if (seen_parameters.find("structure") != seen_parameters.end()) {
        structure_str = input.named_parameters.at("structure").GetValue<string>();
    }
    
    // Parse unified peek parameter (can be INTEGER or VARCHAR)
    int32_t peek_size = 120;
    string peek_mode = "smart";
    if (seen_parameters.find("peek") != seen_parameters.end()) {
        auto& peek_value = input.named_parameters.at("peek");
        if (peek_value.type().id() == LogicalTypeId::INTEGER || 
            peek_value.type().id() == LogicalTypeId::BIGINT) {
            // INTEGER: custom size
            peek_size = peek_value.GetValue<int32_t>();
            peek_mode = "custom";
        } else {
            // VARCHAR: named mode
            string peek_str = peek_value.GetValue<string>();
            string peek_lower = StringUtil::Lower(peek_str);
            if (peek_lower == "none" || peek_lower == "smart" || peek_lower == "full") {
                peek_mode = peek_lower;
            } else {
                throw BinderException("Invalid peek parameter: " + peek_str + 
                                    ". Must be integer or one of: none, smart, full");
            }
        }
    }
    
    // Legacy parameter support (override if provided)
    if (seen_parameters.find("peek_size") != seen_parameters.end()) {
        peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
    }
    if (seen_parameters.find("peek_mode") != seen_parameters.end()) {
        peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
    }
    
    int32_t batch_size = 1;  // Default = current streaming behavior
    if (seen_parameters.find("batch_size") != seen_parameters.end()) {
        batch_size = input.named_parameters.at("batch_size").GetValue<int32_t>();
        if (batch_size < 1) {
            throw BinderException("batch_size must be positive");
        }
    }
    
    // Create ExtractionConfig from parsed parameters
    ExtractionConfig extraction_config = ParseExtractionConfig(context_str, source_str, structure_str, peek_mode, peek_size);
    
    // Use hierarchical backend schema
    return_types = UnifiedASTBackend::GetHierarchicalTableSchema();
    names = UnifiedASTBackend::GetHierarchicalTableColumnNames();
    
    // Use the new ExtractionConfig constructor 
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, extraction_config, batch_size);
}

// Hierarchical streaming bind function for one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTHierarchicalStreamingBindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                                       vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 1) {
        throw BinderException("read_ast requires exactly 1 argument: file_path");
    }
    
    auto file_path_value = input.inputs[0];
    
    // Handle both VARCHAR and LIST(VARCHAR) inputs (DuckDB-consistent pattern)
    vector<string> file_patterns;
    if (file_path_value.type().id() == LogicalTypeId::VARCHAR) {
        file_patterns.push_back(file_path_value.ToString());
    } else if (file_path_value.type().id() == LogicalTypeId::LIST) {
        auto &pattern_list = ListValue::GetChildren(file_path_value);
        if (pattern_list.empty()) {
            throw BinderException("File pattern list cannot be empty");
        }
        for (auto &pattern : pattern_list) {
            if (pattern.IsNull()) {
                throw BinderException("File pattern list cannot contain NULL values");
            }
            file_patterns.push_back(pattern.ToString());
        }
    } else {
        throw BinderException("File patterns must be VARCHAR or LIST(VARCHAR)");
    }
    
    // Check for duplicate parameters (following DuckDB YAML extension pattern)
    std::unordered_set<std::string> seen_parameters;
    for (auto &param : input.named_parameters) {
        if (seen_parameters.find(param.first) != seen_parameters.end()) {
            throw BinderException("Duplicate parameter name: " + param.first);
        }
        seen_parameters.insert(param.first);
    }
    
    // Parse optional named parameters
    bool ignore_errors = false;
    if (seen_parameters.find("ignore_errors") != seen_parameters.end()) {
        ignore_errors = input.named_parameters.at("ignore_errors").GetValue<bool>();
    }
    
    // Parse extraction config parameters
    string context_str = "native";  // Default to native - memory issues fixed with flat schema
    if (seen_parameters.find("context") != seen_parameters.end()) {
        context_str = input.named_parameters.at("context").GetValue<string>();
    }
    
    string source_str = "lines";  // Default
    if (seen_parameters.find("source") != seen_parameters.end()) {
        source_str = input.named_parameters.at("source").GetValue<string>();
    }
    
    string structure_str = "full";  // Default
    if (seen_parameters.find("structure") != seen_parameters.end()) {
        structure_str = input.named_parameters.at("structure").GetValue<string>();
    }
    
    // Parse unified peek parameter (can be INTEGER or VARCHAR)
    int32_t peek_size = 120;
    string peek_mode = "smart";
    if (seen_parameters.find("peek") != seen_parameters.end()) {
        auto& peek_value = input.named_parameters.at("peek");
        if (peek_value.type().id() == LogicalTypeId::INTEGER || 
            peek_value.type().id() == LogicalTypeId::BIGINT) {
            // INTEGER: custom size
            peek_size = peek_value.GetValue<int32_t>();
            peek_mode = "custom";
        } else {
            // VARCHAR: named mode
            string peek_str = peek_value.GetValue<string>();
            string peek_lower = StringUtil::Lower(peek_str);
            if (peek_lower == "none" || peek_lower == "smart" || peek_lower == "full") {
                peek_mode = peek_lower;
            } else {
                throw BinderException("Invalid peek parameter: " + peek_str + 
                                    ". Must be integer or one of: none, smart, full");
            }
        }
    }
    
    // Legacy parameter support (override if provided)
    if (seen_parameters.find("peek_size") != seen_parameters.end()) {
        peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
    }
    if (seen_parameters.find("peek_mode") != seen_parameters.end()) {
        peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
    }
    
    int32_t batch_size = 1;  // Default = current streaming behavior
    if (seen_parameters.find("batch_size") != seen_parameters.end()) {
        batch_size = input.named_parameters.at("batch_size").GetValue<int32_t>();
        if (batch_size < 1) {
            throw BinderException("batch_size must be positive");
        }
    }
    
    // Use auto-detect for language
    string language = "auto";
    
    // Create ExtractionConfig from parsed parameters
    ExtractionConfig extraction_config = ParseExtractionConfig(context_str, source_str, structure_str, peek_mode, peek_size);
    
    // Use hierarchical backend schema
    return_types = UnifiedASTBackend::GetHierarchicalTableSchema();
    names = UnifiedASTBackend::GetHierarchicalTableColumnNames();
    
    // Use the new ExtractionConfig constructor 
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, extraction_config, batch_size);
}

// Functions for read_ast (using hierarchical STRUCT schema)
static TableFunction GetReadASTFunctionTwoArg() {
    TableFunction read_ast_hierarchical_new("read_ast_hierarchical_new", {LogicalType::ANY, LogicalType::VARCHAR}, 
                                           ReadASTHierarchicalFunction, ReadASTHierarchicalStreamingBindTwoArg, ReadASTStreamingInit);
    read_ast_hierarchical_new.name = "read_ast_hierarchical_new";
    read_ast_hierarchical_new.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_hierarchical_new.named_parameters["context"] = LogicalType::VARCHAR;
    read_ast_hierarchical_new.named_parameters["source"] = LogicalType::VARCHAR;
    read_ast_hierarchical_new.named_parameters["structure"] = LogicalType::VARCHAR;
    read_ast_hierarchical_new.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    read_ast_hierarchical_new.named_parameters["batch_size"] = LogicalType::INTEGER;
    
    // Legacy parameters for backward compatibility
    read_ast_hierarchical_new.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_hierarchical_new.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    return read_ast_hierarchical_new;
}

// Hierarchical functions with STRUCT schema using streaming bind (explicit access)
static TableFunction GetReadASTHierarchicalFunctionTwoArg() {
    TableFunction read_ast_hierarchical("read_ast_hierarchical", {LogicalType::ANY, LogicalType::VARCHAR}, 
                                       ReadASTHierarchicalFunction, ReadASTHierarchicalStreamingBindTwoArg, ReadASTStreamingInit);
    read_ast_hierarchical.name = "read_ast_hierarchical";
    read_ast_hierarchical.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_hierarchical.named_parameters["context"] = LogicalType::VARCHAR;
    read_ast_hierarchical.named_parameters["source"] = LogicalType::VARCHAR;
    read_ast_hierarchical.named_parameters["structure"] = LogicalType::VARCHAR;
    read_ast_hierarchical.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    read_ast_hierarchical.named_parameters["batch_size"] = LogicalType::INTEGER;
    
    // Legacy parameters for backward compatibility
    read_ast_hierarchical.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_hierarchical.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    return read_ast_hierarchical;
}

static TableFunction GetReadASTFunctionOneArg() {
    TableFunction read_ast_hierarchical_new("read_ast_hierarchical_new", {LogicalType::ANY}, 
                                           ReadASTHierarchicalFunction, ReadASTHierarchicalStreamingBindOneArg, ReadASTStreamingInit);
    read_ast_hierarchical_new.name = "read_ast_hierarchical_new";
    read_ast_hierarchical_new.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_hierarchical_new.named_parameters["context"] = LogicalType::VARCHAR;
    read_ast_hierarchical_new.named_parameters["source"] = LogicalType::VARCHAR;
    read_ast_hierarchical_new.named_parameters["structure"] = LogicalType::VARCHAR;
    read_ast_hierarchical_new.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    read_ast_hierarchical_new.named_parameters["batch_size"] = LogicalType::INTEGER;
    
    // Legacy parameters for backward compatibility
    read_ast_hierarchical_new.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_hierarchical_new.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    return read_ast_hierarchical_new;
}

static TableFunction GetReadASTHierarchicalFunctionOneArg() {
    TableFunction read_ast_hierarchical("read_ast_hierarchical", {LogicalType::ANY}, 
                                       ReadASTHierarchicalFunction, ReadASTHierarchicalStreamingBindOneArg, ReadASTStreamingInit);
    read_ast_hierarchical.name = "read_ast_hierarchical";
    read_ast_hierarchical.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_hierarchical.named_parameters["context"] = LogicalType::VARCHAR;
    read_ast_hierarchical.named_parameters["source"] = LogicalType::VARCHAR;
    read_ast_hierarchical.named_parameters["structure"] = LogicalType::VARCHAR;
    read_ast_hierarchical.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    read_ast_hierarchical.named_parameters["batch_size"] = LogicalType::INTEGER;
    
    // Legacy parameters for backward compatibility
    read_ast_hierarchical.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_hierarchical.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    return read_ast_hierarchical;
}

// Create read_ast_flat alias functions (explicit access to flat schema)
static TableFunction GetReadASTFlatAliasFunctionOneArg() {
    TableFunction read_ast_flat("read_ast_flat", {LogicalType::ANY}, 
                               ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindOneArg, ReadASTStreamingInit);
    read_ast_flat.name = "read_ast_flat";
    read_ast_flat.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_flat.named_parameters["context"] = LogicalType::VARCHAR;
    read_ast_flat.named_parameters["source"] = LogicalType::VARCHAR;
    read_ast_flat.named_parameters["structure"] = LogicalType::VARCHAR;
    read_ast_flat.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    read_ast_flat.named_parameters["batch_size"] = LogicalType::INTEGER;
    
    // Legacy parameters for backward compatibility
    read_ast_flat.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_flat.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    return read_ast_flat;
}

static TableFunction GetReadASTFlatAliasFunctionTwoArg() {
    TableFunction read_ast_flat("read_ast_flat", {LogicalType::ANY, LogicalType::VARCHAR}, 
                               ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindTwoArg, ReadASTStreamingInit);
    read_ast_flat.name = "read_ast_flat";
    read_ast_flat.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_flat.named_parameters["context"] = LogicalType::VARCHAR;
    read_ast_flat.named_parameters["source"] = LogicalType::VARCHAR;
    read_ast_flat.named_parameters["structure"] = LogicalType::VARCHAR;
    read_ast_flat.named_parameters["peek"] = LogicalType::ANY;  // Can be INTEGER or VARCHAR
    read_ast_flat.named_parameters["batch_size"] = LogicalType::INTEGER;
    
    // Legacy parameters for backward compatibility
    read_ast_flat.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_flat.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    return read_ast_flat;
}

void RegisterReadASTFunction(ExtensionLoader &loader) {
    // Register default read_ast functions (now using flat schema - production ready)
    loader.RegisterFunction(GetReadASTFlatFunctionOneArg());    // ANY (auto-detect)
    loader.RegisterFunction(GetReadASTFlatFunctionTwoArg());    // ANY, VARCHAR (explicit language)
    
    // Register read_ast_flat aliases (explicit access to flat schema)
    loader.RegisterFunction(GetReadASTFlatAliasFunctionOneArg());    // ANY (auto-detect)
    loader.RegisterFunction(GetReadASTFlatAliasFunctionTwoArg());    // ANY, VARCHAR (explicit language)
    
    // Register read_ast_hierarchical_new functions (hierarchical STRUCT schema with memory issues)
    loader.RegisterFunction(GetReadASTFunctionOneArg());    // ANY (auto-detect)
    loader.RegisterFunction(GetReadASTFunctionTwoArg());    // ANY, VARCHAR (explicit language)
    
    // Register read_ast_hierarchical functions for backward compatibility
    loader.RegisterFunction(GetReadASTHierarchicalFunctionOneArg());    // ANY (auto-detect)
    loader.RegisterFunction(GetReadASTHierarchicalFunctionTwoArg());    // ANY, VARCHAR (explicit language)
}

void RegisterReadASTStreamingFunction(ExtensionLoader &loader) {
    // Register explicit streaming functions for comparison
    loader.RegisterFunction(GetReadASTStreamingFunctionOneArg());
    loader.RegisterFunction(GetReadASTStreamingFunctionTwoArg());
}

} // namespace duckdb