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
        // Look for return type after parameter list
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Go return types can be single types or parameter lists for multiple returns
            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "slice_type") == 0 ||
                strcmp(child_type, "array_type") == 0 ||
                strcmp(child_type, "map_type") == 0 ||
                strcmp(child_type, "channel_type") == 0 ||
                strcmp(child_type, "interface_type") == 0 ||
                strcmp(child_type, "struct_type") == 0) {
                // Single return type
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "parameter_list") == 0) {
                // Multiple return types: (int, error)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return ""; // No return type (void equivalent)
    }
    
    static vector<ParameterInfo> ExtractGoParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find parameter_list node  
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "parameter_list") == 0) {
                params = ExtractGoParametersDirect(child, content);
                break;
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
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "parameter_declaration") == 0) {
                // Standard parameter: name Type or just Type
                param = ExtractGoParameterDeclaration(child, content);
                is_valid_param = !param.type.empty(); // Go requires type
            } else if (strcmp(child_type, "variadic_parameter") == 0) {
                // Variadic parameter: ...Type
                param = ExtractGoVariadicParameter(child, content);
                is_valid_param = !param.type.empty();
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractGoParameterDeclaration(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                // Parameter name (optional in Go)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.name = content.substr(start, end - start);
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
                    param.type = content.substr(start, end - start);
                }
            }
        }
        
        // If no name was found, generate one based on position
        if (param.name.empty() && !param.type.empty()) {
            param.name = "arg"; // Go allows unnamed parameters
        }
        
        return param;
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