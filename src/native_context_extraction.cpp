#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>
#include <cstring>

namespace duckdb {

//==============================================================================
// Helper Functions Implementation
//==============================================================================

// Helper function to extract text from a specific child by type
string ExtractChildTextByType(TSNode node, const string& content, const string& child_type) {
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* current_type = ts_node_type(child);
        
        if (child_type == string(current_type)) {
            uint32_t start = ts_node_start_byte(child);
            uint32_t end = ts_node_end_byte(child);
            if (start < content.length() && end <= content.length()) {
                return content.substr(start, end - start);
            }
        }
    }
    return "";
}

// Helper function to extract all children of a specific type
vector<TSNode> FindChildrenByType(TSNode node, const string& child_type) {
    vector<TSNode> children;
    uint32_t child_count = ts_node_child_count(node);
    
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* current_type = ts_node_type(child);
        
        if (child_type == string(current_type)) {
            children.push_back(child);
        }
    }
    
    return children;
}

// Helper function to extract parameter list from common patterns
vector<ParameterInfo> ExtractParameterList(TSNode params_node, const string& content) {
    vector<ParameterInfo> parameters;
    
    uint32_t child_count = ts_node_child_count(params_node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(params_node, i);
        const char* child_type = ts_node_type(child);
        
        ParameterInfo param;
        bool is_valid_param = false;
        
        // Handle different parameter types
        if (strcmp(child_type, "identifier") == 0) {
            // Simple parameter: def func(param):
            param.name = ExtractNodeText(child, content);
            is_valid_param = true;
        } else if (strcmp(child_type, "typed_parameter") == 0) {
            // Typed parameter: def func(param: int):
            param.name = ExtractChildTextByType(child, content, "identifier");
            param.type = ExtractChildTextByType(child, content, "type");
            is_valid_param = true;
        } else if (strcmp(child_type, "default_parameter") == 0) {
            // Parameter with default: def func(param=default):
            param.name = ExtractChildTextByType(child, content, "identifier");
            param.is_optional = true;
            
            // Extract default value
            auto default_children = FindChildrenByType(child, "=");
            if (!default_children.empty() && ts_node_child_count(child) > 2) {
                TSNode default_value = ts_node_child(child, 2); // After identifier and =
                param.default_value = ExtractNodeText(default_value, content);
            }
            is_valid_param = true;
        } else if (strcmp(child_type, "typed_default_parameter") == 0) {
            // Typed parameter with default: def func(param: int = default):
            param.name = ExtractChildTextByType(child, content, "identifier");
            param.type = ExtractChildTextByType(child, content, "type");
            param.is_optional = true;
            
            // Extract default value (typically the last child)
            uint32_t param_child_count = ts_node_child_count(child);
            if (param_child_count > 0) {
                TSNode last_child = ts_node_child(child, param_child_count - 1);
                if (strcmp(ts_node_type(last_child), "=") != 0 && 
                    strcmp(ts_node_type(last_child), "type") != 0 &&
                    strcmp(ts_node_type(last_child), "identifier") != 0) {
                    param.default_value = ExtractNodeText(last_child, content);
                }
            }
            is_valid_param = true;
        } else if (strcmp(child_type, "list_splat_pattern") == 0 || strcmp(child_type, "*") == 0) {
            // Variadic parameter: def func(*args):
            param.name = ExtractChildTextByType(child, content, "identifier");
            if (param.name.empty()) {
                param.name = "*args"; // Default name
            } else {
                param.name = "*" + param.name;
            }
            param.is_variadic = true;
            is_valid_param = true;
        } else if (strcmp(child_type, "dictionary_splat_pattern") == 0 || strcmp(child_type, "**") == 0) {
            // Keyword variadic parameter: def func(**kwargs):
            param.name = ExtractChildTextByType(child, content, "identifier");
            if (param.name.empty()) {
                param.name = "**kwargs"; // Default name
            } else {
                param.name = "**" + param.name;
            }
            param.is_variadic = true;
            is_valid_param = true;
        }
        
        if (is_valid_param && !param.name.empty()) {
            parameters.push_back(param);
        }
    }
    
    return parameters;
}

// Helper function to extract modifiers from various patterns
vector<string> ExtractModifiersFromNode(TSNode node, const string& content) {
    vector<string> modifiers;
    
    // Look for common modifier patterns in the node's children
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);
        
        // Check for common modifiers
        if (strcmp(child_type, "async") == 0) {
            modifiers.push_back("async");
        } else if (strcmp(child_type, "static") == 0) {
            modifiers.push_back("static");
        } else if (strcmp(child_type, "public") == 0) {
            modifiers.push_back("public");
        } else if (strcmp(child_type, "private") == 0) {
            modifiers.push_back("private");
        } else if (strcmp(child_type, "protected") == 0) {
            modifiers.push_back("protected");
        }
    }
    
    return modifiers;
}

// Helper function to build qualified name from context
string BuildQualifiedName(TSNode node, const string& content, const string& base_name) {
    // For now, just return the base name
    // TODO: Implement full qualified name resolution by walking up the AST
    return base_name;
}

// Helper function to extract node text content
string ExtractNodeText(TSNode node, const string& content) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (start < content.length() && end <= content.length()) {
        return content.substr(start, end - start);
    }
    return "";
}

} // namespace duckdb