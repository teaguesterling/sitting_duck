#include "duckdb.hpp"
#include "unified_ast_backend.hpp"

using namespace duckdb;

int main() {
    // Test the collection creation in isolation
    auto database = std::make_unique<DuckDB>(nullptr);
    auto connection = std::make_unique<Connection>(*database);
    auto context = connection->context;
    
    printf("Creating file list Value...\n");
    
    // Create a simple list of two files
    vector<Value> file_list;
    file_list.push_back(Value("src/unified_ast_backend.cpp"));
    file_list.push_back(Value("src/ast_type.cpp"));
    
    Value file_list_value = Value::LIST(LogicalType::VARCHAR, file_list);
    
    printf("Calling ParseFilesToASTCollection...\n");
    
    try {
        auto collection = UnifiedASTBackend::ParseFilesToASTCollection(*context, file_list_value, "auto", false);
        printf("Success! Parsed %zu files\n", collection.results.size());
        
        // Try to access the collection data
        for (size_t i = 0; i < collection.results.size(); i++) {
            printf("File %zu: %s, %zu nodes\n", i, 
                   collection.results[i].source.file_path.c_str(),
                   collection.results[i].nodes.size());
        }
        
        printf("Collection scope ending...\n");
    } catch (const std::exception& e) {
        printf("Exception: %s\n", e.what());
        return 1;
    }
    
    printf("Main scope ending...\n");
    return 0;
}