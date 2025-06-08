#include "read_ast_objects_hybrid.hpp"
#include "unified_ast_backend.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"
#include <sstream>
#include <iomanip>
#include <cctype>

namespace duckdb {


// Helper function to detect language from file extension
string DetectLanguageFromExtension(const string &file_path) {
    auto dot_pos = file_path.find_last_of('.');
    if (dot_pos == string::npos) {
        return "auto";  // No extension, can't detect
    }
    
    string ext = StringUtil::Lower(file_path.substr(dot_pos + 1));
    
    // Try to find an adapter that has this extension as an alias
    auto& registry = LanguageAdapterRegistry::GetInstance();
    
    // Check if the extension itself is registered as an alias
    const LanguageAdapter* adapter = registry.GetAdapter(ext);
    if (adapter) {
        return adapter->GetLanguageName();
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
    
    // Return single AST column using unified schema
    names = {"ast"};
    return_types = {UnifiedASTBackend::GetASTStructSchema()};
    
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
    
    // Return single AST column using unified schema
    names = {"ast"};
    return_types = {UnifiedASTBackend::GetASTStructSchema()};
    
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
    
    // Use unified parsing backend
    ASTResult result = UnifiedASTBackend::ParseToASTResult(content, language, file_path);
    
    // Return AST struct using unified backend
    return UnifiedASTBackend::CreateASTStruct(result);
}

void ReadASTObjectsHybridFunction::Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &data = data_p.bind_data->CastNoConst<ReadASTObjectsHybridData>();
    
    idx_t count = 0;
    
    while (data.current_file_idx < data.files.size() && count < STANDARD_VECTOR_SIZE) {
        const auto &file_path = data.files[data.current_file_idx];
        
        try {
            // Parse file to AST struct
            auto ast_type = output.data[0].GetType();
            Value ast_struct_value = ParseFileToStructs(context, file_path, data.language, ast_type, data.filter_config);
            
            // Set the AST struct value directly
            output.data[0].SetValue(count, ast_struct_value);
            
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