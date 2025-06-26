#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Kotlin-Specific Native Context Extractors
//==============================================================================

// Forward declaration for KotlinAdapter
class KotlinAdapter;

// Base template for Kotlin extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct KotlinNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (Kotlin functions)
template<>
struct KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract return type (Kotlin has strong type system with nullable types)
        context.signature_type = ExtractKotlinReturnType(node, content);
        
        // Extract function parameters with Kotlin type annotations
        context.parameters = ExtractKotlinParameters(node, content);
        
        // Extract function modifiers (public, private, suspend, inline, etc.)
        auto modifiers = ExtractKotlinModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static string ExtractKotlinReturnType(TSNode node, const string& content) {
        // Look for return type after : in function signature
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type") == 0 ||
                strcmp(child_type, "nullable_type") == 0 ||
                strcmp(child_type, "user_type") == 0 ||
                strcmp(child_type, "function_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return "Unit"; // Default return type in Kotlin
    }
    
    static vector<ParameterInfo> ExtractKotlinParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find function_value_parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "function_value_parameters") == 0) {
                params = ExtractKotlinParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractKotlinParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "function_value_parameter") == 0) {
                // Standard parameter: name: Type or name: Type = default
                param = ExtractKotlinParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "vararg_parameter") == 0) {
                // Vararg parameter: vararg args: Type
                param = ExtractKotlinVarargParameter(child, content);
                is_valid_param = !param.name.empty();
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractKotlinParameter(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "simple_identifier") == 0) {
                // Parameter name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type") == 0 ||
                       strcmp(child_type, "nullable_type") == 0 ||
                       strcmp(child_type, "user_type") == 0) {
                // Parameter type
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "default_value") == 0) {
                // Default value
                param.is_optional = true;
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "parameter_modifier") == 0) {
                // crossinline, noinline, vararg
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.annotations = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractKotlinVarargParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_variadic = true;
        param.annotations = "vararg";
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "simple_identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type") == 0 ||
                       strcmp(child_type, "user_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static vector<string> ExtractKotlinModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for function modifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "modifiers") == 0) {
                    // Extract individual modifiers
                    uint32_t mod_count = ts_node_child_count(sibling);
                    for (uint32_t j = 0; j < mod_count; j++) {
                        TSNode modifier = ts_node_child(sibling, j);
                        const char* mod_type = ts_node_type(modifier);
                        
                        if (strcmp(mod_type, "visibility_modifier") == 0 ||
                            strcmp(mod_type, "function_modifier") == 0 ||
                            strcmp(mod_type, "member_modifier") == 0 ||
                            strcmp(mod_type, "parameter_modifier") == 0) {
                            uint32_t start = ts_node_start_byte(modifier);
                            uint32_t end = ts_node_end_byte(modifier);
                            if (start < content.length() && end <= content.length()) {
                                modifiers.push_back(content.substr(start, end - start));
                            }
                        }
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for ARROW_FUNCTION (Kotlin lambdas)
template<>
struct KotlinNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "lambda";
        
        // Extract lambda parameters
        context.parameters = ExtractKotlinLambdaParameters(node, content);
        
        return context;
    }
    
public:
    static vector<ParameterInfo> ExtractKotlinLambdaParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find lambda_parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "lambda_parameters") == 0) {
                params = KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
};

// Specialization for CLASS_WITH_METHODS (Kotlin classes, interfaces, objects)
template<>
struct KotlinNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Determine if this is a class, interface, object, or enum
        const char* node_type = ts_node_type(node);
        if (strcmp(node_type, "class_declaration") == 0) {
            context.signature_type = "class";
        } else if (strcmp(node_type, "interface_declaration") == 0) {
            context.signature_type = "interface";
        } else if (strcmp(node_type, "object_declaration") == 0) {
            context.signature_type = "object";
        } else if (strcmp(node_type, "enum_class_declaration") == 0) {
            context.signature_type = "enum";
        } else {
            context.signature_type = "type";
        }
        
        // Extract inheritance and implementation information
        auto modifiers = ExtractKotlinClassModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static vector<string> ExtractKotlinClassModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "delegation_specifiers") == 0) {
                // : SuperClass(), Interface1, Interface2
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back("inherits " + content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "modifiers") == 0) {
                // Extract class modifiers
                uint32_t mod_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < mod_count; j++) {
                    TSNode modifier = ts_node_child(child, j);
                    const char* mod_type = ts_node_type(modifier);
                    
                    if (strcmp(mod_type, "visibility_modifier") == 0 ||
                        strcmp(mod_type, "class_modifier") == 0 ||
                        strcmp(mod_type, "member_modifier") == 0) {
                        uint32_t start = ts_node_start_byte(modifier);
                        uint32_t end = ts_node_end_byte(modifier);
                        if (start < content.length() && end <= content.length()) {
                            modifiers.push_back(content.substr(start, end - start));
                        }
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for VARIABLE_WITH_TYPE (Kotlin property declarations)
template<>
struct KotlinNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract Kotlin property type (nullable types supported)
        context.signature_type = ExtractKotlinPropertyType(node, content);
        
        // Extract property modifiers (var/val, access level, etc.)
        auto modifiers = ExtractKotlinPropertyModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static string ExtractKotlinPropertyType(TSNode node, const string& content) {
        // Look for type annotation in property declaration
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type") == 0 ||
                strcmp(child_type, "nullable_type") == 0 ||
                strcmp(child_type, "user_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return ""; // Type inferred
    }
    
    static vector<string> ExtractKotlinPropertyModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for property declaration modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "modifiers") == 0) {
                // Extract individual modifiers
                uint32_t mod_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < mod_count; j++) {
                    TSNode modifier = ts_node_child(child, j);
                    const char* mod_type = ts_node_type(modifier);
                    
                    if (strcmp(mod_type, "visibility_modifier") == 0 ||
                        strcmp(mod_type, "member_modifier") == 0 ||
                        strcmp(mod_type, "property_modifier") == 0) {
                        uint32_t start = ts_node_start_byte(modifier);
                        uint32_t end = ts_node_end_byte(modifier);
                        if (start < content.length() && end <= content.length()) {
                            modifiers.push_back(content.substr(start, end - start));
                        }
                    }
                }
            }
        }
        
        // Check if this is var or val
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            const char* parent_type = ts_node_type(parent);
            if (strcmp(parent_type, "property_declaration") == 0) {
                // Check for var/val keyword
                uint32_t parent_count = ts_node_child_count(parent);
                for (uint32_t i = 0; i < parent_count; i++) {
                    TSNode sibling = ts_node_child(parent, i);
                    const char* sibling_type = ts_node_type(sibling);
                    
                    if (strcmp(sibling_type, "var") == 0 || strcmp(sibling_type, "val") == 0) {
                        modifiers.push_back(sibling_type);
                        break;
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for ASYNC_FUNCTION (Kotlin suspend functions)
template<>
struct KotlinNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "suspend";
        
        // Extract parameters and return type (similar to regular functions)
        context.parameters = KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinParameters(node, content);
        
        // Extract suspend and other modifiers
        auto modifiers = ExtractKotlinSuspendModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static vector<string> ExtractKotlinSuspendModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Look for suspend keyword
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "suspend") == 0) {
                modifiers.push_back("suspend");
                break;
            }
        }
        
        // Also get regular function modifiers
        auto regular_modifiers = KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinModifiers(node, content);
        modifiers.insert(modifiers.end(), regular_modifiers.begin(), regular_modifiers.end());
        
        return modifiers;
    }
};

// Specialization for GENERIC_FUNCTION (Kotlin generic functions)
template<>
struct KotlinNativeExtractor<NativeExtractionStrategy::GENERIC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "generic";
        
        // Extract parameters and return type
        context.parameters = KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinParameters(node, content);
        
        // Extract generic type parameters
        auto modifiers = ExtractKotlinGenericModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static vector<string> ExtractKotlinGenericModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Look for type_parameters
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_parameters") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back("generics " + content.substr(start, end - start));
                }
                break;
            }
        }
        
        // Also get regular function modifiers
        auto regular_modifiers = KotlinNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractKotlinModifiers(node, content);
        modifiers.insert(modifiers.end(), regular_modifiers.begin(), regular_modifiers.end());
        
        return modifiers;
    }
};

} // namespace duckdb