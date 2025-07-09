#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// R-Specific Native Context Extractors  
//==============================================================================

// Forward declaration for RAdapter
class RAdapter;

// Base template for R extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct RNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (R functions)
template<>
struct RNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            // R functions don't have explicit return type annotations
            // Default to empty string (becomes NULL in output)
            context.signature_type = "";
            
            // Extract function parameters
            context.parameters = ExtractRParameters(node, content);
            
            // Extract function attributes and visibility
            auto modifiers = ExtractRModifiers(node, content);
            context.modifiers = modifiers;
            
        } catch (...) {
            context.signature_type = "";
            context.parameters.clear();
            context.modifiers.clear();
        }
        
        return context;
    }
    
private:
    static vector<ParameterInfo> ExtractRParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return params;
        }
        
        // Find parameters node within function_definition
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
            
            if (strcmp(child_type, "parameters") == 0) {
                params = ExtractRParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractRParametersDirect(TSNode params_node, const string& content) {
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
            
            if (strcmp(child_type, "parameter") == 0) {
                // Standard parameter: name or name = default
                param = ExtractRParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "dots") == 0) {
                // R's ... parameter for variadic functions
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
    
    static ParameterInfo ExtractRParameter(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count to prevent infinite loops
        if (child_count > 100) {
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
            
            if (strcmp(child_type, "identifier") == 0) {
                // Parameter name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                
                // Safety check: ensure bounds are valid
                if (start < content.length() && end <= content.length() && end > start) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "=") == 0 && i + 1 < child_count) {
                // Parameter has default value, next child is the default
                param.is_optional = true;
                TSNode default_child = ts_node_child(node, i + 1);
                
                if (!ts_node_is_null(default_child)) {
                    uint32_t start = ts_node_start_byte(default_child);
                    uint32_t end = ts_node_end_byte(default_child);
                    
                    // Safety check: ensure bounds are valid
                    if (start < content.length() && end <= content.length() && end > start) {
                        param.default_value = content.substr(start, end - start);
                    }
                }
            }
        }
        
        return param;
    }
    
    static vector<string> ExtractRModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for function visibility based on naming convention
        // In R, functions starting with '.' are considered private
        string function_name = ExtractRFunctionName(node, content);
        if (!function_name.empty() && function_name[0] == '.') {
            modifiers.push_back("private");
        } else if (!function_name.empty()) {
            modifiers.push_back("public");
        }
        
        // Check for special function types (could be extended for S3/S4 methods)
        if (function_name.find(".") != string::npos && function_name[0] != '.') {
            // Potential S3 method (generic.class pattern)
            modifiers.push_back("s3_method");
        }
        
        return modifiers;
    }
    
    static string ExtractRFunctionName(TSNode node, const string& content) {
        // R function definitions are typically: name <- function(params) { body }
        // We need to look at the assignment target
        
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "identifier") == 0) {
                    uint32_t start = ts_node_start_byte(sibling);
                    uint32_t end = ts_node_end_byte(sibling);
                    if (start < content.length() && end <= content.length()) {
                        return content.substr(start, end - start);
                    }
                }
            }
        }
        
        return "";
    }
};

// Specialization for VARIABLE_WITH_TYPE (R variable assignments)
template<>
struct RNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // R is dynamically typed, so no explicit type annotations
        // But we can infer assignment operator used
        context.signature_type = ExtractRAssignmentType(node, content);
        
        // Extract assignment modifiers
        auto modifiers = ExtractRVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractRAssignmentType(TSNode node, const string& content) {
        // Check what assignment operator was used
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode child = ts_node_child(parent, i);
                const char* child_type = ts_node_type(child);
                
                if (strcmp(child_type, "<-") == 0) {
                    return "local_assign";
                } else if (strcmp(child_type, "<<-") == 0) {
                    return "global_assign";
                } else if (strcmp(child_type, "=") == 0) {
                    return "equal_assign";
                } else if (strcmp(child_type, "->") == 0) {
                    return "right_assign";
                } else if (strcmp(child_type, "->>") == 0) {
                    return "global_right_assign";
                }
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractRVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for variable naming conventions
        string var_name = ExtractRVariableName(node, content);
        if (!var_name.empty()) {
            if (var_name[0] == '.') {
                modifiers.push_back("private");
            }
            if (isupper(var_name[0])) {
                modifiers.push_back("constant_style");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractRVariableName(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return "";
    }
};

// Specialization for FUNCTION_CALL (R function calls)
template<>
struct RNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<RLanguageTag>::Extract(node, content);
    }
};

} // namespace duckdb