#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"
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
    
    int32_t peek_size = 120;  // Default 120 characters
    if (seen_parameters.find("peek_size") != seen_parameters.end()) {
        peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
    }
    
    string peek_mode = "auto";  // Default auto mode
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
    
    // Use unified backend schema
    return_types = UnifiedASTBackend::GetFlatTableSchema();
    names = UnifiedASTBackend::GetFlatTableColumnNames();
    
    // Use the new vector<string> constructor for consistent handling
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, peek_size, peek_mode, batch_size);
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
    
    int32_t peek_size = 120;  // Default 120 characters
    if (seen_parameters.find("peek_size") != seen_parameters.end()) {
        peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
    }
    
    string peek_mode = "auto";  // Default auto mode
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
    
    // Use unified backend schema
    return_types = UnifiedASTBackend::GetFlatTableSchema();
    names = UnifiedASTBackend::GetFlatTableColumnNames();
    
    // Use the new vector<string> constructor for consistent handling
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, peek_size, peek_mode, batch_size);
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
            
            // PRE-CREATE ALL NEEDED ADAPTERS (eliminates singleton contention)
            auto& registry = LanguageAdapterRegistry::GetInstance();
            for (const auto& language : unique_languages) {
                auto adapter = registry.CreateAdapter(language);
                if (adapter) {
                    result->pre_created_adapters[language] = std::move(adapter);
                } else if (!bind_data.ignore_errors) {
                    throw InvalidInputException("Unsupported language: " + language);
                }
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
            
            // Use ParseSingleFileToASTResult for consistent error handling with sequential path
            auto result_ptr = UnifiedASTBackend::ParseSingleFileToASTResult(
                context, file_path, file_language, global_state.ignore_errors,
                global_state.peek_size, global_state.peek_mode
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
    idx_t output_count = 0;
    
    // Get output vectors once for efficiency
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
    auto semantic_type_vec = FlatVector::GetData<uint8_t>(output.data[15]);
    auto universal_flags_vec = FlatVector::GetData<uint8_t>(output.data[16]);
    auto arity_bin_vec = FlatVector::GetData<uint8_t>(output.data[17]);
    
    // Get validity masks
    auto &name_validity = FlatVector::Validity(output.data[2]);
    auto &parent_validity = FlatVector::Validity(output.data[9]);
    auto &peek_validity = FlatVector::Validity(output.data[14]);
    
    while (output_count < STANDARD_VECTOR_SIZE) {
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
            
            // Output nodes from current batch results
            while (output_count < STANDARD_VECTOR_SIZE && 
                   global_state.current_batch_result_index < global_state.current_batch_results.size()) {
                
                auto& result = global_state.current_batch_results[global_state.current_batch_result_index];
                
                if (global_state.current_batch_row_index < result.nodes.size()) {
                    auto& node = result.nodes[global_state.current_batch_row_index];
                    
                    // Populate output vectors (existing logic)
                    node_id_vec[output_count] = node.node_id;
                    type_vec[output_count] = StringVector::AddString(output.data[1], node.type.raw);
                    
                    if (!node.name.raw.empty()) {
                        name_vec[output_count] = StringVector::AddString(output.data[2], node.name.raw);
                    } else {
                        name_validity.SetInvalid(output_count);
                    }
                    
                    file_path_vec[output_count] = StringVector::AddString(output.data[3], result.source.file_path);
                    language_vec[output_count] = StringVector::AddString(output.data[4], result.source.language);
                    start_line_vec[output_count] = node.file_position.start_line;
                    start_column_vec[output_count] = node.file_position.start_column;
                    end_line_vec[output_count] = node.file_position.end_line;
                    end_column_vec[output_count] = node.file_position.end_column;
                    
                    if (node.tree_position.parent_index != -1) {
                        parent_id_vec[output_count] = node.tree_position.parent_index;
                    } else {
                        parent_validity.SetInvalid(output_count);
                    }
                    
                    depth_vec[output_count] = node.tree_position.node_depth;
                    sibling_index_vec[output_count] = node.tree_position.sibling_index;
                    children_count_vec[output_count] = node.subtree.children_count;
                    descendant_count_vec[output_count] = node.subtree.descendant_count;
                    
                    if (!node.peek.empty()) {
                        peek_vec[output_count] = StringVector::AddString(output.data[14], node.peek);
                    } else {
                        peek_validity.SetInvalid(output_count);
                    }
                    
                    semantic_type_vec[output_count] = node.semantic_type;
                    universal_flags_vec[output_count] = node.universal_flags;
                    arity_bin_vec[output_count] = node.arity_bin;
                    
                    output_count++;
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
            
            // Parse this single file
            global_state.current_file_result = UnifiedASTBackend::ParseSingleFileToASTResult(
                context, file.path, global_state.language, global_state.ignore_errors, 
                global_state.peek_size, global_state.peek_mode);
            
            if (!global_state.current_file_result) {
                // File was skipped due to errors, continue to next file
                continue;
            }
            
            global_state.current_file_row_index = 0;
            global_state.current_file_parsed = true;
        }
        
        // Emit rows from current file
        if (!global_state.current_file_result) {
            // This shouldn't happen, but add safety check
            break;
        }
        
        idx_t rows_available = global_state.current_file_result->nodes.size() - global_state.current_file_row_index;
        idx_t rows_to_emit = std::min(STANDARD_VECTOR_SIZE - output_count, rows_available);
        
        // Copy rows directly into output vectors
        for (idx_t i = 0; i < rows_to_emit; i++) {
            const auto& node = global_state.current_file_result->nodes[global_state.current_file_row_index + i];
            idx_t output_idx = output_count + i;
            
            // Fill output vectors directly (same logic as original function)
            node_id_vec[output_idx] = node.node_id;
            type_vec[output_idx] = StringVector::AddString(output.data[1], node.type.raw);
            
            if (node.name.raw.empty()) {
                name_validity.SetInvalid(output_idx);
            } else {
                name_vec[output_idx] = StringVector::AddString(output.data[2], node.name.raw);
            }
            
            file_path_vec[output_idx] = StringVector::AddString(output.data[3], global_state.current_file_result->source.file_path);
            language_vec[output_idx] = StringVector::AddString(output.data[4], global_state.current_file_result->source.language);
            start_line_vec[output_idx] = node.file_position.start_line;
            start_column_vec[output_idx] = node.file_position.start_column;
            end_line_vec[output_idx] = node.file_position.end_line;
            end_column_vec[output_idx] = node.file_position.end_column;
            
            if (node.tree_position.parent_index < 0) {
                parent_validity.SetInvalid(output_idx);
            } else {
                parent_id_vec[output_idx] = node.tree_position.parent_index;
            }
            
            depth_vec[output_idx] = node.tree_position.node_depth;
            sibling_index_vec[output_idx] = node.tree_position.sibling_index;
            children_count_vec[output_idx] = node.subtree.children_count;
            descendant_count_vec[output_idx] = node.subtree.descendant_count;
            
            // Handle peek with NULL support
            if (node.peek.empty()) {
                peek_validity.SetInvalid(output_idx);
            } else {
                peek_vec[output_idx] = StringVector::AddString(output.data[14], node.peek);
            }
            
            semantic_type_vec[output_idx] = node.semantic_type;
            universal_flags_vec[output_idx] = node.universal_flags;
            arity_bin_vec[output_idx] = node.arity_bin;
        }
        
        global_state.current_file_row_index += rows_to_emit;
        output_count += rows_to_emit;
    }
    
    output.SetCardinality(output_count);
}

// PARALLEL PROCESSING without batching - let DuckDB handle task distribution
static void ReadASTFlatStreamingFunctionParallel(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output) {
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
    
    // Now just stream the results
    idx_t output_count = 0;
    
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
    auto semantic_type_vec = FlatVector::GetData<uint8_t>(output.data[15]);
    auto universal_flags_vec = FlatVector::GetData<uint8_t>(output.data[16]);
    auto arity_bin_vec = FlatVector::GetData<uint8_t>(output.data[17]);
    
    // Get validity masks
    auto &name_validity = FlatVector::Validity(output.data[2]);
    auto &parent_validity = FlatVector::Validity(output.data[9]);
    auto &peek_validity = FlatVector::Validity(output.data[14]);

    // Stream results from all completed parsing
    while (output_count < STANDARD_VECTOR_SIZE && 
           global_state.current_batch_result_index < global_state.current_batch_results.size()) {
        
        const auto& current_result = global_state.current_batch_results[global_state.current_batch_result_index];
        
        // Check if we've exhausted this result
        if (global_state.current_batch_row_index >= current_result.nodes.size()) {
            global_state.current_batch_result_index++;
            global_state.current_batch_row_index = 0;
            continue;
        }
        
        // Calculate how many rows to emit from this result
        const idx_t rows_available = current_result.nodes.size() - global_state.current_batch_row_index;
        const idx_t rows_to_emit = std::min(STANDARD_VECTOR_SIZE - output_count, rows_available);
        
        // Copy rows directly into output vectors
        for (idx_t i = 0; i < rows_to_emit; i++) {
            const auto& node = current_result.nodes[global_state.current_batch_row_index + i];
            const idx_t output_idx = output_count + i;
            
            // Fill output vectors
            node_id_vec[output_idx] = node.node_id;
            type_vec[output_idx] = StringVector::AddString(output.data[1], node.type.raw);
            
            if (node.name.raw.empty()) {
                name_validity.SetInvalid(output_idx);
            } else {
                name_vec[output_idx] = StringVector::AddString(output.data[2], node.name.raw);
            }
            
            file_path_vec[output_idx] = StringVector::AddString(output.data[3], current_result.source.file_path);
            language_vec[output_idx] = StringVector::AddString(output.data[4], current_result.source.language);
            start_line_vec[output_idx] = node.file_position.start_line;
            start_column_vec[output_idx] = node.file_position.start_column;
            end_line_vec[output_idx] = node.file_position.end_line;
            end_column_vec[output_idx] = node.file_position.end_column;
            
            if (node.tree_position.parent_index < 0) {
                parent_validity.SetInvalid(output_idx);
            } else {
                parent_id_vec[output_idx] = node.tree_position.parent_index;
            }
            
            depth_vec[output_idx] = node.tree_position.node_depth;
            sibling_index_vec[output_idx] = node.tree_position.sibling_index;
            children_count_vec[output_idx] = node.subtree.children_count;
            descendant_count_vec[output_idx] = node.subtree.descendant_count;
            
            // Handle peek with NULL support
            if (node.peek.empty()) {
                peek_validity.SetInvalid(output_idx);
            } else {
                peek_vec[output_idx] = StringVector::AddString(output.data[14], node.peek);
            }
            
            semantic_type_vec[output_idx] = node.semantic_type;
            universal_flags_vec[output_idx] = node.universal_flags;
            arity_bin_vec[output_idx] = node.arity_bin;
        }
        
        global_state.current_batch_row_index += rows_to_emit;
        output_count += rows_to_emit;
    }
    
    // Mark as exhausted when all results have been streamed
    if (global_state.current_batch_result_index >= global_state.current_batch_results.size()) {
        global_state.files_exhausted = true;
    }
    
    output.SetCardinality(output_count);
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
    
    // Temporarily use flat schema until STRUCT functions are properly implemented
    return_types = UnifiedASTBackend::GetFlatTableSchema();
    names = UnifiedASTBackend::GetFlatTableColumnNames();
    
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
    
    // Temporarily use flat schema until STRUCT functions are properly implemented
    return_types = UnifiedASTBackend::GetFlatTableSchema();
    names = UnifiedASTBackend::GetFlatTableColumnNames();
    
    return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, peek_size, peek_mode, batch_size);
}

// Hierarchical streaming functions - use NEW structured fields and hierarchical layout
static void ReadASTHierarchicalFunctionSequential(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output) {
    idx_t output_count = 0;
    
    // Get output vectors for FLAT schema (currently bound to flat schema)
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
    auto semantic_type_vec = FlatVector::GetData<uint8_t>(output.data[15]);
    auto universal_flags_vec = FlatVector::GetData<uint8_t>(output.data[16]);
    auto arity_bin_vec = FlatVector::GetData<uint8_t>(output.data[17]);
    
    // Get validity masks for flat schema
    auto &name_validity = FlatVector::Validity(output.data[2]);
    auto &parent_validity = FlatVector::Validity(output.data[9]);
    auto &peek_validity = FlatVector::Validity(output.data[14]);
    
    while (output_count < STANDARD_VECTOR_SIZE) {
        // Handle batch processing if enabled
        if (global_state.batch_size > 1) {
            // [Same batch logic as original, but using NEW structured fields]
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
            
            // Process current batch using NEW structured fields
            if (global_state.current_batch_result_index < global_state.current_batch_results.size()) {
                const auto& result = global_state.current_batch_results[global_state.current_batch_result_index];
                
                if (global_state.current_batch_row_index < result.nodes.size()) {
                    auto& node = result.nodes[global_state.current_batch_row_index];
                    
                    // HIERARCHICAL PROJECTION - use NEW structured fields
                    node_id_vec[output_count] = node.node_id;
                    
                    // Source Location group - use legacy fields for now
                    file_path_vec[output_count] = StringVector::AddString(output.data[1], result.source.file_path);
                    language_vec[output_count] = StringVector::AddString(output.data[2], result.source.language);
                    start_line_vec[output_count] = node.file_position.start_line;
                    start_column_vec[output_count] = node.file_position.start_column;
                    end_line_vec[output_count] = node.file_position.end_line;
                    end_column_vec[output_count] = node.file_position.end_column;
                    
                    // Tree Structure group - use legacy fields for now
                    if (node.tree_position.parent_index < 0) {
                        parent_validity.SetInvalid(output_count);
                    } else {
                        parent_id_vec[output_count] = node.tree_position.parent_index;
                    }
                    depth_vec[output_count] = node.tree_position.node_depth;
                    sibling_index_vec[output_count] = node.tree_position.sibling_index;
                    children_count_vec[output_count] = node.subtree.children_count;
                    descendant_count_vec[output_count] = node.subtree.descendant_count;
                    
                    // Context Information group - use legacy fields for now
                    type_vec[output_count] = StringVector::AddString(output.data[12], node.type.raw);
                    if (node.name.raw.empty()) {
                        name_validity.SetInvalid(output_count);
                    } else {
                        name_vec[output_count] = StringVector::AddString(output.data[13], node.name.raw);
                    }
                    semantic_type_vec[output_count] = node.semantic_type;
                    universal_flags_vec[output_count] = node.universal_flags;
                    arity_bin_vec[output_count] = node.arity_bin;
                    
                    // Content Preview group
                    if (!node.peek.empty()) {
                        peek_vec[output_count] = StringVector::AddString(output.data[17], node.peek);
                    } else {
                        peek_validity.SetInvalid(output_count);
                    }
                    
                    output_count++;
                    global_state.current_batch_row_index++;
                } else {
                    global_state.current_batch_result_index++;
                    global_state.current_batch_row_index = 0;
                }
            } else {
                break;
            }
        } else {
            // Single file processing using NEW structured fields - MATCH FLAT VERSION LOGIC
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
                
                // Parse this single file
                global_state.current_file_result = UnifiedASTBackend::ParseSingleFileToASTResult(
                    context, file.path, global_state.language, global_state.ignore_errors, 
                    global_state.peek_size, global_state.peek_mode);
                
                if (!global_state.current_file_result) {
                    // File was skipped due to errors, continue to next file
                    continue;
                }
                
                global_state.current_file_row_index = 0;
                global_state.current_file_parsed = true;
            }
            
            // Emit rows from current file
            if (!global_state.current_file_result) {
                // This shouldn't happen, but add safety check
                break;
            }
            
            // Stream from current file using NEW structured fields
            idx_t rows_available = global_state.current_file_result->nodes.size() - global_state.current_file_row_index;
            idx_t rows_to_emit = std::min(STANDARD_VECTOR_SIZE - output_count, rows_available);
            
            for (idx_t i = 0; i < rows_to_emit; i++) {
                const auto& node = global_state.current_file_result->nodes[global_state.current_file_row_index + i];
                idx_t output_idx = output_count + i;
                
                // HIERARCHICAL PROJECTION - use NEW structured fields
                node_id_vec[output_idx] = node.node_id;
                
                // Source Location group - use legacy fields for now
                file_path_vec[output_idx] = StringVector::AddString(output.data[1], global_state.current_file_result->source.file_path);
                language_vec[output_idx] = StringVector::AddString(output.data[2], global_state.current_file_result->source.language);
                start_line_vec[output_idx] = node.file_position.start_line;
                start_column_vec[output_idx] = node.file_position.start_column;
                end_line_vec[output_idx] = node.file_position.end_line;
                end_column_vec[output_idx] = node.file_position.end_column;
                
                // Tree Structure group - use legacy fields for now
                if (node.tree_position.parent_index < 0) {
                    parent_validity.SetInvalid(output_idx);
                } else {
                    parent_id_vec[output_idx] = node.tree_position.parent_index;
                }
                depth_vec[output_idx] = node.tree_position.node_depth;
                sibling_index_vec[output_idx] = node.tree_position.sibling_index;
                children_count_vec[output_idx] = node.subtree.children_count;
                descendant_count_vec[output_idx] = node.subtree.descendant_count;
                
                // Context Information group - use legacy fields for now
                type_vec[output_idx] = StringVector::AddString(output.data[12], node.type.raw);
                if (node.name.raw.empty()) {
                    name_validity.SetInvalid(output_idx);
                } else {
                    name_vec[output_idx] = StringVector::AddString(output.data[13], node.name.raw);
                }
                semantic_type_vec[output_idx] = node.semantic_type;
                universal_flags_vec[output_idx] = node.universal_flags;
                arity_bin_vec[output_idx] = node.arity_bin;
                
                // Content Preview group
                if (node.peek.empty()) {
                    peek_validity.SetInvalid(output_idx);
                } else {
                    peek_vec[output_idx] = StringVector::AddString(output.data[17], node.peek);
                }
            }
            
            global_state.current_file_row_index += rows_to_emit;
            output_count += rows_to_emit;
        }
    }
    
    output.SetCardinality(output_count);
}

static void ReadASTHierarchicalFunctionParallel(ClientContext &context, ReadASTStreamingGlobalState &global_state, DataChunk &output) {
    // Check if we need to do the one-time parallel processing - MATCH FLAT VERSION
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
    
    // Now stream the results using HIERARCHICAL schema and NEW structured fields
    idx_t output_count = 0;
    
    // Get output vectors for hierarchical schema
    auto node_id_vec = FlatVector::GetData<int64_t>(output.data[0]);
    
    // Source Location group (indices 1-6)
    auto file_path_vec = FlatVector::GetData<string_t>(output.data[1]);
    auto language_vec = FlatVector::GetData<string_t>(output.data[2]);
    auto start_line_vec = FlatVector::GetData<uint32_t>(output.data[3]);
    auto start_column_vec = FlatVector::GetData<uint32_t>(output.data[4]);
    auto end_line_vec = FlatVector::GetData<uint32_t>(output.data[5]);
    auto end_column_vec = FlatVector::GetData<uint32_t>(output.data[6]);
    
    // Tree Structure group (indices 7-11)
    auto parent_id_vec = FlatVector::GetData<int64_t>(output.data[7]);
    auto depth_vec = FlatVector::GetData<uint32_t>(output.data[8]);
    auto sibling_index_vec = FlatVector::GetData<uint32_t>(output.data[9]);
    auto children_count_vec = FlatVector::GetData<uint32_t>(output.data[10]);
    auto descendant_count_vec = FlatVector::GetData<uint32_t>(output.data[11]);
    
    // Context Information group (indices 12-16)
    auto type_vec = FlatVector::GetData<string_t>(output.data[12]);
    auto name_vec = FlatVector::GetData<string_t>(output.data[13]);
    auto semantic_type_vec = FlatVector::GetData<uint8_t>(output.data[14]);
    auto universal_flags_vec = FlatVector::GetData<uint8_t>(output.data[15]);
    auto arity_bin_vec = FlatVector::GetData<uint8_t>(output.data[16]);
    
    // Content Preview group (index 17)
    auto peek_vec = FlatVector::GetData<string_t>(output.data[17]);
    
    // Get validity masks
    auto &name_validity = FlatVector::Validity(output.data[13]);
    auto &parent_validity = FlatVector::Validity(output.data[7]);
    auto &peek_validity = FlatVector::Validity(output.data[17]);

    // Stream results from all completed parsing using NEW structured fields
    while (output_count < STANDARD_VECTOR_SIZE && 
           global_state.current_batch_result_index < global_state.current_batch_results.size()) {
        
        const auto& current_result = global_state.current_batch_results[global_state.current_batch_result_index];
        
        if (global_state.current_batch_row_index >= current_result.nodes.size()) {
            global_state.current_batch_result_index++;
            global_state.current_batch_row_index = 0;
            continue;
        }
        
        idx_t remaining_capacity = STANDARD_VECTOR_SIZE - output_count;
        idx_t remaining_rows = current_result.nodes.size() - global_state.current_batch_row_index;
        idx_t rows_to_emit = std::min(remaining_capacity, remaining_rows);
        
        for (idx_t i = 0; i < rows_to_emit; i++) {
            const auto& node = current_result.nodes[global_state.current_batch_row_index + i];
            const idx_t output_idx = output_count + i;
            
            // HIERARCHICAL PROJECTION - use NEW structured fields
            node_id_vec[output_idx] = node.node_id;
            
            // Source Location group - use legacy fields for now
            file_path_vec[output_idx] = StringVector::AddString(output.data[1], current_result.source.file_path);
            language_vec[output_idx] = StringVector::AddString(output.data[2], current_result.source.language);
            start_line_vec[output_idx] = node.file_position.start_line;
            start_column_vec[output_idx] = node.file_position.start_column;
            end_line_vec[output_idx] = node.file_position.end_line;
            end_column_vec[output_idx] = node.file_position.end_column;
            
            // Tree Structure group - use legacy fields for now
            if (node.tree_position.parent_index < 0) {
                parent_validity.SetInvalid(output_idx);
            } else {
                parent_id_vec[output_idx] = node.tree_position.parent_index;
            }
            depth_vec[output_idx] = node.tree_position.node_depth;
            sibling_index_vec[output_idx] = node.tree_position.sibling_index;
            children_count_vec[output_idx] = node.subtree.children_count;
            descendant_count_vec[output_idx] = node.subtree.descendant_count;
            
            // Context Information group - use legacy fields for now
            type_vec[output_idx] = StringVector::AddString(output.data[12], node.type.raw);
            if (node.name.raw.empty()) {
                name_validity.SetInvalid(output_idx);
            } else {
                name_vec[output_idx] = StringVector::AddString(output.data[13], node.name.raw);
            }
            semantic_type_vec[output_idx] = node.semantic_type;
            universal_flags_vec[output_idx] = node.universal_flags;
            arity_bin_vec[output_idx] = node.arity_bin;
            
            // Content Preview group
            if (node.peek.empty()) {
                peek_validity.SetInvalid(output_idx);
            } else {
                peek_vec[output_idx] = StringVector::AddString(output.data[17], node.peek);
            }
        }
        
        global_state.current_batch_row_index += rows_to_emit;
        output_count += rows_to_emit;
    }
    
    if (global_state.current_batch_result_index >= global_state.current_batch_results.size()) {
        global_state.files_exhausted = true;
    }
    
    output.SetCardinality(output_count);
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
    TableFunction read_ast_flat("read_ast_flat", {LogicalType::ANY, LogicalType::VARCHAR}, 
                               ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindTwoArg, ReadASTStreamingInit);
    read_ast_flat.name = "read_ast_flat";
    read_ast_flat.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_flat.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_flat.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    read_ast_flat.named_parameters["batch_size"] = LogicalType::INTEGER;
    return read_ast_flat;
}

static TableFunction GetReadASTFunctionTwoArg() {
    TableFunction read_ast("read_ast", {LogicalType::ANY, LogicalType::VARCHAR}, 
                          ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindTwoArg, ReadASTStreamingInit);
    read_ast.name = "read_ast";
    read_ast.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    read_ast.named_parameters["batch_size"] = LogicalType::INTEGER;
    return read_ast;
}

// Legacy flat schema functions
static TableFunction GetReadASTFlatFunctionOneArg() {
    TableFunction read_ast_flat("read_ast_flat", {LogicalType::ANY}, 
                               ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindOneArg, ReadASTStreamingInit);
    read_ast_flat.name = "read_ast_flat";
    read_ast_flat.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast_flat.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast_flat.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    read_ast_flat.named_parameters["batch_size"] = LogicalType::INTEGER;
    return read_ast_flat;
}

// Default read_ast functions (currently identical to flat - will become hierarchical later)
static TableFunction GetReadASTFunctionOneArg() {
    TableFunction read_ast("read_ast", {LogicalType::ANY}, 
                          ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindOneArg, ReadASTStreamingInit);
    read_ast.name = "read_ast";
    read_ast.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    read_ast.named_parameters["peek_size"] = LogicalType::INTEGER;
    read_ast.named_parameters["peek_mode"] = LogicalType::VARCHAR;
    read_ast.named_parameters["batch_size"] = LogicalType::INTEGER;
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

void RegisterReadASTFunction(DatabaseInstance &instance) {
    // Register flat schema functions (explicit legacy access)
    ExtensionUtil::RegisterFunction(instance, GetReadASTFlatFunctionOneArg());    // ANY (auto-detect)
    ExtensionUtil::RegisterFunction(instance, GetReadASTFlatFunctionTwoArg());    // ANY, VARCHAR (explicit language)
    
    // Register default read_ast functions (currently identical to flat - will become hierarchical STRUCT later)
    ExtensionUtil::RegisterFunction(instance, GetReadASTFunctionOneArg());    // ANY (auto-detect)
    ExtensionUtil::RegisterFunction(instance, GetReadASTFunctionTwoArg());    // ANY, VARCHAR (explicit language)
}

void RegisterReadASTStreamingFunction(DatabaseInstance &instance) {
    // Register explicit streaming functions for comparison
    ExtensionUtil::RegisterFunction(instance, GetReadASTStreamingFunctionOneArg());
    ExtensionUtil::RegisterFunction(instance, GetReadASTStreamingFunctionTwoArg());
}

} // namespace duckdb