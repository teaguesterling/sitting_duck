#include <iostream>
#include "unified_ast_backend.hpp"
#include "language_adapter.hpp"

using namespace duckdb;

int main() {
    try {
        std::cout << "Getting language adapter registry..." << std::endl;
        auto& registry = LanguageAdapterRegistry::GetInstance();
        
        std::cout << "Getting Python adapter..." << std::endl;
        const LanguageAdapter* adapter = registry.GetAdapter("python");
        if (!adapter) {
            std::cout << "Failed to get Python adapter" << std::endl;
            return 1;
        }
        
        std::cout << "Parsing simple Python code..." << std::endl;
        ASTResult result = UnifiedASTBackend::ParseToASTResult("x = 1", "python", "<test>");
        
        std::cout << "Parse completed successfully!" << std::endl;
        std::cout << "Node count: " << result.nodes.size() << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}