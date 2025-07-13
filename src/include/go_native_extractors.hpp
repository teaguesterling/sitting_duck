#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Go-Specific Native Context Extractors
//==============================================================================

// Forward declaration for GoAdapter
class GoAdapter;

// Base template for Go extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct GoNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (Go functions)
template<>
struct GoNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract return type(s) (Go functions can have multiple return values)
        context.signature_type = ExtractGoReturnType(node, content);
        
        // Extract function parameters with Go type annotations
        context.parameters = ExtractGoParameters(node, content);
        
        // Extract function modifiers (receiver, etc.)
        auto modifiers = ExtractGoModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractGoReturnType(TSNode node, const string& content) {
        // In Go, return types come AFTER the parameter list
        // Structure: func name(params) returnType { }
        //         or func name(params) (multipleReturns) { }
        uint32_t child_count = ts_node_child_count(node);
        bool found_input_params = false;
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip until we find the first parameter_list (input parameters)
            if (!found_input_params && strcmp(child_type, "parameter_list") == 0) {
                found_input_params = true;
                continue; // This is the input parameters, keep looking for return type
            }
            
            // After input parameter list, look for return types
            if (found_input_params) {
                if (strcmp(child_type, "type_identifier") == 0 ||
                    strcmp(child_type, "primitive_type") == 0 ||
                    strcmp(child_type, "pointer_type") == 0 ||
                    strcmp(child_type, "slice_type") == 0 ||
                    strcmp(child_type, "array_type") == 0 ||
                    strcmp(child_type, "map_type") == 0 ||
                    strcmp(child_type, "channel_type") == 0 ||
                    strcmp(child_type, "interface_type") == 0 ||
                    strcmp(child_type, "struct_type") == 0 ||
                    strcmp(child_type, "qualified_type") == 0) {
                    // Single return type
                    uint32_t start = ts_node_start_byte(child);
                    uint32_t end = ts_node_end_byte(child);
                    if (start < content.length() && end <= content.length() && end > start) {
                        return content.substr(start, end - start);
                    }
                } else if (strcmp(child_type, "parameter_list") == 0) {
                    // Multiple return types: (int, error) - this is a second parameter_list for returns
                    uint32_t start = ts_node_start_byte(child);
                    uint32_t end = ts_node_end_byte(child);
                    if (start < content.length() && end <= content.length() && end > start) {
                        return content.substr(start, end - start);
                    }
                }
            }
        }
        return ""; // No return type (void equivalent)
    }
    
    static vector<ParameterInfo> ExtractGoParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // In Go functions, we need to extract parameters carefully:
        // - func name(params) returnType
        // - func (receiver) name(params) returnType  
        // - func name(params) (multipleReturns)
        // We should ONLY extract input parameters, not return types
        
        uint32_t child_count = ts_node_child_count(node);
        bool found_identifier = false;
        bool extracted_receiver = false;
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Track when we see the function identifier
            if (strcmp(child_type, "identifier") == 0) {
                found_identifier = true;
                continue;
            }
            
            if (strcmp(child_type, "parameter_list") == 0) {
                if (!found_identifier && !extracted_receiver) {
                    // This parameter_list comes before the identifier, so it's a receiver
                    auto receiver_params = ExtractGoParametersDirect(child, content);
                    for (auto& param : receiver_params) {
                        param.name = "(" + param.name + " " + param.type + ")"; // Format as receiver
                        param.type = "receiver";
                        params.push_back(param);
                    }
                    extracted_receiver = true;
                } else if (found_identifier) {
                    // This parameter_list comes after the identifier
                    // Check if this is input parameters or return parameters
                    // If there are more parameter_lists after this one, this is input params
                    bool has_more_param_lists = false;
                    for (uint32_t j = i + 1; j < child_count; j++) {
                        TSNode future_child = ts_node_child(node, j);
                        if (strcmp(ts_node_type(future_child), "parameter_list") == 0) {
                            has_more_param_lists = true;
                            break;
                        }
                    }
                    
                    // This is input parameters (either first param list after identifier,
                    // or the only param list after identifier)
                    auto input_params = ExtractGoParametersDirect(child, content);
                    for (const auto& param : input_params) {
                        params.push_back(param);
                    }
                    break; // Stop after extracting input parameters
                }
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractGoParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "parameter_declaration") == 0) {
                // Standard parameter: name Type or just Type
                // In Go, this might be grouped: param1, param2 int
                auto grouped_params = ExtractGoParameterDeclaration(child, content);
                if (!grouped_params.empty()) {
                    for (const auto& param : grouped_params) {
                        parameters.push_back(param);
                    }
                }
            } else if (strcmp(child_type, "variadic_parameter") == 0) {
                // Variadic parameter: ...Type
                ParameterInfo param = ExtractGoVariadicParameter(child, content);
                if (!param.type.empty()) {
                    parameters.push_back(param);
                }
            }
        }
        
        return parameters;
    }
    
    static vector<ParameterInfo> ExtractGoParameterDeclaration(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        vector<string> param_names;
        string param_type;
        
        uint32_t child_count = ts_node_child_count(node);
        
        // First pass: collect all identifiers (parameter names) and find the type
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                // Parameter name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param_names.push_back(content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "type_identifier") == 0 ||
                       strcmp(child_type, "primitive_type") == 0 ||
                       strcmp(child_type, "pointer_type") == 0 ||
                       strcmp(child_type, "slice_type") == 0 ||
                       strcmp(child_type, "array_type") == 0 ||
                       strcmp(child_type, "map_type") == 0 ||
                       strcmp(child_type, "channel_type") == 0 ||
                       strcmp(child_type, "interface_type") == 0 ||
                       strcmp(child_type, "struct_type") == 0) {
                // Parameter type
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param_type = content.substr(start, end - start);
                }
            }
        }
        
        // Create one ParameterInfo for each name with the shared type
        if (!param_type.empty()) {
            if (param_names.empty()) {
                // No names provided, only create unnamed parameter if we actually have a type
                // But don't create spurious parameters
                ParameterInfo param;
                param.type = param_type;
                param.name = ""; // Empty name for unnamed parameters
                params.push_back(param);
            } else {
                // Create one parameter for each name
                for (const string& name : param_names) {
                    ParameterInfo param;
                    param.name = name;
                    param.type = param_type;
                    params.push_back(param);
                }
            }
        }
        
        return params;
    }
    
    static ParameterInfo ExtractGoVariadicParameter(TSNode node, const string& content) {
        ParameterInfo param;
        param.is_variadic = true;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                // Parameter name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type_identifier") == 0 ||
                       strcmp(child_type, "primitive_type") == 0) {
                // Variadic type (the ...Type becomes []Type)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.type = "..." + content.substr(start, end - start);
                }
            }
        }
        
        if (param.name.empty() && !param.type.empty()) {
            param.name = "args"; // Default name for variadic
        }
        
        return param;
    }
    
    static vector<string> ExtractGoModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for receiver (method vs function)
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "parameter_list") == 0) {
                    // Check if this is a receiver (comes before func name)
                    // If the parameter_list comes before the identifier, it's a receiver
                    bool is_receiver = false;
                    for (uint32_t j = i + 1; j < parent_count; j++) {
                        TSNode next_sibling = ts_node_child(parent, j);
                        if (strcmp(ts_node_type(next_sibling), "identifier") == 0) {
                            is_receiver = true;
                            break;
                        }
                    }
                    
                    if (is_receiver) {
                        uint32_t start = ts_node_start_byte(sibling);
                        uint32_t end = ts_node_end_byte(sibling);
                        if (start < content.length() && end <= content.length()) {
                            modifiers.push_back("receiver" + content.substr(start, end - start));
                        }
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for CLASS_WITH_METHODS (Go structs and interfaces)
template<>
struct GoNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Determine if this is a struct, interface, or type alias
        const char* node_type = ts_node_type(node);
        if (strcmp(node_type, "struct_type") == 0) {
            context.signature_type = "struct";
        } else if (strcmp(node_type, "interface_type") == 0) {
            context.signature_type = "interface";
        } else if (strcmp(node_type, "type_declaration") == 0) {
            context.signature_type = "type";
        } else {
            context.signature_type = "type";
        }
        
        // Extract type modifiers
        auto modifiers = ExtractGoTypeModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static vector<string> ExtractGoTypeModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Go doesn't have many type modifiers, but we can extract embedded types
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "field_declaration_list") == 0) {
                // Check for embedded fields (anonymous fields)
                uint32_t field_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < field_count; j++) {
                    TSNode field = ts_node_child(child, j);
                    if (strcmp(ts_node_type(field), "field_declaration") == 0) {
                        // Check if this is an embedded field (no name, just type)
                        uint32_t field_child_count = ts_node_child_count(field);
                        if (field_child_count == 1) { // Only type, no name
                            uint32_t start = ts_node_start_byte(field);
                            uint32_t end = ts_node_end_byte(field);
                            if (start < content.length() && end <= content.length()) {
                                modifiers.push_back("embeds " + content.substr(start, end - start));
                            }
                        }
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Reuse CLASS_WITH_METHODS for CLASS_WITH_INHERITANCE
template<>
struct GoNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
    static NativeContext Extract(TSNode node, const string& content) {
        return GoNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
    }
};

// Specialization for VARIABLE_WITH_TYPE (Go variable declarations)
template<>
struct GoNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract Go variable type
        context.signature_type = ExtractGoVariableType(node, content);
        
        // Extract variable declaration modifiers (var, const, etc.)
        auto modifiers = ExtractGoVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractGoVariableType(TSNode node, const string& content) {
        // Look for type in variable declaration
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "slice_type") == 0 ||
                strcmp(child_type, "array_type") == 0 ||
                strcmp(child_type, "map_type") == 0 ||
                strcmp(child_type, "channel_type") == 0 ||
                strcmp(child_type, "interface_type") == 0 ||
                strcmp(child_type, "struct_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractGoVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check parent for declaration type
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode child = ts_node_child(parent, i);
                const char* child_type = ts_node_type(child);
                
                if (strcmp(child_type, "var") == 0 ||
                    strcmp(child_type, "const") == 0) {
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

// Specialization for FUNCTION_CALL (Go function calls)
template<>
struct GoNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<GoLanguageTag>::Extract(node, content);
    }
};

} // namespace duckdb