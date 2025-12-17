#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Swift-Specific Native Context Extractors
//==============================================================================

// Forward declaration for SwiftAdapter
class SwiftAdapter;

// Base template for Swift extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct SwiftNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (Swift functions)
template<>
struct SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract return type (Swift has strong type system)
        context.signature_type = ExtractSwiftReturnType(node, content);
        
        // Extract function parameters with Swift type annotations
        context.parameters = ExtractSwiftParameters(node, content);
        
        // Extract function modifiers (public, private, static, mutating, etc.)
        auto modifiers = ExtractSwiftModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static string ExtractSwiftReturnType(TSNode node, const string& content) {
        // Look for return type after -> in function signature
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_annotation") == 0) {
                // Find type after ->
                uint32_t type_child_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < type_child_count; j++) {
                    TSNode type_child = ts_node_child(child, j);
                    const char* type_child_type = ts_node_type(type_child);
                    
                    if (strcmp(type_child_type, "type_identifier") == 0 ||
                        strcmp(type_child_type, "optional_type") == 0 ||
                        strcmp(type_child_type, "array_type") == 0 ||
                        strcmp(type_child_type, "dictionary_type") == 0 ||
                        strcmp(type_child_type, "tuple_type") == 0) {
                        uint32_t start = ts_node_start_byte(type_child);
                        uint32_t end = ts_node_end_byte(type_child);
                        if (start < content.length() && end <= content.length()) {
                            return content.substr(start, end - start);
                        }
                    }
                }
            } else if (strcmp(child_type, "type_identifier") == 0 ||
                       strcmp(child_type, "optional_type") == 0) {
                // Direct return type annotation
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return ""; // Void or inferred return type
    }
    
    static vector<ParameterInfo> ExtractSwiftParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find parameter_list or function_value_parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "parameter_list") == 0 ||
                strcmp(child_type, "function_value_parameters") == 0) {
                params = ExtractSwiftParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractSwiftParametersDirect(TSNode params_node, const string& content) {
        vector<ParameterInfo> parameters;
        
        uint32_t child_count = ts_node_child_count(params_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(params_node, i);
            const char* child_type = ts_node_type(child);
            
            ParameterInfo param;
            bool is_valid_param = false;
            
            if (strcmp(child_type, "parameter") == 0) {
                // Standard parameter: label name: Type or name: Type = default
                param = ExtractSwiftParameter(child, content);
                is_valid_param = !param.name.empty();
            } else if (strcmp(child_type, "variadic_parameter") == 0) {
                // Variadic parameter: args: Type...
                param = ExtractSwiftVariadicParameter(child, content);
                is_valid_param = !param.name.empty();
            }
            
            if (is_valid_param) {
                parameters.push_back(param);
            }
        }
        
        return parameters;
    }
    
    static ParameterInfo ExtractSwiftParameter(TSNode node, const string& content) {
        ParameterInfo param;
        uint32_t child_count = ts_node_child_count(node);
        
        string external_name = "";
        string internal_name = "";
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                // Could be external name or internal name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    string name = content.substr(start, end - start);
                    if (external_name.empty()) {
                        external_name = name;
                    } else {
                        internal_name = name;
                    }
                }
            } else if (strcmp(child_type, "type_annotation") == 0) {
                // Parameter type
                param.type = ExtractSwiftTypeAnnotation(child, content);
            } else if (strcmp(child_type, "default_parameter_clause") == 0) {
                // Default value
                param.is_optional = true;
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    param.default_value = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "inout") == 0) {
                // inout parameter
                param.annotations = "inout";
            }
        }
        
        // Swift parameter naming: external_name internal_name: Type
        if (!internal_name.empty()) {
            param.name = external_name + " " + internal_name;
        } else {
            param.name = external_name;
        }
        
        return param;
    }
    
    static ParameterInfo ExtractSwiftVariadicParameter(TSNode node, const string& content) {
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
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "type_annotation") == 0) {
                param.type = ExtractSwiftTypeAnnotation(child, content) + "...";
            }
        }
        
        return param;
    }
    
    static string ExtractSwiftTypeAnnotation(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "optional_type") == 0 ||
                strcmp(child_type, "array_type") == 0 ||
                strcmp(child_type, "dictionary_type") == 0 ||
                strcmp(child_type, "tuple_type") == 0 ||
                strcmp(child_type, "function_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractSwiftModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for function modifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "access_level_modifier") == 0 ||
                    strcmp(sibling_type, "member_modifier") == 0 ||
                    strcmp(sibling_type, "mutation_modifier") == 0 ||
                    strcmp(sibling_type, "override_modifier") == 0) {
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

// Specialization for ARROW_FUNCTION (Swift closures)
template<>
struct SwiftNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "closure";
        
        // Extract closure parameters
        context.parameters = ExtractSwiftClosureParameters(node, content);
        
        // Extract closure modifiers (@escaping, @autoclosure, etc.)
        auto modifiers = ExtractSwiftClosureModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static vector<ParameterInfo> ExtractSwiftClosureParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Find closure_parameters node
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "closure_parameters") == 0) {
                params = SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractSwiftParametersDirect(child, content);
                break;
            }
        }
        
        return params;
    }
    
    static vector<string> ExtractSwiftClosureModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Look for closure attributes
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "attribute") == 0) {
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

// Specialization for CLASS_WITH_METHODS (Swift classes, structs, protocols)
template<>
struct SwiftNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;

        // Determine the actual type by looking at keyword children
        // Swift grammar uses class_declaration for all type declarations
        const char* node_type = ts_node_type(node);
        context.signature_type = "type";  // Default

        if (strcmp(node_type, "class_declaration") == 0 ||
            strcmp(node_type, "protocol_declaration") == 0) {
            // Look for keyword child to determine actual type
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char* child_type = ts_node_type(child);

                if (strcmp(child_type, "class") == 0) {
                    context.signature_type = "class";
                    break;
                } else if (strcmp(child_type, "struct") == 0) {
                    context.signature_type = "struct";
                    break;
                } else if (strcmp(child_type, "protocol") == 0) {
                    context.signature_type = "protocol";
                    break;
                } else if (strcmp(child_type, "enum") == 0) {
                    context.signature_type = "enum";
                    break;
                } else if (strcmp(child_type, "actor") == 0) {
                    context.signature_type = "actor";
                    break;
                }
            }
        }

        // Extract parent types into parameters
        bool has_inheritance = false;
        context.parameters = ExtractParentTypes(node, content, has_inheritance);
        context.modifiers = ExtractSwiftTypeModifiers(node, content, has_inheritance);

        return context;
    }

public:
    // Extract parent types from type_inheritance_clause or inheritance_specifier
    static vector<ParameterInfo> ExtractParentTypes(TSNode node, const string& content,
                                                     bool& has_inheritance) {
        vector<ParameterInfo> parents;
        has_inheritance = false;

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            // Check for type_inheritance_clause (classes) or inheritance_specifier (protocols)
            if (strcmp(child_type, "type_inheritance_clause") == 0 ||
                strcmp(child_type, "inheritance_specifier") == 0) {
                has_inheritance = true;
                // : SuperClass, Protocol1, Protocol2
                uint32_t inherit_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < inherit_count; j++) {
                    TSNode inherit_child = ts_node_child(child, j);
                    const char* inherit_type = ts_node_type(inherit_child);

                    // Skip punctuation (: and ,)
                    if (strcmp(inherit_type, ":") == 0 ||
                        strcmp(inherit_type, ",") == 0) {
                        continue;
                    }

                    if (strcmp(inherit_type, "type_identifier") == 0 ||
                        strcmp(inherit_type, "identifier") == 0) {
                        string type_name = ExtractNodeText(inherit_child, content);
                        if (!type_name.empty()) {
                            parents.push_back({type_name, ""});
                        }
                    } else if (strcmp(inherit_type, "user_type") == 0 ||
                               strcmp(inherit_type, "generic_type") == 0) {
                        // Complex type - extract the identifier from within
                        string type_name = ExtractTypeName(inherit_child, content);
                        if (!type_name.empty()) {
                            parents.push_back({type_name, ""});
                        }
                    }
                }
                // Don't break - there might be multiple inheritance_specifier nodes
            }
        }

        return parents;
    }

    static string ExtractTypeName(TSNode node, const string& content) {
        // For complex types, try to get the full text
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "identifier") == 0) {
                return ExtractNodeText(child, content);
            }
        }
        // Fallback: get the whole node text
        return ExtractNodeText(node, content);
    }

    static vector<string> ExtractSwiftTypeModifiers(TSNode node, const string& content,
                                                     bool has_inheritance) {
        vector<string> modifiers;

        // Add extends keyword if type has inheritance
        if (has_inheritance) {
            modifiers.push_back("extends");
        }

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "access_level_modifier") == 0 ||
                strcmp(child_type, "member_modifier") == 0) {
                string mod = ExtractNodeText(child, content);
                if (!mod.empty()) {
                    modifiers.push_back(mod);
                }
            } else if (strcmp(child_type, "attribute") == 0) {
                // @objc, @available, etc.
                string attr = ExtractNodeText(child, content);
                if (!attr.empty()) {
                    modifiers.push_back(attr);
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

// Specialization for VARIABLE_WITH_TYPE (Swift variable declarations)
template<>
struct SwiftNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract Swift variable type (strong type system)
        context.signature_type = ExtractSwiftVariableType(node, content);
        
        // Extract variable modifiers (var/let, access level, etc.)
        auto modifiers = ExtractSwiftVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static string ExtractSwiftVariableType(TSNode node, const string& content) {
        // Look for type annotation in variable declaration
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "type_annotation") == 0) {
                return SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractSwiftTypeAnnotation(child, content);
            }
        }
        
        return ""; // Type inferred
    }
    
    static vector<string> ExtractSwiftVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for variable declaration modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "access_level_modifier") == 0 ||
                strcmp(child_type, "member_modifier") == 0 ||
                strcmp(child_type, "ownership_modifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "attribute") == 0) {
                // @objc, @IBOutlet, etc.
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            }
        }
        
        // Check if this is var or let
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            const char* parent_type = ts_node_type(parent);
            if (strcmp(parent_type, "property_declaration") == 0) {
                // Check for var/let keyword
                uint32_t parent_count = ts_node_child_count(parent);
                for (uint32_t i = 0; i < parent_count; i++) {
                    TSNode sibling = ts_node_child(parent, i);
                    const char* sibling_type = ts_node_type(sibling);
                    
                    if (strcmp(sibling_type, "var") == 0 || strcmp(sibling_type, "let") == 0) {
                        modifiers.push_back(sibling_type);
                        break;
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for ASYNC_FUNCTION (Swift async functions)
template<>
struct SwiftNativeExtractor<NativeExtractionStrategy::ASYNC_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        context.signature_type = "async";
        
        // Extract parameters and return type (similar to regular functions)
        context.parameters = SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractSwiftParameters(node, content);
        
        // Extract async/throws modifiers
        auto modifiers = ExtractSwiftAsyncModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
public:
    static vector<string> ExtractSwiftAsyncModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Look for async/throws keywords
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "async") == 0 ||
                strcmp(child_type, "throws") == 0 ||
                strcmp(child_type, "rethrows") == 0) {
                modifiers.push_back(child_type);
            }
        }
        
        // Also get regular function modifiers
        auto regular_modifiers = SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractSwiftModifiers(node, content);
        modifiers.insert(modifiers.end(), regular_modifiers.begin(), regular_modifiers.end());
        
        return modifiers;
    }
};

// Specialization for FUNCTION_CALL (Swift function calls)
template<>
struct SwiftNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<SwiftLanguageTag>::Extract(node, content);
    }
};

} // namespace duckdb