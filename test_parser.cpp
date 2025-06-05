#include <iostream>
#include <tree_sitter/api.h>

extern "C" {
    const TSLanguage *tree_sitter_python();
}

int main() {
    std::cout << "Creating parser..." << std::endl;
    TSParser* parser = ts_parser_new();
    if (!parser) {
        std::cout << "Failed to create parser" << std::endl;
        return 1;
    }
    std::cout << "Parser created successfully" << std::endl;
    
    std::cout << "Getting Python language..." << std::endl;
    const TSLanguage* lang = tree_sitter_python();
    if (!lang) {
        std::cout << "Failed to get language" << std::endl;
        return 1;
    }
    std::cout << "Language obtained successfully" << std::endl;
    
    std::cout << "Setting language..." << std::endl;
    if (!ts_parser_set_language(parser, lang)) {
        std::cout << "Failed to set language" << std::endl;
        return 1;
    }
    std::cout << "Language set successfully" << std::endl;
    
    ts_parser_delete(parser);
    std::cout << "Test completed successfully" << std::endl;
    return 0;
}