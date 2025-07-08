#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// TypeScript-Specific Native Context Extractors
//==============================================================================

// Base template for TypeScript extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct TypeScriptNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract return type if present (TypeScript return type annotations)
        context.signature_type = ExtractTypeScriptReturnType(node, content);
        
        // Extract function parameters with TypeScript type annotations
        context.parameters = ExtractTypeScriptParameters(node, content);
        
        // Extract access modifiers and decorators
        auto modifiers = ExtractTypeScriptModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static string ExtractTypeScriptReturnType(TSNode node, const string& content) {
        // Look for type annotation after : in function signature
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_annotation") == 0) {
                // Extract the type part
                uint32_t type_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < type_count; j++) {
                    TSNode type_child = ts_node_child(child, j);
                    const char* type_child_type = ts_node_type(type_child);
                    
                    if (strcmp(type_child_type, ":") != 0) { // Skip the colon
                        uint32_t start = ts_node_start_byte(type_child);
                        uint32_t end = ts_node_end_byte(type_child);
                        if (start < content.length() && end <= content.length()) {
                            return content.substr(start, end - start);
                        }
                    }
                }
            }
        }
        return ""; // No return type annotation
    }
    
    static vector<ParameterInfo> ExtractTypeScriptParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find formal_parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "formal_parameters") == 0) {
                params = ExtractTypeScriptParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractTypeScriptParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "required_parameter") == 0) {
                // Required parameter: (param: type)
                param = ExtractRequiredParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "optional_parameter") == 0) {
                // Optional parameter: (param?: type)
                param = ExtractOptionalParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "rest_parameter") == 0) {
                // Rest parameter: (...args: type[])
                param = ExtractRestParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "assignment_pattern") == 0) {
                // Parameter with default: (param: type = default)
                param = ExtractDefaultParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "identifier") == 0) {
                // Simple parameter fallback
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                    is_valid_param = true;
                }
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractRequiredParameter(TSNode node, const string& content) {
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
            } else if (strcmp(child_type, "type_annotation") == 0) {
                param.type = ExtractTypeFromAnnotation(child, content);
            } else if (strcmp(child_type, "accessibility_modifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.annotations = content.substr(start, end - start);
                }
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
            
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type_annotation") == 0) {
                param.type = ExtractTypeFromAnnotation(child, content);
            }
        }
        
        return param;
    }
    
    static ParameterInfo ExtractRestParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_variadic = true;
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
            } else if (strcmp(child_type, "type_annotation") == 0) {
                param.type = ExtractTypeFromAnnotation(child, content);
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
            } else if (strcmp(child_type, "type_annotation") == 0) {
                param.type = ExtractTypeFromAnnotation(child, content);
            } else if (i > 0 && strcmp(child_type, "identifier") != 0 && strcmp(child_type, "type_annotation") != 0) {
                // Default value
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
    static string ExtractTypeFromAnnotation(TSNode annotation_node, const string& content) {
        uint32_t child_count = ts_node_child_count(annotation_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(annotation_node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, ":") != 0) { // Skip the colon
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractTypeScriptModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for function modifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "accessibility_modifier") == 0 ||
                    strcmp(sibling_type, "readonly") == 0 ||
                    strcmp(sibling_type, "static") == 0 ||
                    strcmp(sibling_type, "abstract") == 0 ||
                    strcmp(sibling_type, "async") == 0) {
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

// Specialization for ARROW_FUNCTION 
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract arrow function parameters with TypeScript types
        context.parameters = ExtractArrowFunctionParameters(node, content);
        context.signature_type = ExtractArrowReturnType(node, content);
        if (context.signature_type.empty()) {
            context.signature_type = "arrow";
        }
        
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
                // Arrow function with parentheses: (a: string, b: number) => {}
                params = TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractTypeScriptParametersDirect(child, content);
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
    
    static string ExtractArrowReturnType(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_annotation") == 0) {
                return TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractTypeFromAnnotation(child, content);
            }
        }
        return "";
    }
};

// Specialization for ASYNC_FUNCTION
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        // Reuse FUNCTION_WITH_PARAMS logic and add async modifier
        auto context = TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
        context.modifiers.insert(context.modifiers.begin(), "async");
        return context;
    }
};

// Specialization for CLASS_WITH_METHODS and CLASS_WITH_INHERITANCE
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "class";
        
        // Extract class modifiers, extends, and implements
        auto modifiers = ExtractTypeScriptClassModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static vector<string> ExtractTypeScriptClassModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "class_heritage") == 0) {
                // Extract extends and implements clauses
                modifiers.push_back(ExtractClassHeritage(child, content));
            } else if (strcmp(child_type, "abstract") == 0 ||
                       strcmp(child_type, "export") == 0 ||
                       strcmp(child_type, "declare") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            }
        }
        
        return modifiers;
    }
    
    static string ExtractClassHeritage(TSNode heritage_node, const string& content) {
        uint32_t start = ts_node_start_byte(heritage_node);
        uint32_t end = ts_node_end_byte(heritage_node);
        if (start < content.length() && end <= content.length()) {
            return content.substr(start, end - start);
        }
        return "";
    }
};

// Reuse CLASS_WITH_METHODS for CLASS_WITH_INHERITANCE
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
    static NativeContext Extract(TSNode node, const string& content) {
        return TypeScriptNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
    }
};

// Specialization for VARIABLE_WITH_TYPE
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract TypeScript variable type annotation
        context.signature_type = ExtractVariableType(node, content);
        
        // Extract declaration modifiers (const, let, readonly, etc.)
        auto modifiers = ExtractVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static string ExtractVariableType(TSNode node, const string& content) {
        // Look for type_annotation in variable declarator
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_annotation") == 0) {
                return TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractTypeFromAnnotation(child, content);
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check parent for declaration type and modifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            TSNode grand_parent = ts_node_parent(parent);
            if (!ts_node_is_null(grand_parent)) {
                uint32_t gp_count = ts_node_child_count(grand_parent);
                for (uint32_t i = 0; i < gp_count; i++) {
                    TSNode child = ts_node_child(grand_parent, i);
                    const char* child_type = ts_node_type(child);
                    
                    if (strcmp(child_type, "const") == 0 ||
                        strcmp(child_type, "let") == 0 ||
                        strcmp(child_type, "var") == 0 ||
                        strcmp(child_type, "readonly") == 0 ||
                        strcmp(child_type, "export") == 0 ||
                        strcmp(child_type, "declare") == 0) {
                        uint32_t start = ts_node_start_byte(child);
                        uint32_t end = ts_node_end_byte(child);
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

// Specialization for CUSTOM (TypeScript function calls and expressions)
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::CUSTOM> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            const char* node_type = ts_node_type(node);
            
            if (strcmp(node_type, "call_expression") == 0) {
                context = ExtractTSCallExpression(node, content);
            } else if (strcmp(node_type, "new_expression") == 0) {
                context = ExtractTSNewExpression(node, content);
            } else {
                // Unknown node type for CUSTOM strategy
                context.signature_type = "";
            }
            
        } catch (...) {
            context.signature_type = "";
            context.parameters.clear();
            context.modifiers.clear();
        }
        
        return context;
    }
    
private:
    static NativeContext ExtractTSCallExpression(TSNode node, const string& content) {
        NativeContext context;
        
        // For call_expression: extract function name and arguments
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // First child is usually the function identifier or member expression
            if (i == 0) {
                if (strcmp(child_type, "identifier") == 0 ||
                    strcmp(child_type, "member_expression") == 0 ||
                    strcmp(child_type, "property_identifier") == 0) {
                    
                    uint32_t start = ts_node_start_byte(child);
                    uint32_t end = ts_node_end_byte(child);
                    
                    if (start < content.length() && end <= content.length() && end > start) {
                        context.signature_type = content.substr(start, end - start);
                    }
                }
            }
            
            // Extract arguments from arguments node
            if (strcmp(child_type, "arguments") == 0) {
                context.parameters = ExtractTSCallArguments(child, content);
            }
        }
        
        if (context.signature_type.empty()) {
            context.signature_type = "function_call";  // Fallback
        }
        
        return context;
    }
    
    static NativeContext ExtractTSNewExpression(TSNode node, const string& content) {
        NativeContext context;
        
        // For new_expression: extract class name and constructor arguments
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip "new" keyword, look for identifier
            if (strcmp(child_type, "identifier") == 0 ||
                strcmp(child_type, "member_expression") == 0 ||
                strcmp(child_type, "property_identifier") == 0) {
                
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                
                if (start < content.length() && end <= content.length() && end > start) {
                    context.signature_type = content.substr(start, end - start);
                }
            }
            
            // Extract constructor arguments from arguments node
            if (strcmp(child_type, "arguments") == 0) {
                context.parameters = ExtractTSCallArguments(child, content);
            }
        }
        
        if (context.signature_type.empty()) {
            context.signature_type = "constructor_call";  // Fallback
        }
        
        return context;
    }
    
    static vector<ParameterInfo> ExtractTSCallArguments(TSNode args_node, const string& content) {
        vector<ParameterInfo> arguments;
        
        uint32_t child_count = ts_node_child_count(args_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(args_node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip punctuation like "," and "(" and ")"
            if (strcmp(child_type, ",") == 0 || 
                strcmp(child_type, "(") == 0 || 
                strcmp(child_type, ")") == 0) {
                continue;
            }
            
            ParameterInfo arg;
            
            // Extract argument text as the "type" field
            uint32_t start = ts_node_start_byte(child);
            uint32_t end = ts_node_end_byte(child);
            
            if (start < content.length() && end <= content.length() && end > start) {
                arg.type = content.substr(start, end - start);
                arg.name = "";  // Arguments don't have names in calls
                arguments.push_back(arg);
            }
        }
        
        return arguments;
    }
};

} // namespace duckdb