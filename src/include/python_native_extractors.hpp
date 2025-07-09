#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Python-Specific Native Context Extractors
//==============================================================================

// Base template for Python extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct PythonNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // ALWAYS set fallback to verify this extractor is being called
        context.signature_type = "python_extractor_called";
        
        // Extract return type if present (for type-annotated functions)  
        string return_type = ExtractPythonReturnType(node, content);
        if (!return_type.empty()) {
            context.signature_type = return_type;
        }
        
        // Extract function parameters
        context.parameters = ExtractPythonParameters(node, content);
        
        // Extract decorators if present
        auto decorators = ExtractPythonDecorators(node, content);
        context.modifiers = decorators;
        
        return context;
    }
    
private:
    static string ExtractPythonReturnType(TSNode node, const string& content) {
        // Look for type annotation after -> in function definition
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "->") == 0 && i + 1 < child_count) {
                // Next child should be the return type
                TSNode type_node = ts_node_child(node, i + 1);
                uint32_t start = ts_node_start_byte(type_node);
                uint32_t end = ts_node_end_byte(type_node);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return ""; // No return type annotation
    }

    static vector<ParameterInfo> ExtractPythonParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "parameters") == 0) {
                // Extract each parameter from the parameters list
                params = ExtractPythonParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractPythonParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "identifier") == 0) {
                // Simple parameter: def func(param):
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                    is_valid_param = true;
                }
            } else if (strcmp(child_type, "typed_parameter") == 0) {
                // Typed parameter: def func(param: int):
                param = ExtractTypedParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "default_parameter") == 0) {
                // Parameter with default: def func(param=default):
                param = ExtractDefaultParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "typed_default_parameter") == 0) {
                // Typed parameter with default: def func(param: int = default):
                param = ExtractTypedDefaultParameter(child, content);
                is_valid_param = !param.name.empty();
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractTypedParameter(TSNode node, const string& content) {
        ParameterInfo param;
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
            } else if (strcmp(child_type, "type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            }
        }
        
        return param;
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
            } else if (strcmp(child_type, "=") != 0 && !param.name.empty()) {
                // This is likely the default value (skip the = sign)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractTypedDefaultParameter(TSNode node, const string& content) {
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
            } else if (strcmp(child_type, "type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "=") != 0 && 
                       strcmp(child_type, "identifier") != 0 &&
                       strcmp(child_type, "type") != 0 &&
                       strcmp(child_type, ":") != 0) {
                // This is likely the default value
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static vector<string> ExtractPythonDecorators(TSNode node, const string& content) {
        vector<string> decorators;
        
        // Check if this function has decorators (they appear as siblings before the function)
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t child_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(parent, i);
                const char* child_type = ts_node_type(child);
                
                if (strcmp(child_type, "decorator") == 0) {
                    // Extract decorator name
                    uint32_t start = ts_node_start_byte(child);
                    uint32_t end = ts_node_end_byte(child);
                    if (start < content.length() && end <= content.length()) {
                        string decorator_text = content.substr(start, end - start);
                        decorators.push_back(decorator_text);
                    }
                }
            }
        }
        
        return decorators;
    }
};

// Specialization for ASYNC_FUNCTION
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        // Reuse FUNCTION_WITH_PARAMS logic and add async modifier
        auto context = PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
        context.modifiers.insert(context.modifiers.begin(), "async");
        return context;
    }
};

// Specialization for CLASS_WITH_METHODS
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "class";
        
        // Extract base classes if present
        auto base_classes = ExtractPythonBaseClasses(node, content);
        if (!base_classes.empty()) {
            context.modifiers = base_classes;
        }
        
        return context;
    }
    
private:
    static vector<string> ExtractPythonBaseClasses(TSNode node, const string& content) {
        vector<string> base_classes;
        
        // Find argument_list node which contains base classes
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "argument_list") == 0) {
                // Extract base class names
                uint32_t arg_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < arg_count; j++) {
                    TSNode arg = ts_node_child(child, j);
                    if (strcmp(ts_node_type(arg), "identifier") == 0) {
                        uint32_t start = ts_node_start_byte(arg);
                        uint32_t end = ts_node_end_byte(arg);
                        if (start < content.length() && end <= content.length()) {
                            string base_class = content.substr(start, end - start);
                            base_classes.push_back(base_class);
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
struct PythonNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // For Python variable assignments, extract type annotation if present
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    context.signature_type = content.substr(start, end - start);
                }
                break;
            }
        }
        
        return context;
    }
};

// Specialization for FUNCTION_CALL (Python function calls)
template<>
struct PythonNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<PythonLanguageTag>::Extract(node, content);
    }
};

} // namespace duckdb