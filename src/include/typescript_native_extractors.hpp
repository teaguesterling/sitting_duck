#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
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

        try {
            string node_type = ts_node_type(node);
            bool has_extends = false;
            bool has_implements = false;

            if (node_type == "class_declaration") {
                context.signature_type = ExtractClassType(node, content);
                context.parameters = ExtractClassParents(node, content, has_extends, has_implements);
                context.modifiers = ExtractTypeScriptClassModifiers(node, content, has_extends, has_implements);
            } else if (node_type == "interface_declaration") {
                context.signature_type = "interface";
                context.parameters = ExtractInterfaceParents(node, content, has_extends);
                context.modifiers = ExtractInterfaceModifiers(node, content, has_extends);
            } else if (node_type == "enum_declaration") {
                context.signature_type = "enum";
                context.modifiers = ExtractEnumModifiers(node, content);
            } else if (node_type == "type_alias_declaration") {
                context.signature_type = ExtractTypeAliasType(node, content);
                context.modifiers = ExtractTypeAliasModifiers(node, content);
            } else if (node_type == "module_declaration") {
                context.signature_type = "module";
                context.modifiers = ExtractModuleModifiers(node, content);
            } else if (node_type == "namespace_declaration") {
                context.signature_type = "namespace";
                context.modifiers = ExtractNamespaceModifiers(node, content);
            } else {
                // Generic class-like structure
                context.signature_type = "class";
                context.parameters = ExtractClassParents(node, content, has_extends, has_implements);
                context.modifiers = ExtractTypeScriptClassModifiers(node, content, has_extends, has_implements);
            }
        } catch (...) {
            context.signature_type = "";
            context.modifiers.clear();
            context.parameters.clear();
        }

        return context;
    }

public:
    // Determine class type (class vs abstract_class)
    static string ExtractClassType(TSNode node, const string& content) {
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            if (strcmp(child_type, "abstract") == 0) {
                return "abstract_class";
            }
        }
        return "class";
    }

    // Extract parent types from class_heritage (extends and implements clauses)
    static vector<ParameterInfo> ExtractClassParents(TSNode node, const string& content,
                                                      bool& has_extends, bool& has_implements) {
        vector<ParameterInfo> parents;
        has_extends = false;
        has_implements = false;

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "class_heritage") == 0) {
                // class_heritage contains extends_clause and/or implements_clause
                uint32_t heritage_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < heritage_count; j++) {
                    TSNode heritage_child = ts_node_child(child, j);
                    const char* heritage_type = ts_node_type(heritage_child);

                    if (strcmp(heritage_type, "extends_clause") == 0) {
                        has_extends = true;
                        ExtractTypeIdentifiers(heritage_child, content, parents);
                    } else if (strcmp(heritage_type, "implements_clause") == 0) {
                        has_implements = true;
                        ExtractTypeIdentifiers(heritage_child, content, parents);
                    }
                }
            }
        }

        return parents;
    }

    // Extract parent interfaces for interface declarations
    static vector<ParameterInfo> ExtractInterfaceParents(TSNode node, const string& content,
                                                          bool& has_extends) {
        vector<ParameterInfo> parents;
        has_extends = false;

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "extends_type_clause") == 0 ||
                strcmp(child_type, "extends_clause") == 0) {
                has_extends = true;
                ExtractTypeIdentifiers(child, content, parents);
            }
        }

        return parents;
    }

    // Helper to extract type identifiers from extends/implements clauses
    static void ExtractTypeIdentifiers(TSNode clause_node, const string& content,
                                       vector<ParameterInfo>& parents) {
        uint32_t child_count = ts_node_child_count(clause_node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(clause_node, i);
            const char* child_type = ts_node_type(child);

            // Skip keyword nodes
            if (strcmp(child_type, "extends") == 0 ||
                strcmp(child_type, "implements") == 0 ||
                strcmp(child_type, ",") == 0) {
                continue;
            }

            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "generic_type") == 0 ||
                strcmp(child_type, "nested_type_identifier") == 0 ||
                strcmp(child_type, "member_expression") == 0) {
                string type_name = ExtractNodeText(child, content);
                if (!type_name.empty()) {
                    parents.push_back({type_name, ""});
                }
            }
        }
    }

    static vector<string> ExtractTypeScriptClassModifiers(TSNode node, const string& content,
                                                           bool has_extends, bool has_implements) {
        vector<string> modifiers;

        // Add inheritance keywords first
        if (has_extends) {
            modifiers.push_back("extends");
        }
        if (has_implements) {
            modifiers.push_back("implements");
        }

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "abstract") == 0 ||
                strcmp(child_type, "export") == 0 ||
                strcmp(child_type, "declare") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            }
            // Note: class_heritage is now extracted separately into parameters
        }

        return modifiers;
    }

    static vector<string> ExtractInterfaceModifiers(TSNode node, const string& content,
                                                     bool has_extends) {
        vector<string> modifiers;
        modifiers.push_back("interface");

        // Add extends keyword if interface extends other interfaces
        if (has_extends) {
            modifiers.push_back("extends");
        }

        // Check for export, declare modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "export") == 0 ||
                strcmp(child_type, "declare") == 0) {
                modifiers.push_back(child_type);
            }
            // Note: extends_clause is now extracted separately into parameters
        }

        return modifiers;
    }
    
    static vector<string> ExtractEnumModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("enum");
        
        // Check for const enum, export enum
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "const") == 0) {
                modifiers.push_back("const_enum");
            } else if (strcmp(child_type, "export") == 0) {
                modifiers.push_back("export");
            } else if (strcmp(child_type, "declare") == 0) {
                modifiers.push_back("declare");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractTypeAliasType(TSNode node, const string& content) {
        // Look for the type being aliased
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "union_type") == 0) {
                return "union_type";
            } else if (strcmp(child_type, "intersection_type") == 0) {
                return "intersection_type";
            } else if (strcmp(child_type, "object_type") == 0) {
                return "object_type";
            } else if (strcmp(child_type, "array_type") == 0) {
                return "array_type";
            } else if (strcmp(child_type, "function_type") == 0) {
                return "function_type";
            }
        }
        
        return "type_alias";
    }
    
    static vector<string> ExtractTypeAliasModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("type_definition");
        
        // Check for export, declare
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "export") == 0 ||
                strcmp(child_type, "declare") == 0) {
                modifiers.push_back(child_type);
            }
        }
        
        return modifiers;
    }
    
    static vector<string> ExtractModuleModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("module");
        
        // Check for ambient modules
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "declare") == 0) {
                modifiers.push_back("ambient");
            } else if (strcmp(child_type, "export") == 0) {
                modifiers.push_back("export");
            }
        }
        
        return modifiers;
    }
    
    static vector<string> ExtractNamespaceModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("namespace");

        // Check for export namespace
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "export") == 0) {
                modifiers.push_back("export");
            } else if (strcmp(child_type, "declare") == 0) {
                modifiers.push_back("declare");
            }
        }

        return modifiers;
    }

    static string ExtractNodeText(TSNode node, const string& content) {
        if (ts_node_is_null(node)) {
            return "";
        }

        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);

        if (start < content.length() && end <= content.length() && end > start) {
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
        
        try {
            string node_type = ts_node_type(node);
            
            if (node_type == "variable_declarator") {
                context.signature_type = ExtractVariableType(node, content);
                context.modifiers = ExtractVariableModifiers(node, content);
            } else if (node_type == "type_annotation") {
                context.signature_type = ExtractTypeFromAnnotation(node, content);
                context.modifiers.push_back("type_annotation");
            } else if (node_type == "identifier") {
                context.signature_type = ExtractIdentifierType(node, content);
                context.modifiers = ExtractIdentifierModifiers(node, content);
            } else if (node_type == "property_identifier") {
                context.signature_type = ExtractPropertyType(node, content);
                context.modifiers.push_back("property");
            } else if (node_type == "type_identifier") {
                context.signature_type = ExtractTypeIdentifierInfo(node, content);
                context.modifiers.push_back("type_reference");
            } else if (node_type == "property_signature") {
                context.signature_type = ExtractPropertySignatureType(node, content);
                context.modifiers = ExtractPropertySignatureModifiers(node, content);
            } else if (node_type == "field_declaration") {
                context.signature_type = ExtractFieldType(node, content);
                context.modifiers = ExtractFieldModifiers(node, content);
            } else if (node_type == "member_expression") {
                context.signature_type = ExtractMemberExpressionType(node, content);
                context.modifiers = ExtractMemberExpressionModifiers(node, content);
            } else if (node_type == "subscript_expression") {
                context.signature_type = ExtractSubscriptType(node, content);
                context.modifiers = ExtractSubscriptModifiers(node, content);
            } else if (node_type == "computed_property_name") {
                context.signature_type = ExtractComputedPropertyType(node, content);
                context.modifiers = ExtractComputedPropertyModifiers(node, content);
            } else {
                // Generic variable/type extraction
                context.signature_type = ExtractVariableType(node, content);
                context.modifiers = ExtractVariableModifiers(node, content);
            }
        } catch (...) {
            context.signature_type = "";
            context.modifiers.clear();
        }
        
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
    
    static string ExtractTypeFromAnnotation(TSNode annotation_node, const string& content) {
        // Reuse the existing method from FUNCTION_WITH_PARAMS
        return TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractTypeFromAnnotation(annotation_node, content);
    }
    
    static string ExtractIdentifierType(TSNode node, const string& content) {
        // For identifiers, check parent context to infer type
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            
            if (parent_type == "variable_declarator") {
                return ExtractVariableType(parent, content);
            } else if (parent_type == "type_annotation") {
                return ExtractTypeFromAnnotation(parent, content);
            } else if (parent_type == "property_signature") {
                return ExtractPropertySignatureType(parent, content);
            } else if (parent_type == "parameter") {
                // Check if this is a typed parameter
                uint32_t parent_count = ts_node_child_count(parent);
                for (uint32_t i = 0; i < parent_count; i++) {
                    TSNode child = ts_node_child(parent, i);
                    if (strcmp(ts_node_type(child), "type_annotation") == 0) {
                        return ExtractTypeFromAnnotation(child, content);
                    }
                }
            }
        }
        
        return "identifier";
    }
    
    static vector<string> ExtractIdentifierModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check if this identifier is in a specific context
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            modifiers.push_back("in_" + parent_type);
            
            // Check for additional context modifiers
            if (parent_type == "member_expression") {
                modifiers.push_back("member_access");
            } else if (parent_type == "call_expression") {
                modifiers.push_back("function_call");
            } else if (parent_type == "variable_declarator") {
                modifiers.push_back("variable_name");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractPropertyType(TSNode node, const string& content) {
        // For property identifiers, check parent for type context
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            string parent_type = ts_node_type(parent);
            
            if (parent_type == "member_expression") {
                return "property_access";
            } else if (parent_type == "property_signature") {
                return ExtractPropertySignatureType(parent, content);
            } else if (parent_type == "field_declaration") {
                return ExtractFieldType(parent, content);
            }
        }
        
        return "property";
    }
    
    static string ExtractTypeIdentifierInfo(TSNode node, const string& content) {
        // Extract the actual type name
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        
        if (start < content.length() && end <= content.length() && end > start) {
            return content.substr(start, end - start);
        }
        
        return "type";
    }
    
    static string ExtractPropertySignatureType(TSNode node, const string& content) {
        // Look for type annotation in property signature
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "type_annotation") == 0) {
                return ExtractTypeFromAnnotation(child, content);
            }
        }
        
        return "property_signature";
    }
    
    static vector<string> ExtractPropertySignatureModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("interface_property");
        
        // Check for optional modifier
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "?") == 0) {
                modifiers.push_back("optional");
                break;
            }
        }
        
        return modifiers;
    }
    
    static string ExtractFieldType(TSNode node, const string& content) {
        // Look for type annotation in field declaration
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "type_annotation") == 0) {
                return ExtractTypeFromAnnotation(child, content);
            }
        }
        
        return "field";
    }
    
    static vector<string> ExtractFieldModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("class_field");
        
        // Check for access modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "accessibility_modifier") {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            } else if (child_type == "readonly") {
                modifiers.push_back("readonly");
            } else if (child_type == "static") {
                modifiers.push_back("static");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractMemberExpressionType(TSNode node, const string& content) {
        // For member expressions like obj.property or obj[property]
        uint32_t child_count = ts_node_child_count(node);
        
        string object_type = "";
        string property_type = "";
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (i == 0) {
                // First child is the object
                object_type = child_type;
            } else if (child_type == "property_identifier" || child_type == "identifier") {
                // Property being accessed
                property_type = child_type;
            }
        }
        
        return "member_access";
    }
    
    static vector<string> ExtractMemberExpressionModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("member_expression");
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (i == 0) {
                modifiers.push_back("object_" + child_type);
            } else if (child_type == "property_identifier") {
                modifiers.push_back("property_access");
            } else if (child_type == "identifier") {
                modifiers.push_back("dynamic_property");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractSubscriptType(TSNode node, const string& content) {
        // For subscript expressions like arr[index]
        return "subscript_access";
    }
    
    static vector<string> ExtractSubscriptModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("subscript_expression");
        modifiers.push_back("computed_access");
        
        // Check if it's array-like or object-like access
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "number_literal") {
                modifiers.push_back("numeric_index");
            } else if (child_type == "string_literal") {
                modifiers.push_back("string_index");
            } else if (child_type == "identifier") {
                modifiers.push_back("variable_index");
            }
        }
        
        return modifiers;
    }
    
    static string ExtractComputedPropertyType(TSNode node, const string& content) {
        // For computed property names like [key]: value
        return "computed_property";
    }
    
    static vector<string> ExtractComputedPropertyModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        modifiers.push_back("computed_property_name");
        modifiers.push_back("dynamic_key");
        
        // Analyze the computed expression
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "string_literal") {
                modifiers.push_back("string_computed");
            } else if (child_type == "template_string") {
                modifiers.push_back("template_computed");
            } else if (child_type == "identifier") {
                modifiers.push_back("variable_computed");
            }
        }
        
        return modifiers;
    }
};

// Specialization for FUNCTION_CALL (TypeScript function calls and expressions) 
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<TypeScriptLanguageTag>::Extract(node, content);
    }
};

// Specialization for CUSTOM (TypeScript function calls and expressions) - DEPRECATED: Use FUNCTION_CALL
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

// Specialization for FUNCTION_WITH_DECORATORS
template<>
struct TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_DECORATORS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            string node_type = ts_node_type(node);
            
            if (node_type == "function_declaration" || node_type == "method_definition" || 
                node_type == "method_signature" || node_type == "arrow_function") {
                // Start with basic function extraction
                context = TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
                
                // Add decorators and advanced modifiers
                vector<string> decorators = ExtractTSDecorators(node, content);
                vector<string> advanced_modifiers = ExtractTSAdvancedModifiers(node, content);
                
                // Combine existing modifiers with decorators and advanced modifiers
                context.modifiers.insert(context.modifiers.end(), decorators.begin(), decorators.end());
                context.modifiers.insert(context.modifiers.end(), advanced_modifiers.begin(), advanced_modifiers.end());
                
                // Enhance signature type with decorator info
                if (!decorators.empty()) {
                    context.signature_type = "decorated_" + context.signature_type;
                }
            } else {
                // Fallback to basic function extraction
                context = TypeScriptNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::Extract(node, content);
            }
        } catch (...) {
            context.signature_type = "";
            context.parameters.clear();
            context.modifiers.clear();
        }
        
        return context;
    }
    
private:
    static vector<string> ExtractTSDecorators(TSNode node, const string& content) {
        vector<string> decorators;
        
        // Check parent and siblings for decorators
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                string sibling_type = ts_node_type(sibling);
                
                if (sibling_type == "decorator") {
                    string decorator_text = ExtractNodeText(sibling, content);
                    if (!decorator_text.empty()) {
                        decorators.push_back(decorator_text);
                    }
                }
            }
        }
        
        // Check within the node itself
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "decorator") {
                string decorator_text = ExtractNodeText(child, content);
                if (!decorator_text.empty()) {
                    decorators.push_back(decorator_text);
                }
            }
        }
        
        return decorators;
    }
    
    static vector<string> ExtractTSAdvancedModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            // Extract TypeScript-specific modifiers
            if (child_type == "accessibility_modifier") {
                // public, private, protected
                string modifier_text = ExtractNodeText(child, content);
                if (!modifier_text.empty()) {
                    modifiers.push_back(modifier_text);
                }
            } else if (child_type == "readonly") {
                modifiers.push_back("readonly");
            } else if (child_type == "static") {
                modifiers.push_back("static");
            } else if (child_type == "abstract") {
                modifiers.push_back("abstract");
            } else if (child_type == "async") {
                modifiers.push_back("async");
            } else if (child_type == "override") {
                modifiers.push_back("override");
            }
        }
        
        // Check for generic type parameters
        bool has_generics = false;
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "type_parameters") {
                has_generics = true;
                modifiers.push_back("generic");
                break;
            }
        }
        
        // Check for optional parameters
        bool has_optional = false;
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            string child_type = ts_node_type(child);
            
            if (child_type == "formal_parameters") {
                uint32_t param_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < param_count; j++) {
                    TSNode param = ts_node_child(child, j);
                    string param_type = ts_node_type(param);
                    
                    if (param_type == "optional_parameter") {
                        has_optional = true;
                        modifiers.push_back("has_optional_params");
                        break;
                    }
                }
                if (has_optional) break;
            }
        }
        
        return modifiers;
    }
    
    static string ExtractNodeText(TSNode node, const string& content) {
        if (ts_node_is_null(node)) {
            return "";
        }
        
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        
        if (start < content.length() && end <= content.length() && end > start) {
            return content.substr(start, end - start);
        }
        
        return "";
    }
};

} // namespace duckdb