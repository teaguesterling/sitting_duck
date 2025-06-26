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
    
private:
    static string ExtractCppReturnType(TSNode node, const string& content) {
        // In C++, return type comes before function name
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "template_type") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "reference_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return "";
    }
    
    static vector<ParameterInfo> ExtractCppParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find parameter_list node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "parameter_list") == 0) {
                params = ExtractCppParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractCppParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
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
    
    static ParameterInfo ExtractParameterDeclaration(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "template_type") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "reference_type") == 0) {
                // Parameter type
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "identifier") == 0) {
                // Parameter name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "default_parameter_declaration") == 0) {
                // Parameter with default value
                param.is_optional = true;
                // Extract default value from this node
                uint32_t def_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < def_count; j++) {
                    TSNode def_child = ts_node_child(child, j);
                    const char* def_type = ts_node_type(def_child);
                    if (strcmp(def_type, "=") != 0) { // Skip the equals sign
                        uint32_t start = ts_node_start_byte(def_child);
                        uint32_t end = ts_node_end_byte(def_child);
                        if (start < content.length() && end <= content.length()) {
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