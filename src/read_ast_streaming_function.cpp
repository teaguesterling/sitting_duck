#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "unified_ast_backend.hpp"
#include "ast_file_utils.hpp"
#include "read_ast_streaming_state.hpp"
#include "language_adapter.hpp"
#include <unordered_set>

namespace duckdb {
// Bind function for flat streaming two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTFlatStreamingBindTwoArg(ClientContext &context, TableFunctionBindInput &input,
                                                               vector<LogicalType> &return_types,
                                                               vector<string> &names) {
	if (input.inputs.size() != 2) {
		throw BinderException("read_ast requires exactly 2 arguments: file_path and language");
	}

	auto file_path_value = input.inputs[0];

	// If language is NULL, treat as auto-detect
	string language = "auto";
	if (!input.inputs[1].IsNull()) {
		language = input.inputs[1].GetValue<string>();
	}

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
	string context_str = "native"; // Default to native - memory issues fixed with flat schema
	if (seen_parameters.find("context") != seen_parameters.end()) {
		context_str = input.named_parameters.at("context").GetValue<string>();
	}

	string source_str = "lines"; // Default
	if (seen_parameters.find("source") != seen_parameters.end()) {
		source_str = input.named_parameters.at("source").GetValue<string>();
	}

	string structure_str = "full"; // Default
	if (seen_parameters.find("structure") != seen_parameters.end()) {
		structure_str = input.named_parameters.at("structure").GetValue<string>();
	}

	// Parse unified peek parameter (can be INTEGER or VARCHAR)
	// Validation is deferred to ParseExtractionConfig which handles +schema suffix
	int32_t peek_size = 120;
	string peek_mode = "smart";
	if (seen_parameters.find("peek") != seen_parameters.end()) {
		auto &peek_value = input.named_parameters.at("peek");
		if (peek_value.type().id() == LogicalTypeId::INTEGER || peek_value.type().id() == LogicalTypeId::BIGINT) {
			// INTEGER: custom size
			peek_size = peek_value.GetValue<int32_t>();
			peek_mode = "custom";
		} else {
			// VARCHAR: pass through to ParseExtractionConfig for validation (supports +schema suffix)
			peek_mode = peek_value.GetValue<string>();
		}
	}

	// Legacy parameter support (override if provided)
	if (seen_parameters.find("peek_size") != seen_parameters.end()) {
		peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
	}
	if (seen_parameters.find("peek_mode") != seen_parameters.end()) {
		peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
	}

	int32_t batch_size = 1; // Default = current streaming behavior
	if (seen_parameters.find("batch_size") != seen_parameters.end()) {
		batch_size = input.named_parameters.at("batch_size").GetValue<int32_t>();
		if (batch_size < 1) {
			throw BinderException("batch_size must be positive");
		}
	}

	// Create ExtractionConfig from parsed parameters
	ExtractionConfig extraction_config =
	    ParseExtractionConfig(context_str, source_str, structure_str, peek_mode, peek_size);

	// Parse max_depth parameter (#014)
	if (seen_parameters.find("max_depth") != seen_parameters.end()) {
		extraction_config.max_depth = input.named_parameters.at("max_depth").GetValue<int32_t>();
	}

	// Parse prune parameter (#014)
	if (seen_parameters.find("prune") != seen_parameters.end()) {
		auto &prune_value = input.named_parameters.at("prune");
		auto &policy_list = ListValue::GetChildren(prune_value);
		for (auto &policy : policy_list) {
			if (policy.IsNull()) {
				throw BinderException("Prune policy list cannot contain NULL values");
			}
			CompilePrunePolicy(StringUtil::Lower(policy.ToString()), extraction_config);
		}
	}

	// Parse resource cap parameters (max_source_bytes, parse_timeout_ms, max_parse_nodes)
	ParseResourceCapParameters(input.named_parameters, extraction_config);

	// Use flat dynamic schema based on extraction config
	return_types = UnifiedASTBackend::GetFlatDynamicTableSchema(extraction_config);
	names = UnifiedASTBackend::GetFlatDynamicTableColumnNames(extraction_config);

	// Use the new ExtractionConfig constructor
	return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, extraction_config, batch_size);
}

// Bind function for flat streaming one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTFlatStreamingBindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                               vector<LogicalType> &return_types,
                                                               vector<string> &names) {
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
	string context_str = "native"; // Default to native - memory issues fixed with flat schema
	if (seen_parameters.find("context") != seen_parameters.end()) {
		context_str = input.named_parameters.at("context").GetValue<string>();
	}

	string source_str = "lines"; // Default
	if (seen_parameters.find("source") != seen_parameters.end()) {
		source_str = input.named_parameters.at("source").GetValue<string>();
	}

	string structure_str = "full"; // Default
	if (seen_parameters.find("structure") != seen_parameters.end()) {
		structure_str = input.named_parameters.at("structure").GetValue<string>();
	}

	// Parse unified peek parameter (can be INTEGER or VARCHAR)
	// Validation is deferred to ParseExtractionConfig which handles +schema suffix
	int32_t peek_size = 120;
	string peek_mode = "smart";
	if (seen_parameters.find("peek") != seen_parameters.end()) {
		auto &peek_value = input.named_parameters.at("peek");
		if (peek_value.type().id() == LogicalTypeId::INTEGER || peek_value.type().id() == LogicalTypeId::BIGINT) {
			// INTEGER: custom size
			peek_size = peek_value.GetValue<int32_t>();
			peek_mode = "custom";
		} else {
			// VARCHAR: pass through to ParseExtractionConfig for validation (supports +schema suffix)
			peek_mode = peek_value.GetValue<string>();
		}
	}

	// Legacy parameter support (override if provided)
	if (seen_parameters.find("peek_size") != seen_parameters.end()) {
		peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
	}
	if (seen_parameters.find("peek_mode") != seen_parameters.end()) {
		peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
	}

	int32_t batch_size = 1; // Default = current streaming behavior
	if (seen_parameters.find("batch_size") != seen_parameters.end()) {
		batch_size = input.named_parameters.at("batch_size").GetValue<int32_t>();
		if (batch_size < 1) {
			throw BinderException("batch_size must be positive");
		}
	}

	// Use auto-detect for language
	string language = "auto";

	// Create ExtractionConfig from parsed parameters
	ExtractionConfig extraction_config =
	    ParseExtractionConfig(context_str, source_str, structure_str, peek_mode, peek_size);

	// Parse max_depth parameter (#014)
	if (seen_parameters.find("max_depth") != seen_parameters.end()) {
		extraction_config.max_depth = input.named_parameters.at("max_depth").GetValue<int32_t>();
	}

	// Parse prune parameter (#014)
	if (seen_parameters.find("prune") != seen_parameters.end()) {
		auto &prune_value = input.named_parameters.at("prune");
		auto &policy_list = ListValue::GetChildren(prune_value);
		for (auto &policy : policy_list) {
			if (policy.IsNull()) {
				throw BinderException("Prune policy list cannot contain NULL values");
			}
			CompilePrunePolicy(StringUtil::Lower(policy.ToString()), extraction_config);
		}
	}

	// Parse resource cap parameters (max_source_bytes, parse_timeout_ms, max_parse_nodes)
	ParseResourceCapParameters(input.named_parameters, extraction_config);

	// Use flat dynamic schema based on extraction config
	return_types = UnifiedASTBackend::GetFlatDynamicTableSchema(extraction_config);
	names = UnifiedASTBackend::GetFlatDynamicTableColumnNames(extraction_config);

	// Use the new ExtractionConfig constructor
	return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, extraction_config, batch_size);
}

// Initialize global state: expand file patterns, pre-resolve languages
static unique_ptr<GlobalTableFunctionState> ReadASTStreamingInit(ClientContext &context,
                                                                 TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<ReadASTStreamingBindData>();
	auto result = make_uniq<ReadASTStreamingGlobalState>();

	// Store configuration
	result->language = bind_data.language;
	result->ignore_errors = bind_data.ignore_errors;
	result->extraction_config = bind_data.extraction_config;

	try {
		// Expand file patterns and deduplicate
		vector<string> supported_extensions;
		if (bind_data.language != "auto") {
			supported_extensions = ASTFileUtils::GetSupportedExtensions(bind_data.language);
		}

		auto expanded_files =
		    ASTFileUtils::GetFiles(context, bind_data.file_patterns, bind_data.ignore_errors, supported_extensions);

		if (expanded_files.empty()) {
			if (!bind_data.ignore_errors) {
				throw IOException("read_ast needs at least one file to read");
			}
			// Leave all_file_paths empty; MaxThreads() returns 1, execute will produce 0 rows
			return std::move(result);
		}

		result->all_file_paths = std::move(expanded_files);

		// Pre-resolve languages for all files
		result->resolved_languages.reserve(result->all_file_paths.size());
		std::unordered_set<string> unique_languages;

		for (const auto &file_path : result->all_file_paths) {
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

		// Validate language support upfront
		auto &registry = LanguageAdapterRegistry::GetInstance();
		for (const auto &language : unique_languages) {
			auto adapter = registry.CreateAdapter(language);
			if (!adapter && !bind_data.ignore_errors) {
				throw InvalidInputException("Unsupported language: " + language);
			}
		}

		result->next_file_idx.store(0);

	} catch (const Exception &e) {
		if (!bind_data.ignore_errors) {
			throw IOException("Failed to initialize file processing: " + string(e.what()));
		}
		// Leave all_file_paths empty so threads produce 0 rows
	}

	return std::move(result);
}

// Initialize per-thread local state
static unique_ptr<LocalTableFunctionState> ReadASTInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                            GlobalTableFunctionState *global_state) {
	return make_uniq<ReadASTLocalState>();
}

// Flat streaming execute function — called from multiple threads by DuckDB's pipeline executor.
// Each thread has its own ReadASTLocalState and claims files atomically from the global state.
static void ReadASTFlatStreamingFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &global_state = data_p.global_state->Cast<ReadASTStreamingGlobalState>();
	auto &local_state = data_p.local_state->Cast<ReadASTLocalState>();

	idx_t output_index = 0;

	while (output_index < STANDARD_VECTOR_SIZE) {
		// If we have a current file result with remaining nodes, project them
		if (local_state.current_result && local_state.current_row_idx < local_state.current_result->nodes.size()) {
			UnifiedASTBackend::ProjectToDynamicTable(*local_state.current_result, output, local_state.current_row_idx,
			                                         output_index, global_state.extraction_config);
			// ProjectToDynamicTable advances both current_row_idx and output_index
			// If output chunk is full, it will stop and we return what we have
			if (output_index >= STANDARD_VECTOR_SIZE) {
				break;
			}
			// If we finished this file's nodes, fall through to claim next file
			if (local_state.current_row_idx >= local_state.current_result->nodes.size()) {
				local_state.current_result.reset();
				local_state.current_row_idx = 0;
			}
			continue;
		}

		// Claim the next file atomically
		idx_t file_idx = global_state.next_file_idx.fetch_add(1);
		if (file_idx >= global_state.all_file_paths.size()) {
			// No more files — this thread is done
			break;
		}

		const auto &file_path = global_state.all_file_paths[file_idx];
		const auto &file_language = global_state.resolved_languages[file_idx];

		// Skip files with unknown language
		if (file_language == "unknown") {
			continue;
		}

		try {
			// Get or create a cached adapter for this language (avoids per-file adapter creation)
			auto *adapter = local_state.GetOrCreateAdapter(file_language);
			if (!adapter) {
				if (!global_state.ignore_errors) {
					throw IOException("Unsupported language: " + file_language);
				}
				continue;
			}

			// Read file content, enforcing the configured source-size cap up front
			auto &config = global_state.extraction_config;
			auto &fs = FileSystem::GetFileSystem(context);
			string content = ReadSourceFileWithCap(fs, file_path, config.max_source_bytes);

			// Parse using unified backend with full ExtractionConfig (supports max_depth, prune)
			ASTResult result = UnifiedASTBackend::ParseToASTResult(content, file_language, file_path, config);

			local_state.current_result = make_uniq<ASTResult>(std::move(result));
			local_state.current_row_idx = 0;

		} catch (const std::exception &e) {
			if (!global_state.ignore_errors) {
				throw IOException("Failed to process " + file_path + ": " + e.what());
			}
			local_state.current_result.reset();
			continue;
		}
	}

	output.SetCardinality(output_index);
}

//==============================================================================
// NEW: Hierarchical Schema Bind Functions
//==============================================================================

// Hierarchical bind function for two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTHierarchicalBindTwoArg(ClientContext &context, TableFunctionBindInput &input,
                                                              vector<LogicalType> &return_types,
                                                              vector<string> &names) {
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

	return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, peek_size, peek_mode,
	                                           batch_size);
}

// Hierarchical bind function for one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTHierarchicalBindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                              vector<LogicalType> &return_types,
                                                              vector<string> &names) {
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

	return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, peek_size, peek_mode,
	                                           batch_size);
}

// Hierarchical streaming execute function — called from multiple threads by DuckDB's pipeline executor.
// Same pattern as flat: each thread claims files atomically and streams nodes.
static void ReadASTHierarchicalFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
	auto &global_state = data.global_state->Cast<ReadASTStreamingGlobalState>();
	auto &local_state = data.local_state->Cast<ReadASTLocalState>();

	idx_t output_index = 0;

	while (output_index < STANDARD_VECTOR_SIZE) {
		// If we have a current file result with remaining nodes, project them
		if (local_state.current_result && local_state.current_row_idx < local_state.current_result->nodes.size()) {
			idx_t old_output_index = output_index;
			UnifiedASTBackend::ProjectToHierarchicalTableStreaming(local_state.current_result->nodes, output,
			                                                       local_state.current_row_idx, output_index,
			                                                       local_state.current_result->source);

			idx_t rows_processed = output_index - old_output_index;
			local_state.current_row_idx += rows_processed;

			if (output_index >= STANDARD_VECTOR_SIZE) {
				break;
			}
			// If we finished this file's nodes, fall through to claim next file
			if (local_state.current_row_idx >= local_state.current_result->nodes.size()) {
				local_state.current_result.reset();
				local_state.current_row_idx = 0;
			}
			continue;
		}

		// Claim the next file atomically
		idx_t file_idx = global_state.next_file_idx.fetch_add(1);
		if (file_idx >= global_state.all_file_paths.size()) {
			break;
		}

		const auto &file_path = global_state.all_file_paths[file_idx];
		const auto &file_language = global_state.resolved_languages[file_idx];

		if (file_language == "unknown") {
			continue;
		}

		try {
			auto *adapter = local_state.GetOrCreateAdapter(file_language);
			if (!adapter) {
				if (!global_state.ignore_errors) {
					throw IOException("Unsupported language: " + file_language);
				}
				continue;
			}

			// Read file content, enforcing the configured source-size cap up front
			auto &config = global_state.extraction_config;
			auto &fs = FileSystem::GetFileSystem(context);
			string content = ReadSourceFileWithCap(fs, file_path, config.max_source_bytes);

			// Parse using unified backend with full ExtractionConfig (supports max_depth, prune)
			ASTResult result = UnifiedASTBackend::ParseToASTResult(content, file_language, file_path, config);

			local_state.current_result = make_uniq<ASTResult>(std::move(result));
			local_state.current_row_idx = 0;

		} catch (const std::exception &e) {
			if (!global_state.ignore_errors) {
				throw IOException("Failed to process " + file_path + ": " + e.what());
			}
			local_state.current_result.reset();
			continue;
		}
	}

	output.SetCardinality(output_index);
}

// Register the resource cap named parameters on a table function (shared by
// every read_ast variant so no registration site can drift)
static void AddResourceCapNamedParameters(TableFunction &func) {
	func.named_parameters["max_source_bytes"] = LogicalType::BIGINT;
	func.named_parameters["parse_timeout_ms"] = LogicalType::BIGINT;
	func.named_parameters["max_parse_nodes"] = LogicalType::BIGINT;
}

//==============================================================================
// Legacy Flat Schema Functions
//==============================================================================

// Flat schema read_ast functions using flat streaming implementation
static TableFunction GetReadASTFlatFunctionTwoArg() {
	TableFunction read_ast("read_ast", {LogicalType::ANY, LogicalType::VARCHAR}, ReadASTFlatStreamingFunction,
	                       ReadASTFlatStreamingBindTwoArg, ReadASTStreamingInit);
	read_ast.name = "read_ast";
	read_ast.init_local = ReadASTInitLocal;
	read_ast.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast.named_parameters["context"] = LogicalType::VARCHAR;
	read_ast.named_parameters["source"] = LogicalType::VARCHAR;
	read_ast.named_parameters["structure"] = LogicalType::VARCHAR;
	read_ast.named_parameters["peek"] = LogicalType::ANY; // Can be INTEGER or VARCHAR
	read_ast.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast);

	// Legacy parameters for backward compatibility
	read_ast.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast;
}

// Legacy flat schema functions
static TableFunction GetReadASTFlatFunctionOneArg() {
	TableFunction read_ast("read_ast", {LogicalType::ANY}, ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindOneArg,
	                       ReadASTStreamingInit);
	read_ast.name = "read_ast";
	read_ast.init_local = ReadASTInitLocal;
	read_ast.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast.named_parameters["context"] = LogicalType::VARCHAR;
	read_ast.named_parameters["source"] = LogicalType::VARCHAR;
	read_ast.named_parameters["structure"] = LogicalType::VARCHAR;
	read_ast.named_parameters["peek"] = LogicalType::ANY; // Can be INTEGER or VARCHAR
	read_ast.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast);

	// Legacy parameters for backward compatibility
	read_ast.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast;
}

// Keep streaming functions as well for explicit access (these use flat schema)
static TableFunction GetReadASTStreamingFunctionTwoArg() {
	TableFunction read_ast_streaming("read_ast_streaming", {LogicalType::ANY, LogicalType::VARCHAR},
	                                 ReadASTFlatStreamingFunction, ReadASTFlatStreamingBindTwoArg,
	                                 ReadASTStreamingInit);
	read_ast_streaming.name = "read_ast_streaming";
	read_ast_streaming.init_local = ReadASTInitLocal;
	read_ast_streaming.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast_streaming.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_streaming.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_streaming.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	read_ast_streaming.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast_streaming.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast_streaming.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast_streaming);
	return read_ast_streaming;
}

static TableFunction GetReadASTStreamingFunctionOneArg() {
	TableFunction read_ast_streaming("read_ast_streaming", {LogicalType::ANY}, ReadASTFlatStreamingFunction,
	                                 ReadASTFlatStreamingBindOneArg, ReadASTStreamingInit);
	read_ast_streaming.name = "read_ast_streaming";
	read_ast_streaming.init_local = ReadASTInitLocal;
	read_ast_streaming.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast_streaming.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_streaming.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_streaming.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	read_ast_streaming.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast_streaming.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast_streaming.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast_streaming);
	return read_ast_streaming;
}

// Hierarchical streaming bind function for two-argument version (explicit language)
static unique_ptr<FunctionData> ReadASTHierarchicalStreamingBindTwoArg(ClientContext &context,
                                                                       TableFunctionBindInput &input,
                                                                       vector<LogicalType> &return_types,
                                                                       vector<string> &names) {
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
	string context_str = "native"; // Default to native - memory issues fixed with flat schema
	if (seen_parameters.find("context") != seen_parameters.end()) {
		context_str = input.named_parameters.at("context").GetValue<string>();
	}

	string source_str = "lines"; // Default
	if (seen_parameters.find("source") != seen_parameters.end()) {
		source_str = input.named_parameters.at("source").GetValue<string>();
	}

	string structure_str = "full"; // Default
	if (seen_parameters.find("structure") != seen_parameters.end()) {
		structure_str = input.named_parameters.at("structure").GetValue<string>();
	}

	// Parse unified peek parameter (can be INTEGER or VARCHAR)
	// Validation is deferred to ParseExtractionConfig which handles +schema suffix
	int32_t peek_size = 120;
	string peek_mode = "smart";
	if (seen_parameters.find("peek") != seen_parameters.end()) {
		auto &peek_value = input.named_parameters.at("peek");
		if (peek_value.type().id() == LogicalTypeId::INTEGER || peek_value.type().id() == LogicalTypeId::BIGINT) {
			// INTEGER: custom size
			peek_size = peek_value.GetValue<int32_t>();
			peek_mode = "custom";
		} else {
			// VARCHAR: pass through to ParseExtractionConfig for validation (supports +schema suffix)
			peek_mode = peek_value.GetValue<string>();
		}
	}

	// Legacy parameter support (override if provided)
	if (seen_parameters.find("peek_size") != seen_parameters.end()) {
		peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
	}
	if (seen_parameters.find("peek_mode") != seen_parameters.end()) {
		peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
	}

	int32_t batch_size = 1; // Default = current streaming behavior
	if (seen_parameters.find("batch_size") != seen_parameters.end()) {
		batch_size = input.named_parameters.at("batch_size").GetValue<int32_t>();
		if (batch_size < 1) {
			throw BinderException("batch_size must be positive");
		}
	}

	// Create ExtractionConfig from parsed parameters
	ExtractionConfig extraction_config =
	    ParseExtractionConfig(context_str, source_str, structure_str, peek_mode, peek_size);

	// Parse max_depth parameter (#014)
	if (seen_parameters.find("max_depth") != seen_parameters.end()) {
		extraction_config.max_depth = input.named_parameters.at("max_depth").GetValue<int32_t>();
	}

	// Parse prune parameter (#014)
	if (seen_parameters.find("prune") != seen_parameters.end()) {
		auto &prune_value = input.named_parameters.at("prune");
		auto &policy_list = ListValue::GetChildren(prune_value);
		for (auto &policy : policy_list) {
			if (policy.IsNull()) {
				throw BinderException("Prune policy list cannot contain NULL values");
			}
			CompilePrunePolicy(StringUtil::Lower(policy.ToString()), extraction_config);
		}
	}

	// Parse resource cap parameters (max_source_bytes, parse_timeout_ms, max_parse_nodes)
	ParseResourceCapParameters(input.named_parameters, extraction_config);

	// Use hierarchical backend schema
	return_types = UnifiedASTBackend::GetHierarchicalTableSchema();
	names = UnifiedASTBackend::GetHierarchicalTableColumnNames();

	// Use the new ExtractionConfig constructor
	return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, extraction_config, batch_size);
}

// Hierarchical streaming bind function for one-argument version (auto-detect language)
static unique_ptr<FunctionData> ReadASTHierarchicalStreamingBindOneArg(ClientContext &context,
                                                                       TableFunctionBindInput &input,
                                                                       vector<LogicalType> &return_types,
                                                                       vector<string> &names) {
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
	string context_str = "native"; // Default to native - memory issues fixed with flat schema
	if (seen_parameters.find("context") != seen_parameters.end()) {
		context_str = input.named_parameters.at("context").GetValue<string>();
	}

	string source_str = "lines"; // Default
	if (seen_parameters.find("source") != seen_parameters.end()) {
		source_str = input.named_parameters.at("source").GetValue<string>();
	}

	string structure_str = "full"; // Default
	if (seen_parameters.find("structure") != seen_parameters.end()) {
		structure_str = input.named_parameters.at("structure").GetValue<string>();
	}

	// Parse unified peek parameter (can be INTEGER or VARCHAR)
	// Validation is deferred to ParseExtractionConfig which handles +schema suffix
	int32_t peek_size = 120;
	string peek_mode = "smart";
	if (seen_parameters.find("peek") != seen_parameters.end()) {
		auto &peek_value = input.named_parameters.at("peek");
		if (peek_value.type().id() == LogicalTypeId::INTEGER || peek_value.type().id() == LogicalTypeId::BIGINT) {
			// INTEGER: custom size
			peek_size = peek_value.GetValue<int32_t>();
			peek_mode = "custom";
		} else {
			// VARCHAR: pass through to ParseExtractionConfig for validation (supports +schema suffix)
			peek_mode = peek_value.GetValue<string>();
		}
	}

	// Legacy parameter support (override if provided)
	if (seen_parameters.find("peek_size") != seen_parameters.end()) {
		peek_size = input.named_parameters.at("peek_size").GetValue<int32_t>();
	}
	if (seen_parameters.find("peek_mode") != seen_parameters.end()) {
		peek_mode = input.named_parameters.at("peek_mode").GetValue<string>();
	}

	int32_t batch_size = 1; // Default = current streaming behavior
	if (seen_parameters.find("batch_size") != seen_parameters.end()) {
		batch_size = input.named_parameters.at("batch_size").GetValue<int32_t>();
		if (batch_size < 1) {
			throw BinderException("batch_size must be positive");
		}
	}

	// Use auto-detect for language
	string language = "auto";

	// Create ExtractionConfig from parsed parameters
	ExtractionConfig extraction_config =
	    ParseExtractionConfig(context_str, source_str, structure_str, peek_mode, peek_size);

	// Parse max_depth parameter (#014)
	if (seen_parameters.find("max_depth") != seen_parameters.end()) {
		extraction_config.max_depth = input.named_parameters.at("max_depth").GetValue<int32_t>();
	}

	// Parse prune parameter (#014)
	if (seen_parameters.find("prune") != seen_parameters.end()) {
		auto &prune_value = input.named_parameters.at("prune");
		auto &policy_list = ListValue::GetChildren(prune_value);
		for (auto &policy : policy_list) {
			if (policy.IsNull()) {
				throw BinderException("Prune policy list cannot contain NULL values");
			}
			CompilePrunePolicy(StringUtil::Lower(policy.ToString()), extraction_config);
		}
	}

	// Parse resource cap parameters (max_source_bytes, parse_timeout_ms, max_parse_nodes)
	ParseResourceCapParameters(input.named_parameters, extraction_config);

	// Use hierarchical backend schema
	return_types = UnifiedASTBackend::GetHierarchicalTableSchema();
	names = UnifiedASTBackend::GetHierarchicalTableColumnNames();

	// Use the new ExtractionConfig constructor
	return make_uniq<ReadASTStreamingBindData>(file_patterns, language, ignore_errors, extraction_config, batch_size);
}

// Functions for read_ast (using hierarchical STRUCT schema)
static TableFunction GetReadASTFunctionTwoArg() {
	TableFunction read_ast_hierarchical_new("read_ast_hierarchical_new", {LogicalType::ANY, LogicalType::VARCHAR},
	                                        ReadASTHierarchicalFunction, ReadASTHierarchicalStreamingBindTwoArg,
	                                        ReadASTStreamingInit);
	read_ast_hierarchical_new.name = "read_ast_hierarchical_new";
	read_ast_hierarchical_new.init_local = ReadASTInitLocal;
	read_ast_hierarchical_new.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast_hierarchical_new.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_hierarchical_new.named_parameters["context"] = LogicalType::VARCHAR;
	read_ast_hierarchical_new.named_parameters["source"] = LogicalType::VARCHAR;
	read_ast_hierarchical_new.named_parameters["structure"] = LogicalType::VARCHAR;
	read_ast_hierarchical_new.named_parameters["peek"] = LogicalType::ANY; // Can be INTEGER or VARCHAR
	read_ast_hierarchical_new.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast_hierarchical_new.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast_hierarchical_new.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast_hierarchical_new);

	// Legacy parameters for backward compatibility
	read_ast_hierarchical_new.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_hierarchical_new.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast_hierarchical_new;
}

// Hierarchical functions with STRUCT schema using streaming bind (explicit access)
static TableFunction GetReadASTHierarchicalFunctionTwoArg() {
	TableFunction read_ast_hierarchical("read_ast_hierarchical", {LogicalType::ANY, LogicalType::VARCHAR},
	                                    ReadASTHierarchicalFunction, ReadASTHierarchicalStreamingBindTwoArg,
	                                    ReadASTStreamingInit);
	read_ast_hierarchical.name = "read_ast_hierarchical";
	read_ast_hierarchical.init_local = ReadASTInitLocal;
	read_ast_hierarchical.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast_hierarchical.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_hierarchical.named_parameters["context"] = LogicalType::VARCHAR;
	read_ast_hierarchical.named_parameters["source"] = LogicalType::VARCHAR;
	read_ast_hierarchical.named_parameters["structure"] = LogicalType::VARCHAR;
	read_ast_hierarchical.named_parameters["peek"] = LogicalType::ANY; // Can be INTEGER or VARCHAR
	read_ast_hierarchical.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast_hierarchical.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast_hierarchical.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast_hierarchical);

	// Legacy parameters for backward compatibility
	read_ast_hierarchical.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_hierarchical.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast_hierarchical;
}

static TableFunction GetReadASTFunctionOneArg() {
	TableFunction read_ast_hierarchical_new("read_ast_hierarchical_new", {LogicalType::ANY},
	                                        ReadASTHierarchicalFunction, ReadASTHierarchicalStreamingBindOneArg,
	                                        ReadASTStreamingInit);
	read_ast_hierarchical_new.name = "read_ast_hierarchical_new";
	read_ast_hierarchical_new.init_local = ReadASTInitLocal;
	read_ast_hierarchical_new.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast_hierarchical_new.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_hierarchical_new.named_parameters["context"] = LogicalType::VARCHAR;
	read_ast_hierarchical_new.named_parameters["source"] = LogicalType::VARCHAR;
	read_ast_hierarchical_new.named_parameters["structure"] = LogicalType::VARCHAR;
	read_ast_hierarchical_new.named_parameters["peek"] = LogicalType::ANY; // Can be INTEGER or VARCHAR
	read_ast_hierarchical_new.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast_hierarchical_new.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast_hierarchical_new.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast_hierarchical_new);

	// Legacy parameters for backward compatibility
	read_ast_hierarchical_new.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_hierarchical_new.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast_hierarchical_new;
}

static TableFunction GetReadASTHierarchicalFunctionOneArg() {
	TableFunction read_ast_hierarchical("read_ast_hierarchical", {LogicalType::ANY}, ReadASTHierarchicalFunction,
	                                    ReadASTHierarchicalStreamingBindOneArg, ReadASTStreamingInit);
	read_ast_hierarchical.name = "read_ast_hierarchical";
	read_ast_hierarchical.init_local = ReadASTInitLocal;
	read_ast_hierarchical.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast_hierarchical.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_hierarchical.named_parameters["context"] = LogicalType::VARCHAR;
	read_ast_hierarchical.named_parameters["source"] = LogicalType::VARCHAR;
	read_ast_hierarchical.named_parameters["structure"] = LogicalType::VARCHAR;
	read_ast_hierarchical.named_parameters["peek"] = LogicalType::ANY; // Can be INTEGER or VARCHAR
	read_ast_hierarchical.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast_hierarchical.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast_hierarchical.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast_hierarchical);

	// Legacy parameters for backward compatibility
	read_ast_hierarchical.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_hierarchical.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast_hierarchical;
}

// Create read_ast_flat alias functions (explicit access to flat schema)
static TableFunction GetReadASTFlatAliasFunctionOneArg() {
	TableFunction read_ast_flat("read_ast_flat", {LogicalType::ANY}, ReadASTFlatStreamingFunction,
	                            ReadASTFlatStreamingBindOneArg, ReadASTStreamingInit);
	read_ast_flat.name = "read_ast_flat";
	read_ast_flat.init_local = ReadASTInitLocal;
	read_ast_flat.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast_flat.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_flat.named_parameters["context"] = LogicalType::VARCHAR;
	read_ast_flat.named_parameters["source"] = LogicalType::VARCHAR;
	read_ast_flat.named_parameters["structure"] = LogicalType::VARCHAR;
	read_ast_flat.named_parameters["peek"] = LogicalType::ANY; // Can be INTEGER or VARCHAR
	read_ast_flat.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast_flat.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast_flat.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast_flat);

	// Legacy parameters for backward compatibility
	read_ast_flat.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_flat.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast_flat;
}

static TableFunction GetReadASTFlatAliasFunctionTwoArg() {
	TableFunction read_ast_flat("read_ast_flat", {LogicalType::ANY, LogicalType::VARCHAR}, ReadASTFlatStreamingFunction,
	                            ReadASTFlatStreamingBindTwoArg, ReadASTStreamingInit);
	read_ast_flat.name = "read_ast_flat";
	read_ast_flat.init_local = ReadASTInitLocal;
	read_ast_flat.order_preservation_type = OrderPreservationType::NO_ORDER;
	read_ast_flat.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
	read_ast_flat.named_parameters["context"] = LogicalType::VARCHAR;
	read_ast_flat.named_parameters["source"] = LogicalType::VARCHAR;
	read_ast_flat.named_parameters["structure"] = LogicalType::VARCHAR;
	read_ast_flat.named_parameters["peek"] = LogicalType::ANY; // Can be INTEGER or VARCHAR
	read_ast_flat.named_parameters["batch_size"] = LogicalType::INTEGER;
	read_ast_flat.named_parameters["max_depth"] = LogicalType::INTEGER;
	read_ast_flat.named_parameters["prune"] = LogicalType::LIST(LogicalType::VARCHAR);
	AddResourceCapNamedParameters(read_ast_flat);

	// Legacy parameters for backward compatibility
	read_ast_flat.named_parameters["peek_size"] = LogicalType::INTEGER;
	read_ast_flat.named_parameters["peek_mode"] = LogicalType::VARCHAR;
	return read_ast_flat;
}

void RegisterReadASTFunction(ExtensionLoader &loader) {
	// Register default read_ast functions (now using flat schema - production ready)
	loader.RegisterFunction(GetReadASTFlatFunctionOneArg()); // ANY (auto-detect)
	loader.RegisterFunction(GetReadASTFlatFunctionTwoArg()); // ANY, VARCHAR (explicit language)

	// Register read_ast_flat aliases (explicit access to flat schema)
	loader.RegisterFunction(GetReadASTFlatAliasFunctionOneArg()); // ANY (auto-detect)
	loader.RegisterFunction(GetReadASTFlatAliasFunctionTwoArg()); // ANY, VARCHAR (explicit language)

	// Register read_ast_hierarchical_new functions (hierarchical STRUCT schema with memory issues)
	loader.RegisterFunction(GetReadASTFunctionOneArg()); // ANY (auto-detect)
	loader.RegisterFunction(GetReadASTFunctionTwoArg()); // ANY, VARCHAR (explicit language)

	// Register read_ast_hierarchical functions for backward compatibility
	loader.RegisterFunction(GetReadASTHierarchicalFunctionOneArg()); // ANY (auto-detect)
	loader.RegisterFunction(GetReadASTHierarchicalFunctionTwoArg()); // ANY, VARCHAR (explicit language)
}

void RegisterReadASTStreamingFunction(ExtensionLoader &loader) {
	// Register explicit streaming functions for comparison
	loader.RegisterFunction(GetReadASTStreamingFunctionOneArg());
	loader.RegisterFunction(GetReadASTStreamingFunctionTwoArg());
}

} // namespace duckdb
