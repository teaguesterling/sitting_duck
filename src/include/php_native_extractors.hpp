#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
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
            
            if (strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "return_type") == 0) {
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
            } else if (strcmp(child_type, "primitive_type") == 0 ||
                       strcmp(child_type, "type_declaration") == 0 ||
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
            } else if (strcmp(child_type, "primitive_type") == 0 ||
                       strcmp(child_type, "type_declaration") == 0) {
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
            } else if (strcmp(child_type, "primitive_type") == 0 ||
                       strcmp(child_type, "type_declaration") == 0) {
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
            } else if (strcmp(child_type, "primitive_type") == 0 ||
                       strcmp(child_type, "type_declaration") == 0) {
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

        // Determine class type
        const char* node_type = ts_node_type(node);
        if (strcmp(node_type, "interface_declaration") == 0) {
            context.signature_type = "interface";
        } else if (strcmp(node_type, "trait_declaration") == 0) {
            context.signature_type = "trait";
        } else if (strcmp(node_type, "enum_declaration") == 0) {
            context.signature_type = "enum";
        } else {
            context.signature_type = ExtractClassType(node, content);
        }

        // Extract parent types into parameters
        bool has_extends = false;
        bool has_implements = false;
        context.parameters = ExtractParentTypes(node, content, has_extends, has_implements);
        context.modifiers = ExtractPHPClassModifiers(node, content, has_extends, has_implements);

        return context;
    }

public:
    static string ExtractClassType(TSNode node, const string& content) {
        // Check for abstract or final class
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "abstract_modifier") == 0) {
                return "abstract_class";
            } else if (strcmp(child_type, "final_modifier") == 0) {
                return "final_class";
            } else if (strcmp(child_type, "readonly_modifier") == 0) {
                return "readonly_class";
            }
        }
        return "class";
    }

    // Extract parent types from base_clause (extends) and class_interface_clause (implements)
    static vector<ParameterInfo> ExtractParentTypes(TSNode node, const string& content,
                                                     bool& has_extends, bool& has_implements) {
        vector<ParameterInfo> parents;
        has_extends = false;
        has_implements = false;

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "base_clause") == 0) {
                // extends clause - extract parent class
                has_extends = true;
                uint32_t base_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < base_count; j++) {
                    TSNode base_child = ts_node_child(child, j);
                    const char* base_type = ts_node_type(base_child);

                    if (strcmp(base_type, "name") == 0 ||
                        strcmp(base_type, "qualified_name") == 0) {
                        string type_name = ExtractNodeText(base_child, content);
                        if (!type_name.empty()) {
                            parents.push_back({type_name, "extends"});
                        }
                    }
                }
            } else if (strcmp(child_type, "class_interface_clause") == 0) {
                // implements clause - extract interfaces
                has_implements = true;
                uint32_t impl_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < impl_count; j++) {
                    TSNode impl_child = ts_node_child(child, j);
                    const char* impl_type = ts_node_type(impl_child);

                    // Skip "implements" keyword and punctuation
                    if (strcmp(impl_type, "implements") == 0 ||
                        strcmp(impl_type, ",") == 0) {
                        continue;
                    }

                    if (strcmp(impl_type, "name") == 0 ||
                        strcmp(impl_type, "qualified_name") == 0) {
                        string type_name = ExtractNodeText(impl_child, content);
                        if (!type_name.empty()) {
                            parents.push_back({type_name, "implements"});
                        }
                    }
                }
            }
        }

        return parents;
    }

    static vector<string> ExtractPHPClassModifiers(TSNode node, const string& content,
                                                    bool has_extends, bool has_implements) {
        vector<string> modifiers;

        // Note: inheritance kind is now in ParameterInfo.type, not in modifiers

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "abstract_modifier") == 0 ||
                strcmp(child_type, "final_modifier") == 0 ||
                strcmp(child_type, "readonly_modifier") == 0) {
                string mod = ExtractNodeText(child, content);
                if (!mod.empty()) {
                    modifiers.push_back(mod);
                }
            }
        }

        return modifiers;
    }

    static string ExtractNodeText(TSNode node, const string& content) {
        if (ts_node_is_null(node)) return "";

        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);

        if (start < content.length() && end <= content.length() && end > start) {
            return content.substr(start, end - start);
        }

        return "";
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

// Specialization for FUNCTION_CALL (PHP function calls and method calls)
template<>
struct PHPNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<PHPLanguageTag>::Extract(node, content);
    }
};

} // namespace duckdb