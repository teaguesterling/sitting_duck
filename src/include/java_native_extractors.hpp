#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Java-Specific Native Context Extractors
//==============================================================================

// Base template for Java extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct JavaNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (Java methods)
template<>
struct JavaNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract return type (Java methods have explicit return types)
        context.signature_type = ExtractJavaReturnType(node, content);
        
        // Extract method parameters with Java type annotations
        context.parameters = ExtractJavaParameters(node, content);
        
        // Extract access modifiers and other modifiers
        auto modifiers = ExtractJavaModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractJavaReturnType(TSNode node, const string& content) {
        // In Java, return type comes before method name
        // Structure: [modifiers] returnType methodName(params) { }
        uint32_t child_count = ts_node_child_count(node);
        
        // Look for return type node (can be anywhere in the method declaration)
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Look for any return type (primitive types like int, or reference types like String)
            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "generic_type") == 0 ||
                strcmp(child_type, "array_type") == 0 ||
                strcmp(child_type, "void_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length() && end > start) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        // If not found, check parent for return type (method_declaration structure)
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            
            for (uint32_t i = 0; i < parent_count && i < 20; i++) { // Limit search
                TSNode child = ts_node_child(parent, i);
                const char* child_type = ts_node_type(child);
                
                // Look for any return type in parent
                if (strcmp(child_type, "type_identifier") == 0 ||
                    strcmp(child_type, "primitive_type") == 0 ||
                    strcmp(child_type, "generic_type") == 0 ||
                    strcmp(child_type, "array_type") == 0 ||
                    strcmp(child_type, "void_type") == 0) {
                    uint32_t start = ts_node_start_byte(child);
                    uint32_t end = ts_node_end_byte(child);
                    if (start < content.length() && end <= content.length() && end > start) {
                        return content.substr(start, end - start);
                    }
                }
            }
        }
        
        return "";
    }
    
    static vector<ParameterInfo> ExtractJavaParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find formal_parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "formal_parameters") == 0) {
                params = ExtractJavaParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractJavaParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "formal_parameter") == 0) {
                // Standard parameter: (Type param)
                param = ExtractFormalParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "spread_parameter") == 0) {
                // Varargs parameter: (Type... args)
                param = ExtractSpreadParameter(child, content);
                is_valid_param = !param.name.empty();
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractFormalParameter(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "generic_type") == 0 ||
                strcmp(child_type, "array_type") == 0) {
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
            } else if (strcmp(child_type, "modifiers") == 0) {
                // Parameter modifiers (final, etc.)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.annotations = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractSpreadParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_variadic = true;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "generic_type") == 0) {
                // Varargs type (without the ...)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start) + "[]";
                }
            } else if (strcmp(child_type, "identifier") == 0) {
                // Parameter name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static vector<string> ExtractJavaModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // First check within the node itself for modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "modifiers") == 0) {
                // Extract individual modifiers
                uint32_t mod_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < mod_count; j++) {
                    TSNode modifier = ts_node_child(child, j);
                    const char* modifier_type = ts_node_type(modifier);
                    uint32_t start = ts_node_start_byte(modifier);
                    uint32_t end = ts_node_end_byte(modifier);
                    if (start < content.length() && end <= content.length() && end > start) {
                        modifiers.push_back(content.substr(start, end - start));
                    }
                }
            } else if (strcmp(child_type, "annotation") == 0) {
                // Java annotations
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length() && end > start) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            }
        }
        
        // If no modifiers found, check parent (but avoid ts_node_parent() reliability issues)
        if (modifiers.empty()) {
            TSNode parent = ts_node_parent(node);
            if (!ts_node_is_null(parent)) {
                uint32_t parent_count = ts_node_child_count(parent);
                for (uint32_t i = 0; i < parent_count && i < 10; i++) { // Limit search
                    TSNode sibling = ts_node_child(parent, i);
                    const char* sibling_type = ts_node_type(sibling);
                    
                    if (strcmp(sibling_type, "modifiers") == 0) {
                        // Extract individual modifiers
                        uint32_t mod_count = ts_node_child_count(sibling);
                        for (uint32_t j = 0; j < mod_count && j < 20; j++) { // Limit search
                            TSNode modifier = ts_node_child(sibling, j);
                            uint32_t start = ts_node_start_byte(modifier);
                            uint32_t end = ts_node_end_byte(modifier);
                            if (start < content.length() && end <= content.length() && end > start) {
                                modifiers.push_back(content.substr(start, end - start));
                            }
                        }
                    } else if (strcmp(sibling_type, "annotation") == 0) {
                        // Java annotations
                        uint32_t start = ts_node_start_byte(sibling);
                        uint32_t end = ts_node_end_byte(sibling);
                        if (start < content.length() && end <= content.length() && end > start) {
                            modifiers.push_back(content.substr(start, end - start));
                        }
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for CLASS_WITH_METHODS and CLASS_WITH_INHERITANCE
template<>
struct JavaNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "class";
        
        // Extract class modifiers, extends, and implements
        auto modifiers = ExtractJavaClassModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static vector<string> ExtractJavaClassModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "modifiers") == 0) {
                // Extract class modifiers (public, abstract, final, etc.)
                uint32_t mod_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < mod_count; j++) {
                    TSNode modifier = ts_node_child(child, j);
                    uint32_t start = ts_node_start_byte(modifier);
                    uint32_t end = ts_node_end_byte(modifier);
                    if (start < content.length() && end <= content.length()) {
                        modifiers.push_back(content.substr(start, end - start));
                    }
                }
            } else if (strcmp(child_type, "superclass") == 0) {
                // Extract extends clause
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "super_interfaces") == 0) {
                // Extract implements clause
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "annotation") == 0) {
                // Class annotations
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

// Reuse CLASS_WITH_METHODS for CLASS_WITH_INHERITANCE
template<>
struct JavaNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
    static NativeContext Extract(TSNode node, const string& content) {
        return JavaNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
    }
};

// Specialization for VARIABLE_WITH_TYPE (Java field declarations)
template<>
struct JavaNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract Java variable type
        context.signature_type = ExtractJavaVariableType(node, content);
        
        // Extract field modifiers
        auto modifiers = ExtractJavaVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractJavaVariableType(TSNode node, const string& content) {
        // Look for type in variable declarator
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "generic_type") == 0 ||
                strcmp(child_type, "array_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractJavaVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check parent for field modifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode child = ts_node_child(parent, i);
                const char* child_type = ts_node_type(child);
                
                if (strcmp(child_type, "modifiers") == 0) {
                    // Extract field modifiers (public, private, static, final, etc.)
                    uint32_t mod_count = ts_node_child_count(child);
                    for (uint32_t j = 0; j < mod_count; j++) {
                        TSNode modifier = ts_node_child(child, j);
                        uint32_t start = ts_node_start_byte(modifier);
                        uint32_t end = ts_node_end_byte(modifier);
                        if (start < content.length() && end <= content.length()) {
                            modifiers.push_back(content.substr(start, end - start));
                        }
                    }
                } else if (strcmp(child_type, "annotation") == 0) {
                    // Field annotations
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

// Specialization for FUNCTION_CALL (Java method invocations and object creation)
template<>
struct JavaNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<JavaLanguageTag>::Extract(node, content);
    }
};

} // namespace duckdb