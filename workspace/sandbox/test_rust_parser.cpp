#include <iostream>
#include <tree_sitter/api.h>
#include <cstring>

extern "C" {
    TSLanguage *tree_sitter_rust();
}

int main() {
    std::cout << "Creating parser..." << std::endl;
    TSParser *parser = ts_parser_new();
    if (!parser) {
        std::cerr << "Failed to create parser" << std::endl;
        return 1;
    }
    
    std::cout << "Getting Rust language..." << std::endl;
    const TSLanguage *lang = tree_sitter_rust();
    if (!lang) {
        std::cerr << "Failed to get Rust language" << std::endl;
        ts_parser_delete(parser);
        return 1;
    }
    
    std::cout << "Language ABI version: " << ts_language_version(lang) << std::endl;
    std::cout << "Tree-sitter lib version: " << TREE_SITTER_LANGUAGE_VERSION << std::endl;
    std::cout << "Min compatible version: " << TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION << std::endl;
    
    std::cout << "Setting language..." << std::endl;
    if (!ts_parser_set_language(parser, lang)) {
        std::cerr << "Failed to set language" << std::endl;
        ts_parser_delete(parser);
        return 1;
    }
    
    std::cout << "Parsing simple Rust code..." << std::endl;
    const char *source_code = "fn main() { println!(\"Hello\"); }";
    TSTree *tree = ts_parser_parse_string(parser, nullptr, source_code, strlen(source_code));
    
    if (!tree) {
        std::cerr << "Failed to parse" << std::endl;
        ts_parser_delete(parser);
        return 1;
    }
    
    std::cout << "Parse successful!" << std::endl;
    TSNode root = ts_tree_root_node(tree);
    std::cout << "Root node type: " << ts_node_type(root) << std::endl;
    
    ts_tree_delete(tree);
    ts_parser_delete(parser);
    
    return 0;
}