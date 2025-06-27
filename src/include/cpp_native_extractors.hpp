#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// C++ Native Context Extractors
//==============================================================================

// Forward declaration for CppAdapter
class CppAdapter;

// Base template for C++ extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct CppNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (C++ functions/methods)
template<>
struct CppNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract return type (C++ functions have explicit return types)
        context.signature_type = ExtractCppReturnType(node, content);
        
        // Extract function parameters with C++ type annotations
        context.parameters = ExtractCppParameters(node, content);
        
        // Extract function specifiers and qualifiers
        auto modifiers = ExtractCppModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
    // Public helper method accessible to other extractors
    static ParameterInfo ExtractParameterDeclaration(TSNode node, const string& content) {
        ParameterInfo param;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return param;
        }
        
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count to prevent infinite loops
        if (child_count > 1000) {
            return param;
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            
            // Safety check: ensure child node is valid
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            // Safety check: ensure child_type is not null
            if (!child_type) {
                continue;
            }
            
            if (strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "template_type") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "reference_type") == 0) {
                // Parameter type
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                
                // Safety check: ensure bounds are valid
                if (start < content.length() && end <= content.length() && end > start) {
                    param.type = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "identifier") == 0) {
                // Parameter name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                // Safety check: ensure bounds are valid
                if (start < content.length() && end <= content.length() && end > start) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "default_parameter_declaration") == 0) {
                // Parameter with default value
                param.is_optional = true;
                // Extract default value from this node
                uint32_t def_count = ts_node_child_count(child);
                
                // Safety check: reasonable limit on default value children
                if (def_count > 100) {
                    continue;
                }
                
                for (uint32_t j = 0; j < def_count; j++) {
                    TSNode def_child = ts_node_child(child, j);
                    
                    // Safety check: ensure child node is valid
                    if (ts_node_is_null(def_child)) {
                        continue;
                    }
                    
                    const char* def_type = ts_node_type(def_child);
                    
                    // Safety check: ensure def_type is not null
                    if (!def_type) {
                        continue;
                    }
                    
                    if (strcmp(def_type, "=") != 0) { // Skip the equals sign
                        uint32_t start = ts_node_start_byte(def_child);
                        uint32_t end = ts_node_end_byte(def_child);
                        
                        // Safety check: ensure bounds are valid
                        if (start < content.length() && end <= content.length() && end > start) {
                            param.default_value = content.substr(start, end - start);
                        }
                    }
                }
            } else if (strcmp(child_type, "storage_class_specifier") == 0 ||
                       strcmp(child_type, "type_qualifier") == 0) {
                // Parameter qualifiers (const, volatile, etc.)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    if (!param.annotations.empty()) {
                        param.annotations += " ";
                    }
                    param.annotations += content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
private:
    static string ExtractCppReturnType(TSNode node, const string& content) {
        // In C++, the return type appears as a direct child before the function_declarator
        // For function_definition nodes, look for type nodes before function_declarator
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Stop when we reach the function_declarator - return type comes before it
            if (strcmp(child_type, "function_declarator") == 0) {
                break;
            }
            
            // Look for various type constructs
            if (strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "template_type") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "reference_type") == 0 ||
                strcmp(child_type, "auto") == 0 ||
                strcmp(child_type, "const") == 0 ||
                strcmp(child_type, "static") == 0) {
                
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                
                if (start < content.length() && end <= content.length() && end > start) {
                    string type_text = content.substr(start, end - start);
                    // Skip modifiers like "static", "const" that are not the actual return type
                    if (type_text != "static" && type_text != "const" && type_text != "inline" && 
                        type_text != "virtual" && type_text != "extern") {
                        return type_text;
                    }
                }
            }
        }
        
        // If no explicit return type found, it might be a constructor/destructor
        return "";
    }
    
    static vector<ParameterInfo> ExtractCppParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return params;
        }
        
        // For C++, parameters are nested: function_definition -> function_declarator -> parameter_list
        // First find the function_declarator
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 1000) {
            return params;
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            
            // Safety check: ensure child node is valid
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            // Safety check: ensure child_type is not null
            if (!child_type) {
                continue;
            }
            
            if (strcmp(child_type, "function_declarator") == 0) {
                // Now look for parameter_list within the function_declarator
                uint32_t declarator_child_count = ts_node_child_count(child);
                
                // Safety check: reasonable limit on child count
                if (declarator_child_count > 1000) {
                    continue;
                }
                
                for (uint32_t j = 0; j < declarator_child_count; j++) {
                    TSNode param_child = ts_node_child(child, j);
                    
                    // Safety check: ensure child node is valid
                    if (ts_node_is_null(param_child)) {
                        continue;
                    }
                    
                    const char* param_child_type = ts_node_type(param_child);
                    
                    // Safety check: ensure child_type is not null
                    if (!param_child_type) {
                        continue;
                    }
                    
                    if (strcmp(param_child_type, "parameter_list") == 0) {
                        params = ExtractCppParametersDirect(param_child, content);
                        return params; // Found parameters, return immediately
                    }
                }
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractCppParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(params_node)) {
            return parameters;
        }
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        // Safety check: reasonable limit on parameter count
        if (child_count > 1000) {
            return parameters;
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            
            // Safety check: ensure child node is valid
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            // Safety check: ensure child_type is not null
            if (!child_type) {
                continue;
            }
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "parameter_declaration") == 0) {
                // Standard parameter: (Type param) or (Type param = default)
                param = ExtractParameterDeclaration(child, content);
                is_valid_param = !param.name.empty() || !param.type.empty();
            } else if (strcmp(child_type, "variadic_parameter") == 0) {
                // Variadic parameter: (...)
                param.is_variadic = true;
                param.name = "...";
                param.type = "variadic";
                is_valid_param = true;
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static vector<string> ExtractCppModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for function specifiers and qualifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "storage_class_specifier") == 0 ||
                    strcmp(sibling_type, "type_qualifier") == 0 ||
                    strcmp(sibling_type, "function_specifier") == 0) {
                    uint32_t start = ts_node_start_byte(sibling);
                    uint32_t end = ts_node_end_byte(sibling);
                    if (start < content.length() && end <= content.length()) {
                        modifiers.push_back(content.substr(start, end - start));
                    }
                }
            }
        }
        
        // Check for trailing specifiers (const, noexcept, etc.)
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "trailing_return_type") == 0 ||
                strcmp(child_type, "noexcept") == 0 ||
                strcmp(child_type, "const") == 0 ||
                strcmp(child_type, "override") == 0 ||
                strcmp(child_type, "final") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for CLASS_WITH_METHODS and CLASS_WITH_INHERITANCE 
template<>
struct CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "class";
        
        // Extract class specifiers and inheritance
        auto modifiers = ExtractCppClassModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static vector<string> ExtractCppClassModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "base_class_clause") == 0) {
                // Extract inheritance information
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "class_specifier") == 0 ||
                       strcmp(child_type, "struct_specifier") == 0) {
                // Extract class/struct keyword
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            }
        }
        
        // Check for template parameters
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "template_declaration") == 0) {
                    uint32_t start = ts_node_start_byte(sibling);
                    uint32_t end = ts_node_end_byte(sibling);
                    if (start < content.length() && end <= content.length()) {
                        modifiers.push_back("template");
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Reuse CLASS_WITH_METHODS for CLASS_WITH_INHERITANCE
template<>
struct CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
    static NativeContext Extract(TSNode node, const string& content) {
        return CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
    }
};

// Specialization for ARROW_FUNCTION (C++ lambda expressions)
template<>
struct CppNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract lambda return type (often inferred, may be empty)
        context.signature_type = ExtractLambdaReturnType(node, content);
        
        // Extract lambda parameters (if present)
        context.parameters = ExtractLambdaParameters(node, content);
        
        // Extract lambda capture list and modifiers
        auto modifiers = ExtractLambdaModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractLambdaReturnType(TSNode node, const string& content) {
        // Lambda return types are often inferred in C++, but may be explicitly specified
        // Look for trailing return type syntax: []() -> ReturnType
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            if (!child_type) {
                continue;
            }
            
            if (strcmp(child_type, "trailing_return_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        // Most lambdas have inferred return types
        return "";
    }
    
    static vector<ParameterInfo> ExtractLambdaParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return params;
        }
        
        // Look for parameter_list in lambda_expression
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 1000) {
            return params;
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            
            // Safety check: ensure child node is valid
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            // Safety check: ensure child_type is not null
            if (!child_type) {
                continue;
            }
            
            if (strcmp(child_type, "parameter_list") == 0) {
                // Use the same parameter extraction logic as regular functions
                uint32_t param_child_count = ts_node_child_count(child);
                
                // Safety check: reasonable limit on parameter count
                if (param_child_count > 1000) {
                    return params;
                }
                
                for (uint32_t j = 0; j < param_child_count; j++) {
                    TSNode param_child = ts_node_child(child, j);
                    
                    // Safety check: ensure child node is valid
                    if (ts_node_is_null(param_child)) {
                        continue;
                    }
                    
                    const char* param_child_type = ts_node_type(param_child);
                    
                    // Safety check: ensure child_type is not null
                    if (!param_child_type) {
                        continue;
                    }
                    
                    if (strcmp(param_child_type, "parameter_declaration") == 0) {
                        ParameterInfo param = ExtractParameterDeclaration(param_child, content);
                        if (!param.name.empty() || !param.type.empty()) {
                            params.push_back(param);
                        }
                    }
                }
                break;
            }
        }
        
        return params;
    }
    
    static vector<string> ExtractLambdaModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Look for lambda capture list and other lambda-specific modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            if (!child_type) {
                continue;
            }
            
            if (strcmp(child_type, "lambda_capture_specifier") == 0 ||
                strcmp(child_type, "lambda_default_capture") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            }
        }
        
        return modifiers;
    }
    
    // Reuse the parameter declaration extraction from FUNCTION_WITH_PARAMS
    static ParameterInfo ExtractParameterDeclaration(TSNode node, const string& content) {
        return CppNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractParameterDeclaration(node, content);
    }
};

// Specialization for VARIABLE_WITH_TYPE (C++ variable declarations)
template<>
struct CppNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract C++ variable type
        context.signature_type = ExtractCppVariableType(node, content);
        
        // Extract variable specifiers and qualifiers
        auto modifiers = ExtractCppVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractCppVariableType(TSNode node, const string& content) {
        // Look for type in variable declaration
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "template_type") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "reference_type") == 0 ||
                strcmp(child_type, "auto") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractCppVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check parent for variable specifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode child = ts_node_child(parent, i);
                const char* child_type = ts_node_type(child);
                
                if (strcmp(child_type, "storage_class_specifier") == 0 ||
                    strcmp(child_type, "type_qualifier") == 0 ||
                    strcmp(child_type, "constexpr") == 0 ||
                    strcmp(child_type, "thread_local") == 0) {
                    uint32_t start = ts_node_start_byte(child);
                    uint32_t end = ts_node_end_byte(child);
                    if (start < content.length() && end <= content.length()) {
                        modifiers.push_back(content.substr(start, end - start));
                    }
                }
            }
        }
        
        return modifiers;
    }
};

} // namespace duckdb