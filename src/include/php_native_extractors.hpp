#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// PHP-Specific Native Context Extractors
//==============================================================================

// Forward declaration for PHPAdapter
class PHPAdapter;

// Base template for PHP extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct PHPNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (PHP functions)
template<>
struct PHPNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract return type if present (PHP 7+ return type declarations)
        context.signature_type = ExtractPHPReturnType(node, content);
        
        // Extract function parameters with PHP type hints
        context.parameters = ExtractPHPParameters(node, content);
        
        // Extract function modifiers (public, private, static, abstract, etc.)
        auto modifiers = ExtractPHPModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static string ExtractPHPReturnType(TSNode node, const string& content) {
        // Look for return type after : in function signature
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "return_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "union_type") == 0 ||
                       strcmp(child_type, "intersection_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return ""; // No return type annotation
    }
    
    static vector<ParameterInfo> ExtractPHPParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find formal_parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "formal_parameters") == 0) {
                params = ExtractPHPParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractPHPParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "simple_parameter") == 0) {
                // Simple parameter: $param or Type $param
                param = ExtractSimpleParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "optional_parameter") == 0) {
                // Optional parameter: $param = default
                param = ExtractOptionalParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "variadic_parameter") == 0) {
                // Variadic parameter: ...$args
                param = ExtractVariadicParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "property_promotion_parameter") == 0) {
                // PHP 8 constructor property promotion: public $param
                param = ExtractPropertyPromotionParameter(child, content);
                is_valid_param = !param.name.empty();
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractSimpleParameter(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "variable_name") == 0) {
                // Parameter name (includes $)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type_declaration") == 0 ||
                       strcmp(child_type, "union_type") == 0 ||
                       strcmp(child_type, "intersection_type") == 0) {
                // Parameter type hint
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "reference_modifier") == 0) {
                // Reference parameter: &$param
                param.annotations = "reference";
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractOptionalParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_optional = true;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "variable_name") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type_declaration") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            } else if (i > 0 && strcmp(child_type, "=") != 0) {
                // Default value (skip the = sign)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractVariadicParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_variadic = true;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "variable_name") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type_declaration") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractPropertyPromotionParameter(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "variable_name") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type_declaration") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "visibility_modifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.annotations = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static vector<string> ExtractPHPModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for function modifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "visibility_modifier") == 0 ||
                    strcmp(sibling_type, "static_modifier") == 0 ||
                    strcmp(sibling_type, "abstract_modifier") == 0 ||
                    strcmp(sibling_type, "final_modifier") == 0) {
                    uint32_t start = ts_node_start_byte(sibling);
                    uint32_t end = ts_node_end_byte(sibling);
                    if (start < content.length() && end <= content.length()) {
                        modifiers.push_back(content.substr(start, end - start));
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for CLASS_WITH_METHODS (PHP classes)
template<>
struct PHPNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "class";
        
        // Extract class modifiers and inheritance
        auto modifiers = ExtractPHPClassModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static vector<string> ExtractPHPClassModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "class_interface_clause") == 0) {
                // implements clause
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back("implements " + content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "base_clause") == 0) {
                // extends clause
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back("extends " + content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "abstract_modifier") == 0 ||
                       strcmp(child_type, "final_modifier") == 0) {
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

// Specialization for VARIABLE_WITH_TYPE (PHP variable declarations)
template<>
struct PHPNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract PHP variable type (type hints)
        context.signature_type = ExtractPHPVariableType(node, content);
        
        // Extract variable modifiers (public, private, static, etc.)
        auto modifiers = ExtractPHPVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static string ExtractPHPVariableType(TSNode node, const string& content) {
        // Look for type declaration in property or variable
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_declaration") == 0 ||
                strcmp(child_type, "union_type") == 0 ||
                strcmp(child_type, "intersection_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractPHPVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for property modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "visibility_modifier") == 0 ||
                strcmp(child_type, "static_modifier") == 0 ||
                strcmp(child_type, "readonly_modifier") == 0) {
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

} // namespace duckdb