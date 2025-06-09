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
    TableFunction function("read_ast_objects", {LogicalType::ANY}, Execute, BindOneArg);
    function.name = "read_ast_objects";
    
    // Add named parameters for filtering (same as the two-arg version)
    function.named_parameters["exclude_types"] = LogicalType::LIST(LogicalType::VARCHAR);
    function.named_parameters["include_types"] = LogicalType::LIST(LogicalType::VARCHAR);
    function.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    
    return function;
}

TableFunction ReadASTObjectsHybridFunction::GetFunctionWithFilters() {
    TableFunction function("read_ast_objects", {LogicalType::ANY, LogicalType::VARCHAR}, Execute, BindWithFilters);
    function.name = "read_ast_objects";
    
    // Set named parameters for filtering (all optional)
    function.named_parameters["exclude_types"] = LogicalType::LIST(LogicalType::VARCHAR);
    function.named_parameters["include_types"] = LogicalType::LIST(LogicalType::VARCHAR);
    function.named_parameters["ignore_errors"] = LogicalType::BOOLEAN;
    
    return function;
}


unique_ptr<FunctionData> ReadASTObjectsHybridFunction::BindOneArg(ClientContext &context, TableFunctionBindInput &input,
                                                                vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 1) {
        throw BinderException("read_ast_objects with one argument requires exactly 1 argument: file_pattern");
    }
    
    auto file_path_value = input.inputs[0];
    
    // Parse named parameters
    bool ignore_errors = false;
    if (input.named_parameters.find("ignore_errors") != input.named_parameters.end()) {
        ignore_errors = input.named_parameters.at("ignore_errors").GetValue<bool>();
    }
    
    // Use auto-detect for language
    string language = "auto";
    
    // Parse named filter parameters (for future use - currently ignored)
    FilterConfig filter_config; // Empty config - no filtering applied
    
    // Return single AST column using unified schema
    names = {"ast"};
    return_types = {UnifiedASTBackend::GetASTStructSchema()};
    
    return make_uniq<ReadASTObjectsHybridData>(file_path_value, std::move(language), std::move(filter_config), ignore_errors);
}

unique_ptr<FunctionData> ReadASTObjectsHybridFunction::BindWithFilters(ClientContext &context, TableFunctionBindInput &input,
                                                                      vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("read_ast_objects with filters requires 2 positional arguments: file_pattern, language");
    }
    
    auto file_path_value = input.inputs[0];
    auto language = input.inputs[1].GetValue<string>();
    
    // Parse named parameters
    bool ignore_errors = false;
    if (input.named_parameters.find("ignore_errors") != input.named_parameters.end()) {
        ignore_errors = input.named_parameters.at("ignore_errors").GetValue<bool>();
    }
    
    // Parse named filter parameters (for future use - currently ignored)
    FilterConfig filter_config; // Empty config - no filtering applied
    
    // Return single AST column using unified schema
    names = {"ast"};
    return_types = {UnifiedASTBackend::GetASTStructSchema()};
    
    return make_uniq<ReadASTObjectsHybridData>(file_path_value, std::move(language), std::move(filter_config), ignore_errors);
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
    
    // Parse the file(s) if not already done
    if (!data.parsed) {
        try {
            // Use unified parsing backend with glob support
            data.collection = UnifiedASTBackend::ParseFilesToASTCollection(context, data.file_path_value, data.language, data.ignore_errors);
            data.parsed = true;
        } catch (const Exception &e) {
            throw IOException("Failed to parse files: " + string(e.what()));
        }
    }
    
    // Return each parsed file as a separate AST struct
    idx_t count = 0;
    
    while (data.current_result_index < data.collection.results.size() && count < STANDARD_VECTOR_SIZE) {
        auto &current_result = data.collection.results[data.current_result_index];
        
        // Create AST struct value for this individual file
        Value ast_struct_value = UnifiedASTBackend::CreateASTStruct(current_result);
        
        // Set the AST struct value
        output.data[0].SetValue(count, ast_struct_value);
        
        count++;
        data.current_result_index++;
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