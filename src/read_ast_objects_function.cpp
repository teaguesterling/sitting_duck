#include "read_ast_objects_function.hpp"
#include "ast_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/extension_util.hpp"

namespace duckdb {

TableFunction ReadASTObjectsFunction::GetFunction() {
    TableFunction function("read_ast_objects", {LogicalType::VARCHAR, LogicalType::VARCHAR}, Execute, Bind);
    function.name = "read_ast_objects";
    return function;
}

unique_ptr<FunctionData> ReadASTObjectsFunction::Bind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<string> &names) {
    if (input.inputs.size() != 2) {
        throw BinderException("read_ast_objects requires exactly 2 arguments: file_pattern and language");
    }
    
    auto file_pattern = input.inputs[0].GetValue<string>();
    auto language = input.inputs[1].GetValue<string>();
    
    // Get list of files matching pattern
    auto &fs = FileSystem::GetFileSystem(context);
    vector<string> files;
    
    // Simple file pattern expansion - in production would use proper globbing
    if (file_pattern.find('*') != string::npos) {
        // For now, just handle single file
        throw NotImplementedException("File patterns not yet implemented. Please specify a single file.");
    } else {
        // Single file
        files.push_back(file_pattern);
    }
    
    // Define output columns
    names = {"file_path", "ast"};
    return_types = {LogicalType::VARCHAR, LogicalType::BLOB};  // Using BLOB to store serialized AST for now
    
    return make_uniq<ReadASTObjectsData>(std::move(files), std::move(language));
}

unique_ptr<ASTType> ReadASTObjectsFunction::ParseFile(ClientContext &context, const string &file_path, const string &language) {
    auto &fs = FileSystem::GetFileSystem(context);
    
    // Read file content
    auto handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
    auto file_size = fs.GetFileSize(*handle);
    
    string source_code;
    source_code.resize(file_size);
    fs.Read(*handle, (void*)source_code.data(), file_size);
    
    // Create parser for language
    ASTParser parser;
    TSParser *ts_parser = parser.CreateParser(language);
    if (!ts_parser) {
        throw IOException("Unsupported language: " + language);
    }
    
    // Parse file into AST
    auto ast = make_uniq<ASTType>(file_path, language);
    ast->ParseFile(source_code, ts_parser);
    
    ts_parser_delete(ts_parser);
    return ast;
}

void ReadASTObjectsFunction::Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
    auto &data = data_p.bind_data->CastNoConst<ReadASTObjectsData>();
    
    idx_t count = 0;
    auto file_path_data = FlatVector::GetData<string_t>(output.data[0]);
    auto ast_data = FlatVector::GetData<string_t>(output.data[1]);
    
    while (data.current_file_idx < data.files.size() && count < STANDARD_VECTOR_SIZE) {
        const auto &file_path = data.files[data.current_file_idx];
        
        try {
            // Parse file
            auto ast = ParseFile(context, file_path, data.language);
            
            // Serialize AST to JSON for now (will use proper type later)
            string json = ast->ToJSON();
            
            // Set values
            file_path_data[count] = StringVector::AddString(output.data[0], file_path);
            ast_data[count] = StringVector::AddStringOrBlob(output.data[1], json);
            
            count++;
        } catch (const Exception &e) {
            // Skip files that can't be parsed
            // Could optionally add error handling here
        }
        
        data.current_file_idx++;
    }
    
    output.SetCardinality(count);
}

void RegisterReadASTObjectsFunction(DatabaseInstance &instance) {
    ExtensionUtil::RegisterFunction(instance, ReadASTObjectsFunction::GetFunction());
}

} // namespace duckdb