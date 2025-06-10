#include "language_handler.hpp"
#include "grammars.hpp"
#include "duckdb/common/exception.hpp"
#include <tree_sitter/api.h>
#include <cstring>

// Tree-sitter function declarations
extern "C" {
    const TSLanguage *tree_sitter_python();
    const TSLanguage *tree_sitter_javascript();
    const TSLanguage *tree_sitter_cpp();
    const TSLanguage *tree_sitter_typescript();
    // Temporarily disabled due to ABI compatibility issues:
    // const TSLanguage *tree_sitter_rust();
}

namespace duckdb {

//==============================================================================
// Base LanguageHandler implementation
//==============================================================================

LanguageHandler::~LanguageHandler() {
    if (parser) {
        ts_parser_delete(parser);
        parser = nullptr;
    }
}

string LanguageHandler::FindIdentifierChild(TSNode node, const string &content) const {
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);
        if (strcmp(child_type, "identifier") == 0) {
            return ExtractNodeText(child, content);
        }
    }
    return "";
}

string LanguageHandler::ExtractNodeText(TSNode node, const string &content) const {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (start < content.size() && end <= content.size()) {
        return content.substr(start, end - start);
    }
    return "";
}

void LanguageHandler::SetParserLanguageWithValidation(TSParser* parser, const TSLanguage* language, const string &language_name) const {
    if (!language) {
        throw InvalidInputException("Tree-sitter language for " + language_name + " is NULL");
    }
    
    // Check ABI compatibility
    uint32_t language_version = ts_language_version(language);
    if (language_version > TREE_SITTER_LANGUAGE_VERSION) {
        throw InvalidInputException(
            language_name + " grammar ABI version " + std::to_string(language_version) + 
            " is newer than tree-sitter library version " + std::to_string(TREE_SITTER_LANGUAGE_VERSION) +
            ". Please update the tree-sitter library.");
    }
    
    if (language_version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) {
        throw InvalidInputException(
            language_name + " grammar ABI version " + std::to_string(language_version) + 
            " is too old for tree-sitter library (minimum version: " + 
            std::to_string(TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) +
            "). Please regenerate the grammar with a newer tree-sitter CLI.");
    }
    
    if (!ts_parser_set_language(parser, language)) {
        throw InvalidInputException("Failed to set " + language_name + " language for parser");
    }
}

//==============================================================================
// PythonLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> PythonLanguageHandler::type_mappings = {
    // Declarations
    {"function_definition", NormalizedTypes::FUNCTION_DECLARATION},
    {"async_function_definition", NormalizedTypes::FUNCTION_DECLARATION},
    {"class_definition", NormalizedTypes::CLASS_DECLARATION},
    {"assignment", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Expressions
    {"call", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"string", NormalizedTypes::LITERAL},
    {"integer", NormalizedTypes::LITERAL},
    {"float", NormalizedTypes::LITERAL},
    {"true", NormalizedTypes::LITERAL},
    {"false", NormalizedTypes::LITERAL},
    {"none", NormalizedTypes::LITERAL},
    {"binary_operator", NormalizedTypes::BINARY_EXPRESSION},
    
    // Control flow
    {"if_statement", NormalizedTypes::IF_STATEMENT},
    {"for_statement", NormalizedTypes::LOOP_STATEMENT},
    {"while_statement", NormalizedTypes::LOOP_STATEMENT},
    {"return_statement", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"comment", NormalizedTypes::COMMENT},
    {"import_statement", NormalizedTypes::IMPORT_STATEMENT},
    {"import_from_statement", NormalizedTypes::IMPORT_STATEMENT},
};

string PythonLanguageHandler::GetLanguageName() const {
    return "python";
}

vector<string> PythonLanguageHandler::GetAliases() const {
    return {"python", "py"};
}

void PythonLanguageHandler::InitializeParser() const {
    parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    const TSLanguage *ts_language = tree_sitter_python();
    SetParserLanguageWithValidation(parser, ts_language, "Python");
}

string PythonLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string PythonLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    // Extract names for declaration types
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION || 
        normalized == NormalizedTypes::CLASS_DECLARATION ||
        normalized == NormalizedTypes::METHOD_DECLARATION) {
        return FindIdentifierChild(node, content);
    } else if (normalized == NormalizedTypes::VARIABLE_REFERENCE) {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

string PythonLanguageHandler::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // For literals, extract the actual value
    if (node_type == "string" || node_type == "integer" || node_type == "float" || 
        node_type == "true" || node_type == "false" || node_type == "none") {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

bool PythonLanguageHandler::IsPublicNode(TSNode node, const string &content) const {
    // For now, always return false - TODO: implement Python visibility rules
    return false;
}

const NodeTypeConfig* PythonLanguageHandler::GetNodeTypeConfig(const string &node_type) const {
    // Simple if-chain approach for KIND mapping
    if (node_type == "function_definition" || node_type == "async_function_definition") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "class_definition") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 1, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "call") {
        static NodeTypeConfig config(ASTKind::COMPUTATION, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    if (node_type == "identifier") {
        static NodeTypeConfig config(ASTKind::NAME, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "string" || node_type == "integer" || node_type == "float") {
        static NodeTypeConfig config(ASTKind::LITERAL, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "if_statement") {
        static NodeTypeConfig config(ASTKind::FLOW_CONTROL, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    
    // Default fallback
    static NodeTypeConfig default_config(ASTKind::PARSER_SPECIFIC, 0, 0, HashMethod::Structural(), 0);
    return &default_config;
}

const LanguageConfig& PythonLanguageHandler::GetConfig() const {
    // TODO: implement proper LanguageConfig - for now just throw
    throw NotImplementedException("LanguageConfig not implemented yet");
}

void PythonLanguageHandler::ParseFile(const string &content, vector<ASTNode> &nodes) const {
    TSParser* my_parser = GetParser();
    
    TSTree *tree = ts_parser_parse_string(my_parser, nullptr, content.c_str(), content.length());
    if (!tree) {
        throw IOException("Failed to parse Python content");
    }
    
    // Convert tree to nodes using depth-first traversal
    TSNode root = ts_tree_root_node(tree);
    int64_t node_counter = 0;
    
    struct StackEntry {
        TSNode node;
        int64_t parent_id;
        int32_t depth;
        int32_t sibling_index;
    };
    
    vector<StackEntry> stack;
    stack.push_back({root, -1, 0, 0});
    
    while (!stack.empty()) {
        auto entry = stack.back();
        stack.pop_back();
        
        // Create ASTNode
        ASTNode ast_node;
        ast_node.tree_position.node_index = node_counter++;
        ast_node.node_id = ast_node.tree_position.node_index; // Simple ID for now
        ast_node.type.raw = ts_node_type(entry.node);
        ast_node.tree_position.parent_index = entry.parent_id;
        ast_node.tree_position.node_depth = entry.depth;
        ast_node.tree_position.sibling_index = entry.sibling_index;
        
        // Extract position
        TSPoint start = ts_node_start_point(entry.node);
        TSPoint end = ts_node_end_point(entry.node);
        ast_node.file_position.start_line = start.row + 1;
        ast_node.file_position.start_column = start.column + 1;
        ast_node.file_position.end_line = end.row + 1;
        ast_node.file_position.end_column = end.column + 1;
        
        // Extract name and value
        ast_node.name.raw = ExtractNodeName(entry.node, content);
        
        // Extract source text (peek)
        uint32_t start_byte = ts_node_start_byte(entry.node);
        uint32_t end_byte = ts_node_end_byte(entry.node);
        if (start_byte < content.size() && end_byte <= content.size()) {
            string source_text = content.substr(start_byte, end_byte - start_byte);
            ast_node.peek = source_text.length() > 120 ? source_text.substr(0, 120) : source_text;
        }
        
        // Apply taxonomy
        const NodeTypeConfig* config = GetNodeTypeConfig(ast_node.type.raw);
        if (config) {
            ast_node.kind = static_cast<uint8_t>(config->kind);
            ast_node.universal_flags = config->universal_flags;
            ast_node.super_type = config->super_type;
        }
        
        ast_node.arity_bin = ASTNode::BinArityFibonacci(ts_node_child_count(entry.node));
        ast_node.type.normalized = GetNormalizedType(ast_node.type.raw);
        ast_node.type.kind = ASTNode::GetKindName(static_cast<ASTKind>(ast_node.kind));
        
        nodes.push_back(ast_node);
        
        // Add children in reverse order for correct processing
        uint32_t child_count = ts_node_child_count(entry.node);
        for (int32_t i = child_count - 1; i >= 0; i--) {
            TSNode child = ts_node_child(entry.node, i);
            stack.push_back({child, ast_node.tree_position.node_index, entry.depth + 1, i});
        }
    }
    
    ts_tree_delete(tree);
}

//==============================================================================
// JavaScriptLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> JavaScriptLanguageHandler::type_mappings = {
    // Declarations
    {"function_declaration", NormalizedTypes::FUNCTION_DECLARATION},
    {"arrow_function", NormalizedTypes::FUNCTION_DECLARATION},
    {"function_expression", NormalizedTypes::FUNCTION_DECLARATION},
    {"class_declaration", NormalizedTypes::CLASS_DECLARATION},
    {"lexical_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"variable_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"const", NormalizedTypes::VARIABLE_DECLARATION},
    {"let", NormalizedTypes::VARIABLE_DECLARATION},
    {"var", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Method declarations
    {"method_definition", NormalizedTypes::METHOD_DECLARATION},
    
    // Expressions
    {"call_expression", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"string", NormalizedTypes::LITERAL},
    {"number", NormalizedTypes::LITERAL},
    {"true", NormalizedTypes::LITERAL},
    {"false", NormalizedTypes::LITERAL},
    {"null", NormalizedTypes::LITERAL},
    {"template_string", NormalizedTypes::LITERAL},
    {"binary_expression", NormalizedTypes::BINARY_EXPRESSION},
    
    // Control flow
    {"if_statement", NormalizedTypes::IF_STATEMENT},
    {"for_statement", NormalizedTypes::LOOP_STATEMENT},
    {"while_statement", NormalizedTypes::LOOP_STATEMENT},
    {"do_statement", NormalizedTypes::LOOP_STATEMENT},
    {"for_in_statement", NormalizedTypes::LOOP_STATEMENT},
    {"return_statement", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"comment", NormalizedTypes::COMMENT},
    {"import_statement", NormalizedTypes::IMPORT_STATEMENT},
    {"export_statement", NormalizedTypes::EXPORT_STATEMENT},
};

string JavaScriptLanguageHandler::GetLanguageName() const {
    return "javascript";
}

vector<string> JavaScriptLanguageHandler::GetAliases() const {
    return {"javascript", "js"};
}

void JavaScriptLanguageHandler::InitializeParser() const {
    parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    const TSLanguage *ts_language = tree_sitter_javascript();
    SetParserLanguageWithValidation(parser, ts_language, "JavaScript");
}

string JavaScriptLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string JavaScriptLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    // Extract names for declaration types
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION || 
        normalized == NormalizedTypes::CLASS_DECLARATION ||
        normalized == NormalizedTypes::METHOD_DECLARATION) {
        // For method_definition, look for property_identifier child
        if (node_type == "method_definition") {
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char* child_type = ts_node_type(child);
                if (strcmp(child_type, "property_identifier") == 0) {
                    return ExtractNodeText(child, content);
                }
            }
        }
        return FindIdentifierChild(node, content);
    } else if (normalized == NormalizedTypes::VARIABLE_REFERENCE) {
        return ExtractNodeText(node, content);
    } else if (node_type == "property_identifier") {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

string JavaScriptLanguageHandler::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // For literals, extract the actual value
    if (node_type == "string" || node_type == "number" || node_type == "template_string" ||
        node_type == "true" || node_type == "false" || node_type == "null") {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

bool JavaScriptLanguageHandler::IsPublicNode(TSNode node, const string &content) const {
    // For now, always return false - TODO: implement JS export detection
    return false;
}

const NodeTypeConfig* JavaScriptLanguageHandler::GetNodeTypeConfig(const string &node_type) const {
    // Simple if-chain approach for KIND mapping
    if (node_type == "function_declaration" || node_type == "arrow_function" || node_type == "function_expression") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "class_declaration") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 1, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "call_expression") {
        static NodeTypeConfig config(ASTKind::COMPUTATION, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    if (node_type == "identifier") {
        static NodeTypeConfig config(ASTKind::NAME, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "string" || node_type == "number" || node_type == "template_string") {
        static NodeTypeConfig config(ASTKind::LITERAL, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "if_statement") {
        static NodeTypeConfig config(ASTKind::FLOW_CONTROL, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    
    // Default fallback
    static NodeTypeConfig default_config(ASTKind::PARSER_SPECIFIC, 0, 0, HashMethod::Structural(), 0);
    return &default_config;
}

const LanguageConfig& JavaScriptLanguageHandler::GetConfig() const {
    // TODO: implement proper LanguageConfig - for now just throw
    throw NotImplementedException("LanguageConfig not implemented yet");
}

void JavaScriptLanguageHandler::ParseFile(const string &content, vector<ASTNode> &nodes) const {
    // TODO: Implement JavaScript-specific parsing - for now, throw
    throw NotImplementedException("JavaScript ParseFile not implemented yet");
}

//==============================================================================
// TypeScriptLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> TypeScriptLanguageHandler::type_mappings = {
    // Declarations (inherit from JavaScript)
    {"function_declaration", NormalizedTypes::FUNCTION_DECLARATION},
    {"arrow_function", NormalizedTypes::FUNCTION_DECLARATION},
    {"function_expression", NormalizedTypes::FUNCTION_DECLARATION},
    {"class_declaration", NormalizedTypes::CLASS_DECLARATION},
    {"interface_declaration", NormalizedTypes::CLASS_DECLARATION},
    {"type_alias_declaration", NormalizedTypes::CLASS_DECLARATION},
    {"enum_declaration", NormalizedTypes::CLASS_DECLARATION},
    {"lexical_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"variable_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Method declarations
    {"method_definition", NormalizedTypes::METHOD_DECLARATION},
    {"method_signature", NormalizedTypes::METHOD_DECLARATION},
    
    // Expressions
    {"call_expression", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"string", NormalizedTypes::LITERAL},
    {"number", NormalizedTypes::LITERAL},
    {"true", NormalizedTypes::LITERAL},
    {"false", NormalizedTypes::LITERAL},
    {"null", NormalizedTypes::LITERAL},
    
    // Control flow
    {"binary_expression", NormalizedTypes::BINARY_EXPRESSION},
    {"if_statement", NormalizedTypes::IF_STATEMENT},
    {"for_statement", NormalizedTypes::LOOP_STATEMENT},
    {"while_statement", NormalizedTypes::LOOP_STATEMENT},
    {"return_statement", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"comment", NormalizedTypes::COMMENT},
    {"import_statement", NormalizedTypes::IMPORT_STATEMENT},
    {"export_statement", NormalizedTypes::EXPORT_STATEMENT},
};

string TypeScriptLanguageHandler::GetLanguageName() const {
    return "typescript";
}

vector<string> TypeScriptLanguageHandler::GetAliases() const {
    return {"typescript", "ts"};
}

void TypeScriptLanguageHandler::InitializeParser() const {
    parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    const TSLanguage *ts_language = tree_sitter_typescript();
    SetParserLanguageWithValidation(parser, ts_language, "TypeScript");
}

string TypeScriptLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string TypeScriptLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    // Extract names for declaration types
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION || 
        normalized == NormalizedTypes::CLASS_DECLARATION ||
        normalized == NormalizedTypes::METHOD_DECLARATION) {
        // For method_definition, look for property_identifier child
        if (node_type == "method_definition") {
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char* child_type = ts_node_type(child);
                if (strcmp(child_type, "property_identifier") == 0) {
                    return ExtractNodeText(child, content);
                }
            }
        }
        return FindIdentifierChild(node, content);
    }
    
    return "";
}

string TypeScriptLanguageHandler::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // For literals, extract the actual value
    if (node_type == "string" || node_type == "number" || node_type == "template_string" ||
        node_type == "true" || node_type == "false" || node_type == "null") {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

bool TypeScriptLanguageHandler::IsPublicNode(TSNode node, const string &content) const {
    // For now, always return false - TODO: implement TypeScript visibility detection
    return false;
}

const NodeTypeConfig* TypeScriptLanguageHandler::GetNodeTypeConfig(const string &node_type) const {
    // Simple if-chain approach for KIND mapping
    if (node_type == "function_declaration" || node_type == "arrow_function" || node_type == "function_expression") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "class_declaration" || node_type == "interface_declaration" || 
        node_type == "type_alias_declaration" || node_type == "enum_declaration") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 1, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "call_expression") {
        static NodeTypeConfig config(ASTKind::COMPUTATION, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    if (node_type == "identifier") {
        static NodeTypeConfig config(ASTKind::NAME, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "string" || node_type == "number" || node_type == "template_string") {
        static NodeTypeConfig config(ASTKind::LITERAL, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "if_statement") {
        static NodeTypeConfig config(ASTKind::FLOW_CONTROL, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    
    // Default fallback
    static NodeTypeConfig default_config(ASTKind::PARSER_SPECIFIC, 0, 0, HashMethod::Structural(), 0);
    return &default_config;
}

const LanguageConfig& TypeScriptLanguageHandler::GetConfig() const {
    // TODO: implement proper LanguageConfig - for now just throw
    throw NotImplementedException("LanguageConfig not implemented yet");
}

void TypeScriptLanguageHandler::ParseFile(const string &content, vector<ASTNode> &nodes) const {
    // TODO: Implement TypeScript-specific parsing - for now, throw
    throw NotImplementedException("TypeScript ParseFile not implemented yet");
}

//==============================================================================
// CPPLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> CPPLanguageHandler::type_mappings = {
    // Declarations
    {"function_definition", NormalizedTypes::FUNCTION_DECLARATION},
    {"class_specifier", NormalizedTypes::CLASS_DECLARATION},
    {"struct_specifier", NormalizedTypes::CLASS_DECLARATION},
    {"declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"field_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"parameter_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Method declarations
    {"function_definition", NormalizedTypes::METHOD_DECLARATION}, // When inside a class
    {"field_declaration", NormalizedTypes::METHOD_DECLARATION}, // Method declarations in class
    
    // Expressions
    {"call_expression", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"field_expression", NormalizedTypes::VARIABLE_REFERENCE},
    {"string_literal", NormalizedTypes::LITERAL},
    {"number_literal", NormalizedTypes::LITERAL},
    {"true", NormalizedTypes::LITERAL},
    {"false", NormalizedTypes::LITERAL},
    {"null", NormalizedTypes::LITERAL},
    {"nullptr", NormalizedTypes::LITERAL},
    {"binary_expression", NormalizedTypes::BINARY_EXPRESSION},
    
    // Control flow
    {"if_statement", NormalizedTypes::IF_STATEMENT},
    {"for_statement", NormalizedTypes::LOOP_STATEMENT},
    {"while_statement", NormalizedTypes::LOOP_STATEMENT},
    {"do_statement", NormalizedTypes::LOOP_STATEMENT},
    {"for_range_loop", NormalizedTypes::LOOP_STATEMENT},
    {"return_statement", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"comment", NormalizedTypes::COMMENT},
    {"preproc_include", NormalizedTypes::IMPORT_STATEMENT},
    {"using_declaration", NormalizedTypes::IMPORT_STATEMENT},
};

string CPPLanguageHandler::GetLanguageName() const {
    return "cpp";
}

vector<string> CPPLanguageHandler::GetAliases() const {
    return {"cpp", "c++", "cxx", "cc", "hpp"};
}

void CPPLanguageHandler::InitializeParser() const {
    parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    const TSLanguage *ts_language = tree_sitter_cpp();
    SetParserLanguageWithValidation(parser, ts_language, "C++");
}

string CPPLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string CPPLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    // Extract names for declaration types
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION || 
        normalized == NormalizedTypes::CLASS_DECLARATION ||
        normalized == NormalizedTypes::METHOD_DECLARATION) {
        
        // For function_definition, ALWAYS look for function_declarator first
        if (node_type == "function_definition") {
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                if (strcmp(ts_node_type(child), "function_declarator") == 0) {
                    // Look for identifier or field_identifier within declarator
                    uint32_t declarator_child_count = ts_node_child_count(child);
                    for (uint32_t j = 0; j < declarator_child_count; j++) {
                        TSNode declarator_child = ts_node_child(child, j);
                        const char* declarator_child_type = ts_node_type(declarator_child);
                        if (strcmp(declarator_child_type, "identifier") == 0 || 
                            strcmp(declarator_child_type, "field_identifier") == 0) {
                            return ExtractNodeText(declarator_child, content);
                        }
                    }
                }
            }
        } else {
            // For other types (class_specifier, etc), use original logic
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char* child_type = ts_node_type(child);
                if (strcmp(child_type, "identifier") == 0 || 
                   strcmp(child_type, "type_identifier") == 0) {
                    return ExtractNodeText(child, content);
                }
            }
        }
    } else if (normalized == NormalizedTypes::VARIABLE_REFERENCE) {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

string CPPLanguageHandler::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // For literals, extract the actual value
    if (node_type == "string_literal" || node_type == "number_literal" ||
        node_type == "true" || node_type == "false" || node_type == "null" || node_type == "nullptr") {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

bool CPPLanguageHandler::IsPublicNode(TSNode node, const string &content) const {
    // For now, always return false - TODO: implement C++ public/private detection
    return false;
}

const NodeTypeConfig* CPPLanguageHandler::GetNodeTypeConfig(const string &node_type) const {
    // Simple if-chain approach for KIND mapping
    if (node_type == "function_definition") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "class_specifier" || node_type == "struct_specifier") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 1, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "call_expression") {
        static NodeTypeConfig config(ASTKind::COMPUTATION, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    if (node_type == "identifier") {
        static NodeTypeConfig config(ASTKind::NAME, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "string_literal" || node_type == "number_literal") {
        static NodeTypeConfig config(ASTKind::LITERAL, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "if_statement") {
        static NodeTypeConfig config(ASTKind::FLOW_CONTROL, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    
    // Default fallback
    static NodeTypeConfig default_config(ASTKind::PARSER_SPECIFIC, 0, 0, HashMethod::Structural(), 0);
    return &default_config;
}

const LanguageConfig& CPPLanguageHandler::GetConfig() const {
    // TODO: implement proper LanguageConfig - for now just throw
    throw NotImplementedException("LanguageConfig not implemented yet");
}

void CPPLanguageHandler::ParseFile(const string &content, vector<ASTNode> &nodes) const {
    // TODO: Implement C++-specific parsing - for now, throw
    throw NotImplementedException("C++ ParseFile not implemented yet");
}

//==============================================================================
// RustLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> RustLanguageHandler::type_mappings = {
    // Declarations
    {"function_item", NormalizedTypes::FUNCTION_DECLARATION},
    {"struct_item", NormalizedTypes::CLASS_DECLARATION},
    {"enum_item", NormalizedTypes::CLASS_DECLARATION},
    {"trait_item", NormalizedTypes::CLASS_DECLARATION},
    {"impl_item", NormalizedTypes::CLASS_DECLARATION},
    {"mod_item", NormalizedTypes::CLASS_DECLARATION},
    {"let_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"const_item", NormalizedTypes::VARIABLE_DECLARATION},
    {"static_item", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Expressions  
    {"call_expression", NormalizedTypes::FUNCTION_CALL},
    {"method_call_expression", NormalizedTypes::FUNCTION_CALL},
    {"macro_invocation", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"field_identifier", NormalizedTypes::VARIABLE_REFERENCE},
    
    // Literals
    {"integer_literal", NormalizedTypes::LITERAL},
    {"float_literal", NormalizedTypes::LITERAL},
    {"string_literal", NormalizedTypes::LITERAL},
    {"char_literal", NormalizedTypes::LITERAL},
    {"boolean_literal", NormalizedTypes::LITERAL},
    {"raw_string_literal", NormalizedTypes::LITERAL},
    
    // Control flow
    {"binary_expression", NormalizedTypes::BINARY_EXPRESSION},
    {"if_expression", NormalizedTypes::IF_STATEMENT},
    {"match_expression", NormalizedTypes::IF_STATEMENT},
    {"while_expression", NormalizedTypes::LOOP_STATEMENT},
    {"loop_expression", NormalizedTypes::LOOP_STATEMENT},
    {"for_expression", NormalizedTypes::LOOP_STATEMENT},
    {"return_expression", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"line_comment", NormalizedTypes::COMMENT},
    {"block_comment", NormalizedTypes::COMMENT},
    {"use_declaration", NormalizedTypes::IMPORT_STATEMENT},
    {"extern_crate_declaration", NormalizedTypes::IMPORT_STATEMENT},
};

string RustLanguageHandler::GetLanguageName() const {
    return "rust";
}

vector<string> RustLanguageHandler::GetAliases() const {
    return {"rust", "rs"};
}

void RustLanguageHandler::InitializeParser() const {
    parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    // TODO: Re-enable when Rust grammar is working
    //const TSLanguage* ts_language = tree_sitter_rust();
    //SetParserLanguageWithValidation(parser, ts_language, "Rust");
}

string RustLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string RustLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION) {
        // For function_item, look for identifier child
        return FindIdentifierChild(node, content);
    } else if (normalized == NormalizedTypes::CLASS_DECLARATION) {
        // For struct_item, enum_item, trait_item, etc., look for type_identifier
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "identifier") == 0) {
                return ExtractNodeText(child, content);
            }
        }
    } else if (normalized == NormalizedTypes::VARIABLE_REFERENCE) {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

string RustLanguageHandler::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // For literals, extract the actual value
    if (node_type == "string_literal" || node_type == "integer_literal" || node_type == "float_literal" ||
        node_type == "char_literal" || node_type == "boolean_literal" || node_type == "raw_string_literal") {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

bool RustLanguageHandler::IsPublicNode(TSNode node, const string &content) const {
    // For now, always return false - TODO: implement Rust pub detection
    return false;
}

const NodeTypeConfig* RustLanguageHandler::GetNodeTypeConfig(const string &node_type) const {
    // Simple if-chain approach for KIND mapping
    if (node_type == "function_item") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "struct_item" || node_type == "enum_item" || node_type == "trait_item") {
        static NodeTypeConfig config(ASTKind::DEFINITION, 1, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "call_expression" || node_type == "method_call_expression") {
        static NodeTypeConfig config(ASTKind::COMPUTATION, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    if (node_type == "identifier") {
        static NodeTypeConfig config(ASTKind::NAME, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "string_literal" || node_type == "integer_literal" || node_type == "float_literal") {
        static NodeTypeConfig config(ASTKind::LITERAL, 0, 0, HashMethod::Literal(), 0);
        return &config;
    }
    if (node_type == "if_expression") {
        static NodeTypeConfig config(ASTKind::FLOW_CONTROL, 0, 0, HashMethod::Structural(), 0);
        return &config;
    }
    
    // Default fallback
    static NodeTypeConfig default_config(ASTKind::PARSER_SPECIFIC, 0, 0, HashMethod::Structural(), 0);
    return &default_config;
}

const LanguageConfig& RustLanguageHandler::GetConfig() const {
    // TODO: implement proper LanguageConfig - for now just throw
    throw NotImplementedException("LanguageConfig not implemented yet");
}

void RustLanguageHandler::ParseFile(const string &content, vector<ASTNode> &nodes) const {
    // TODO: Implement Rust-specific parsing - for now, throw
    throw NotImplementedException("Rust ParseFile not implemented yet");
}

//==============================================================================
// LanguageHandlerRegistry implementation
//==============================================================================

LanguageHandlerRegistry::LanguageHandlerRegistry() {
    InitializeDefaultHandlers();
}

LanguageHandlerRegistry& LanguageHandlerRegistry::GetInstance() {
    static LanguageHandlerRegistry instance;
    return instance;
}

void LanguageHandlerRegistry::RegisterHandler(unique_ptr<LanguageHandler> handler) {
    // Validate ABI compatibility before registering
    ValidateLanguageABI(handler.get());
    
    string language = handler->GetLanguageName();
    vector<string> aliases = handler->GetAliases();
    
    // Register all aliases
    for (const auto &alias : aliases) {
        alias_to_language[alias] = language;
    }
    
    handlers[language] = std::move(handler);
}

const LanguageHandler* LanguageHandlerRegistry::GetHandler(const string &language) const {
    // First try direct lookup
    auto it = handlers.find(language);
    if (it != handlers.end()) {
        return it->second.get();
    }
    
    // Try alias lookup
    auto alias_it = alias_to_language.find(language);
    if (alias_it != alias_to_language.end()) {
        auto handler_it = handlers.find(alias_it->second);
        if (handler_it != handlers.end()) {
            return handler_it->second.get();
        }
    }
    
    return nullptr;
}

vector<string> LanguageHandlerRegistry::GetSupportedLanguages() const {
    vector<string> languages;
    for (const auto &pair : handlers) {
        languages.push_back(pair.first);
    }
    return languages;
}

void LanguageHandlerRegistry::ValidateLanguageABI(const LanguageHandler* handler) const {
    // Test parser initialization to validate ABI compatibility
    try {
        // Create a temporary handler to test parser creation
        auto test_handler = const_cast<LanguageHandler*>(handler);
        TSParser* parser = test_handler->GetParser();
        if (!parser) {
            throw InvalidInputException("Language handler for '" + handler->GetLanguageName() + 
                                       "' failed to create parser");
        }
        // Parser initialization validates ABI compatibility automatically
    } catch (const std::exception& e) {
        throw InvalidInputException("Language handler for '" + handler->GetLanguageName() + 
                                   "' failed validation: " + e.what());
    }
}

void LanguageHandlerRegistry::InitializeDefaultHandlers() {
    RegisterHandler(make_uniq<PythonLanguageHandler>());
    RegisterHandler(make_uniq<JavaScriptLanguageHandler>());
    RegisterHandler(make_uniq<CPPLanguageHandler>());
    RegisterHandler(make_uniq<TypeScriptLanguageHandler>());
    // Temporarily disabled due to ABI compatibility issues:
    // RegisterHandler(make_uniq<RustLanguageHandler>());
}

} // namespace duckdb
