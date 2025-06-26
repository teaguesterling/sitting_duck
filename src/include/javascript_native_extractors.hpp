#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// JavaScript-Specific Native Context Extractors
//==============================================================================

// Base template for JavaScript extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct JavaScriptNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS
template<>
struct JavaScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract function parameters
        context.parameters = ExtractJavaScriptParameters(node, content);
        
        // JavaScript functions don't have explicit return type annotations (pre-TypeScript)
        context.signature_type = ""; 
        
        return context;
    }
    
public:
    static vector<ParameterInfo> ExtractJavaScriptParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find formal_parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "formal_parameters") == 0) {
                params = ExtractJavaScriptParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractJavaScriptParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "identifier") == 0) {
                // Simple parameter: function func(param) {}
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                    is_valid_param = true;
                }
            } else if (strcmp(child_type, "assignment_pattern") == 0) {
                // Parameter with default: function func(param = default) {}
                param = ExtractDefaultParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "rest_pattern") == 0) {
                // Rest parameter: function func(...args) {}
                param = ExtractRestParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "object_pattern") == 0) {
                // Destructuring parameter: function func({a, b}) {}
                param = ExtractDestructuringParameter(child, content);
                is_valid_param = !param.name.empty();
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractDefaultParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_optional = true;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (i > 0) { // Skip the identifier, get the default value
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractRestParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_variadic = true;
        
        // Rest pattern contains an identifier
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = "..." + content.substr(start, end - start);
                }
                break;
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractDestructuringParameter(TSNode node, const string& content) {
        ParameterInfo param;
        
        // For destructuring, use the full pattern as the name
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        if (start < content.length() && end <= content.length()) {
            param.name = content.substr(start, end - start);
        }
        
        return param;
    }
};

// Specialization for ARROW_FUNCTION
template<>
struct JavaScriptNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract arrow function parameters
        context.parameters = ExtractArrowFunctionParameters(node, content);
        context.signature_type = "arrow";
        
        return context;
    }
    
public:
    static vector<ParameterInfo> ExtractArrowFunctionParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "formal_parameters") == 0) {
                // Arrow function with parentheses: (a, b) => {}
                params = JavaScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractJavaScriptParametersDirect(child, content);
                break;
            } else if (strcmp(child_type, "identifier") == 0) {
                // Single parameter arrow function: a => {}
                ParameterInfo param;
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                    params.push_back(param);
                }
                break;
            }
        }
        
        return params;
    }
};

// Specialization for ASYNC_FUNCTION
template<>
struct JavaScriptNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        // Reuse FUNCTION_WITH_PARAMS logic and add async modifier
        auto context = JavaScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
        context.modifiers.push_back("async");
        return context;
    }
};

// Specialization for CLASS_WITH_METHODS
template<>
struct JavaScriptNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "class";
        
        // Extract extends clause if present
        auto base_classes = ExtractJavaScriptBaseClasses(node, content);
        if (!base_classes.empty()) {
            context.modifiers = base_classes;
        }
        
        return context;
    }
    
public:
    static vector<string> ExtractJavaScriptBaseClasses(TSNode node, const string& content) {
        vector<string> base_classes;
        
        // Find class_heritage (extends clause)
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "class_heritage") == 0) {
                // Extract identifier from extends clause
                uint32_t heritage_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < heritage_count; j++) {
                    TSNode heritage_child = ts_node_child(child, j);
                    if (strcmp(ts_node_type(heritage_child), "identifier") == 0) {
                        uint32_t start = ts_node_start_byte(heritage_child);
                        uint32_t end = ts_node_end_byte(heritage_child);
                        if (start < content.length() && end <= content.length()) {
                            string base_class = content.substr(start, end - start);
                            base_classes.push_back("extends " + base_class);
                        }
                    }
                }
                break;
            }
        }
        
        return base_classes;
    }
};

// Specialization for VARIABLE_WITH_TYPE  
template<>
struct JavaScriptNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // JavaScript variables don't have explicit types (pre-TypeScript)
        // But we can detect declaration patterns
        context.signature_type = ExtractDeclarationType(node, content);
        
        return context;
    }
    
public:
    static string ExtractDeclarationType(TSNode node, const string& content) {
        // Check if this is const, let, or var declaration
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            const char* parent_type = ts_node_type(parent);
            if (strcmp(parent_type, "lexical_declaration") == 0) {
                // Check for const/let
                uint32_t child_count = ts_node_child_count(parent);
                for (uint32_t i = 0; i < child_count; i++) {
                    TSNode child = ts_node_child(parent, i);
                    const char* child_type = ts_node_type(child);
                    if (strcmp(child_type, "const") == 0) {
                        return "const";
                    } else if (strcmp(child_type, "let") == 0) {
                        return "let";
                    }
                }
            } else if (strcmp(parent_type, "variable_declaration") == 0) {
                return "var";
            }
        }
        
        return "";
    }
};

} // namespace duckdb