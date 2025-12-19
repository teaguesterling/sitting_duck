#pragma once

#include "native_context_extraction.hpp"
#include "function_call_extractor.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// C++ Native Context Extractors  
//==============================================================================

// Forward declaration for CppAdapter
class CppAdapter;

// Base template for C++ extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct CppNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (C++ functions/methods)
template<>
struct CppNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            // Extract return type (C++ functions have explicit return types)
            context.signature_type = ExtractCppReturnType(node, content);
            
            // Extract function parameters with C++ type annotations
            context.parameters = ExtractCppParameters(node, content);
            
            // Extract function specifiers and qualifiers
            auto modifiers = ExtractCppModifiers(node, content);
            context.modifiers = modifiers;
            
        } catch (...) {
            // Fallback on any error: provide minimal working context
            context.signature_type = "";  // Empty string becomes NULL in output
            context.parameters.clear();
            context.modifiers.clear();
        }
        
        return context;
    }
    
    // Public helper method accessible to other extractors
    static ParameterInfo ExtractParameterDeclaration(TSNode node, const string& content) {
        ParameterInfo param;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return param;
        }
        
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count to prevent infinite loops
        if (child_count > 1000) {
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
            
            if (strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "template_type") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "reference_type") == 0) {
                // Parameter type
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                
                // Safety check: ensure bounds are valid
                if (start < content.length() && end <= content.length() && end > start) {
                    param.type = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "identifier") == 0) {
                // Parameter name
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                // Safety check: ensure bounds are valid
                if (start < content.length() && end <= content.length() && end > start) {
                    param.name = content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "reference_declarator") == 0 ||
                       strcmp(child_type, "pointer_declarator") == 0 ||
                       strcmp(child_type, "array_declarator") == 0 ||
                       strcmp(child_type, "function_declarator") == 0) {
                // C++ declarators - look for nested identifier
                uint32_t declarator_child_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < declarator_child_count && j < 100; j++) {
                    TSNode declarator_child = ts_node_child(child, j);
                    if (!ts_node_is_null(declarator_child)) {
                        const char* declarator_child_type = ts_node_type(declarator_child);
                        if (declarator_child_type && strcmp(declarator_child_type, "identifier") == 0) {
                            uint32_t start = ts_node_start_byte(declarator_child);
                            uint32_t end = ts_node_end_byte(declarator_child);
                            if (start < content.length() && end <= content.length() && end > start) {
                                param.name = content.substr(start, end - start);
                                break; // Found identifier, stop looking
                            }
                        }
                    }
                }
            } else if (strcmp(child_type, "default_parameter_declaration") == 0) {
                // Parameter with default value
                param.is_optional = true;
                // Extract default value from this node
                uint32_t def_count = ts_node_child_count(child);
                
                // Safety check: reasonable limit on default value children
                if (def_count > 100) {
                    continue;
                }
                
                for (uint32_t j = 0; j < def_count; j++) {
                    TSNode def_child = ts_node_child(child, j);
                    
                    // Safety check: ensure child node is valid
                    if (ts_node_is_null(def_child)) {
                        continue;
                    }
                    
                    const char* def_type = ts_node_type(def_child);
                    
                    // Safety check: ensure def_type is not null
                    if (!def_type) {
                        continue;
                    }
                    
                    if (strcmp(def_type, "=") != 0) { // Skip the equals sign
                        uint32_t start = ts_node_start_byte(def_child);
                        uint32_t end = ts_node_end_byte(def_child);
                        
                        // Safety check: ensure bounds are valid
                        if (start < content.length() && end <= content.length() && end > start) {
                            param.default_value = content.substr(start, end - start);
                        }
                    }
                }
            } else if (strcmp(child_type, "storage_class_specifier") == 0 ||
                       strcmp(child_type, "type_qualifier") == 0) {
                // Parameter qualifiers (const, volatile, etc.)
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    if (!param.annotations.empty()) {
                        param.annotations += " ";
                    }
                    param.annotations += content.substr(start, end - start);
                }
            }
        }
        
        return param;
    }
    
private:
    static string ExtractCppReturnType(TSNode node, const string& content) {
        // In C++, the return type appears as a direct child before the function_declarator
        // For function_definition nodes, look for type nodes before function_declarator

        const char* node_type = ts_node_type(node);

        // If this IS a function_declarator, we need to look at the parent for return type
        // This happens when extracting from standalone declarations like:
        //   void foo();  -- declaration node contains: type + function_declarator
        if (strcmp(node_type, "function_declarator") == 0) {
            TSNode parent = ts_node_parent(node);
            if (!ts_node_is_null(parent)) {
                string parent_return_type = ExtractReturnTypeFromParent(parent, node, content);
                if (!parent_return_type.empty()) {
                    return parent_return_type;
                }
            }
            // Fall through to check for constructor/destructor patterns
            string function_name = ExtractFunctionName(node, content);
            return CheckConstructorDestructor(function_name);
        }

        uint32_t child_count = ts_node_child_count(node);

        // Debug: Track what we find
        string function_name;

        // First pass: find function name and look for explicit return type
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            // Stop when we reach the function_declarator - return type comes before it
            if (strcmp(child_type, "function_declarator") == 0) {
                // Extract function name for constructor detection
                function_name = ExtractFunctionName(child, content);
                break;
            }

            // Look for various type constructs
            if (IsTypeNode(child_type)) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);

                if (start < content.length() && end <= content.length() && end > start) {
                    string type_text = content.substr(start, end - start);
                    // Skip modifiers like "static", "const" that are not the actual return type
                    if (!IsModifierKeyword(type_text)) {
                        return type_text;
                    }
                }
            }
        }

        // If no explicit return type found, check if this is a constructor or destructor
        return CheckConstructorDestructor(function_name);
    }

    // Check if a node type represents a type construct
    static bool IsTypeNode(const char* child_type) {
        return strcmp(child_type, "primitive_type") == 0 ||
               strcmp(child_type, "type_identifier") == 0 ||
               strcmp(child_type, "template_type") == 0 ||
               strcmp(child_type, "qualified_identifier") == 0 ||
               strcmp(child_type, "pointer_type") == 0 ||
               strcmp(child_type, "reference_type") == 0 ||
               strcmp(child_type, "auto") == 0 ||
               strcmp(child_type, "sized_type_specifier") == 0 ||
               strcmp(child_type, "decltype") == 0;
    }

    // Check if a type text is actually a modifier keyword
    static bool IsModifierKeyword(const string& type_text) {
        return type_text == "static" || type_text == "const" ||
               type_text == "inline" || type_text == "virtual" ||
               type_text == "extern" || type_text == "constexpr" ||
               type_text == "explicit" || type_text == "friend";
    }

    // Extract return type from parent node (for function_declarator nodes)
    static string ExtractReturnTypeFromParent(TSNode parent, TSNode declarator_node, const string& content) {
        const char* parent_type = ts_node_type(parent);

        // Parent could be: declaration, function_definition, field_declaration, etc.
        uint32_t child_count = ts_node_child_count(parent);

        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(parent, i);

            // Stop when we reach our function_declarator - type comes before it
            if (ts_node_eq(child, declarator_node)) {
                break;
            }

            const char* child_type = ts_node_type(child);

            if (IsTypeNode(child_type)) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);

                if (start < content.length() && end <= content.length() && end > start) {
                    string type_text = content.substr(start, end - start);
                    if (!IsModifierKeyword(type_text)) {
                        return type_text;
                    }
                }
            }
        }

        return "";
    }

    // Check if function name indicates constructor/destructor
    static string CheckConstructorDestructor(const string& function_name) {
        if (function_name.empty()) {
            return "";
        }

        // Check for destructor (starts with ~)
        if (function_name.length() > 1 && function_name[0] == '~') {
            return "void"; // Destructors return void
        }

        // For qualified names like "ClassName::MethodName", extract just the method name
        string method_name = function_name;
        string class_prefix;
        size_t scope_pos = function_name.find("::");
        if (scope_pos != string::npos) {
            class_prefix = function_name.substr(0, scope_pos);
            method_name = function_name.substr(scope_pos + 2);

            // Only treat as constructor if MethodName == ClassName
            // E.g., "ASTType::ASTType" is a constructor, but "CatalogSet::EntryLookup" is not
            if (method_name == class_prefix) {
                return class_prefix; // Constructor returns instance of the class
            }
        }
        // For unqualified names, be very conservative - many are legitimately constructors
        // Don't try to detect class context as it's unreliable

        // If we can't determine the return type, return empty string (will show as NULL)
        // This is better than guessing wrong
        return "";
    }
    
    static string ExtractFunctionName(TSNode function_declarator, const string& content) {
        uint32_t child_count = ts_node_child_count(function_declarator);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(function_declarator, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "identifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            } else if (strcmp(child_type, "destructor_name") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        return "";
    }
    
    static string ExtractContainingClassName(TSNode node, const string& content) {
        // Walk up the AST to find the containing class
        TSNode current = ts_node_parent(node);
        while (!ts_node_is_null(current)) {
            const char* node_type = ts_node_type(current);
            if (strcmp(node_type, "class_specifier") == 0 || 
                strcmp(node_type, "struct_specifier") == 0) {
                // Found containing class, extract its name
                uint32_t child_count = ts_node_child_count(current);
                for (uint32_t i = 0; i < child_count; i++) {
                    TSNode child = ts_node_child(current, i);
                    const char* child_type = ts_node_type(child);
                    if (strcmp(child_type, "type_identifier") == 0) {
                        uint32_t start = ts_node_start_byte(child);
                        uint32_t end = ts_node_end_byte(child);
                        if (start < content.length() && end <= content.length()) {
                            return content.substr(start, end - start);
                        }
                    }
                }
            }
            current = ts_node_parent(current);
        }
        return "";
    }
    
    static vector<ParameterInfo> ExtractCppParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return params;
        }
        
        // For C++, parameters are nested: function_definition -> function_declarator -> parameter_list
        // First find the function_declarator
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
            
            if (strcmp(child_type, "function_declarator") == 0) {
                // Now look for parameter_list within the function_declarator
                uint32_t declarator_child_count = ts_node_child_count(child);
                
                // Safety check: reasonable limit on child count
                if (declarator_child_count > 1000) {
                    continue;
                }
                
                for (uint32_t j = 0; j < declarator_child_count; j++) {
                    TSNode param_child = ts_node_child(child, j);
                    
                    // Safety check: ensure child node is valid
                    if (ts_node_is_null(param_child)) {
                        continue;
                    }
                    
                    const char* param_child_type = ts_node_type(param_child);
                    
                    // Safety check: ensure child_type is not null
                    if (!param_child_type) {
                        continue;
                    }
                    
                    if (strcmp(param_child_type, "parameter_list") == 0) {
                        params = ExtractCppParametersDirect(param_child, content);
                        return params; // Found parameters, return immediately
                    }
                }
            }
        }
        
        return params;
    }
    
    static vector<ParameterInfo> ExtractCppParametersDirect(TSNode params_node, const string& content) {
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
            
            if (strcmp(child_type, "parameter_declaration") == 0) {
                // Standard parameter: (Type param) or (Type param = default)
                param = ExtractParameterDeclaration(child, content);
                is_valid_param = !param.name.empty() || !param.type.empty();
            } else if (strcmp(child_type, "variadic_parameter") == 0) {
                // Variadic parameter: (...)
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
    
    static vector<string> ExtractCppModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // C++ function modifiers appear as siblings to the function_declarator within function_definition
        // Look for modifier siblings in the current node's children
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (ts_node_is_null(child)) continue;
            
            const char* child_type = ts_node_type(child);
            if (!child_type) continue;
            
            // Function specifiers and qualifiers that appear before function_declarator
            if (strcmp(child_type, "storage_class_specifier") == 0 ||
                strcmp(child_type, "type_qualifier") == 0 ||
                strcmp(child_type, "function_specifier") == 0 ||
                strcmp(child_type, "virtual_specifier") == 0 ||
                strcmp(child_type, "explicit_function_specifier") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length() && end > start) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            } else if (strcmp(child_type, "function_declarator") == 0) {
                // Look for trailing modifiers in the function_declarator
                uint32_t declarator_child_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < declarator_child_count; j++) {
                    TSNode declarator_child = ts_node_child(child, j);
                    if (ts_node_is_null(declarator_child)) continue;
                    
                    const char* declarator_child_type = ts_node_type(declarator_child);
                    if (!declarator_child_type) continue;
                    
                    if (strcmp(declarator_child_type, "type_qualifier") == 0 ||
                        strcmp(declarator_child_type, "noexcept") == 0 ||
                        strcmp(declarator_child_type, "override") == 0 ||
                        strcmp(declarator_child_type, "final") == 0) {
                        uint32_t start = ts_node_start_byte(declarator_child);
                        uint32_t end = ts_node_end_byte(declarator_child);
                        if (start < content.length() && end <= content.length() && end > start) {
                            modifiers.push_back(content.substr(start, end - start));
                        }
                    }
                }
            }
        }
        
        return modifiers;
    }
};

// Specialization for CLASS_WITH_METHODS and CLASS_WITH_INHERITANCE
template<>
struct CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;

        bool has_inheritance = false;
        context.signature_type = ExtractClassType(node, content);
        context.parameters = ExtractBaseClasses(node, content, has_inheritance);
        context.modifiers = ExtractCppClassModifiers(node, content, has_inheritance);

        return context;
    }

private:
    // Determine class type (class vs struct)
    static string ExtractClassType(TSNode node, const string& content) {
        string node_type = ts_node_type(node);
        if (node_type == "struct_specifier") {
            return "struct";
        }
        return "class";
    }

    // Extract base classes into parameters
    static vector<ParameterInfo> ExtractBaseClasses(TSNode node, const string& content,
                                                     bool& has_inheritance) {
        vector<ParameterInfo> parents;
        has_inheritance = false;

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "base_class_clause") == 0) {
                has_inheritance = true;
                // Extract each base class from the clause
                uint32_t base_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < base_count; j++) {
                    TSNode base_child = ts_node_child(child, j);
                    const char* base_type = ts_node_type(base_child);

                    // Skip punctuation and access specifiers
                    if (strcmp(base_type, ":") == 0 ||
                        strcmp(base_type, ",") == 0 ||
                        strcmp(base_type, "access_specifier") == 0 ||
                        strcmp(base_type, "public") == 0 ||
                        strcmp(base_type, "protected") == 0 ||
                        strcmp(base_type, "private") == 0 ||
                        strcmp(base_type, "virtual") == 0) {
                        continue;
                    }

                    // Extract type identifier
                    if (strcmp(base_type, "type_identifier") == 0 ||
                        strcmp(base_type, "qualified_identifier") == 0 ||
                        strcmp(base_type, "template_type") == 0 ||
                        strcmp(base_type, "dependent_type") == 0) {
                        string type_name = ExtractNodeText(base_child, content);
                        if (!type_name.empty()) {
                            parents.push_back({type_name, ""});
                        }
                    }
                }
                break;
            }
        }

        return parents;
    }

    static vector<string> ExtractCppClassModifiers(TSNode node, const string& content,
                                                    bool has_inheritance) {
        vector<string> modifiers;

        // Add extends keyword if class has base classes
        if (has_inheritance) {
            modifiers.push_back("extends");
        }

        // Check for template
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            const char* parent_type = ts_node_type(parent);
            if (strcmp(parent_type, "template_declaration") == 0) {
                modifiers.push_back("template");
            }
        }

        // Check for final specifier
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (strcmp(child_type, "virtual_specifier") == 0 ||
                strcmp(child_type, "final") == 0) {
                string text = ExtractNodeText(child, content);
                if (text == "final") {
                    modifiers.push_back("final");
                }
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
struct CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_INHERITANCE> {
    static NativeContext Extract(TSNode node, const string& content) {
        return CppNativeExtractor<NativeExtractionStrategy::CLASS_WITH_METHODS>::Extract(node, content);
    }
};

// Specialization for ARROW_FUNCTION (C++ lambda expressions)
template<>
struct CppNativeExtractor<NativeExtractionStrategy::ARROW_FUNCTION> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract lambda return type (often inferred, may be empty)
        context.signature_type = ExtractLambdaReturnType(node, content);
        
        // Extract lambda parameters (if present)
        context.parameters = ExtractLambdaParameters(node, content);
        
        // Extract lambda capture list and modifiers
        auto modifiers = ExtractLambdaModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractLambdaReturnType(TSNode node, const string& content) {
        // Lambda return types are often inferred in C++, but may be explicitly specified
        // Look for trailing return type syntax: []() -> ReturnType
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            if (!child_type) {
                continue;
            }
            
            if (strcmp(child_type, "trailing_return_type") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        // Most lambdas have inferred return types
        return "";
    }
    
    static vector<ParameterInfo> ExtractLambdaParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return params;
        }
        
        // Look for parameter_list in lambda_expression
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
            
            if (strcmp(child_type, "parameter_list") == 0) {
                // Use the same parameter extraction logic as regular functions
                uint32_t param_child_count = ts_node_child_count(child);
                
                // Safety check: reasonable limit on parameter count
                if (param_child_count > 1000) {
                    return params;
                }
                
                for (uint32_t j = 0; j < param_child_count; j++) {
                    TSNode param_child = ts_node_child(child, j);
                    
                    // Safety check: ensure child node is valid
                    if (ts_node_is_null(param_child)) {
                        continue;
                    }
                    
                    const char* param_child_type = ts_node_type(param_child);
                    
                    // Safety check: ensure child_type is not null
                    if (!param_child_type) {
                        continue;
                    }
                    
                    if (strcmp(param_child_type, "parameter_declaration") == 0) {
                        ParameterInfo param = ExtractParameterDeclaration(param_child, content);
                        if (!param.name.empty() || !param.type.empty()) {
                            params.push_back(param);
                        }
                    }
                }
                break;
            }
        }
        
        return params;
    }
    
    static vector<string> ExtractLambdaModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Look for lambda capture list and other lambda-specific modifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            if (!child_type) {
                continue;
            }
            
            if (strcmp(child_type, "lambda_capture_specifier") == 0 ||
                strcmp(child_type, "lambda_default_capture") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    modifiers.push_back(content.substr(start, end - start));
                }
            }
        }
        
        return modifiers;
    }
    
    // Reuse the parameter declaration extraction from FUNCTION_WITH_PARAMS
    static ParameterInfo ExtractParameterDeclaration(TSNode node, const string& content) {
        return CppNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS>::ExtractParameterDeclaration(node, content);
    }
};

// Specialization for VARIABLE_WITH_TYPE (C++ variable declarations)
template<>
struct CppNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Extract C++ variable type
        context.signature_type = ExtractCppVariableType(node, content);
        
        // Extract variable specifiers and qualifiers
        auto modifiers = ExtractCppVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractCppVariableType(TSNode node, const string& content) {
        // Look for type in variable declaration
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            if (strcmp(child_type, "primitive_type") == 0 ||
                strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "template_type") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0 ||
                strcmp(child_type, "pointer_type") == 0 ||
                strcmp(child_type, "reference_type") == 0 ||
                strcmp(child_type, "auto") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                if (start < content.length() && end <= content.length()) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return "";
    }
    
    static vector<string> ExtractCppVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check parent for variable specifiers
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode child = ts_node_child(parent, i);
                const char* child_type = ts_node_type(child);
                
                if (strcmp(child_type, "storage_class_specifier") == 0 ||
                    strcmp(child_type, "type_qualifier") == 0 ||
                    strcmp(child_type, "constexpr") == 0 ||
                    strcmp(child_type, "thread_local") == 0) {
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

// Specialization for FUNCTION_CALL (C++ function calls and expressions) 
template<>
struct CppNativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
    static NativeContext Extract(TSNode node, const string& content) {
        return UnifiedFunctionCallExtractor<CppLanguageTag>::Extract(node, content);
    }
};

// Specialization for CUSTOM (C++ function calls and expressions) - DEPRECATED: Use FUNCTION_CALL
template<>
struct CppNativeExtractor<NativeExtractionStrategy::CUSTOM> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            const char* node_type = ts_node_type(node);
            
            if (strcmp(node_type, "call_expression") == 0) {
                context = ExtractCallExpression(node, content);
            } else if (strcmp(node_type, "new_expression") == 0) {
                context = ExtractNewExpression(node, content);
            } else if (strcmp(node_type, "delete_expression") == 0) {
                context = ExtractDeleteExpression(node, content);
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
    static NativeContext ExtractCallExpression(TSNode node, const string& content) {
        NativeContext context;
        
        // For call_expression: extract function name and arguments
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // First child is usually the function identifier or member expression
            if (i == 0) {
                if (strcmp(child_type, "identifier") == 0 ||
                    strcmp(child_type, "qualified_identifier") == 0 ||
                    strcmp(child_type, "field_expression") == 0) {
                    
                    uint32_t start = ts_node_start_byte(child);
                    uint32_t end = ts_node_end_byte(child);
                    
                    if (start < content.length() && end <= content.length() && end > start) {
                        context.signature_type = content.substr(start, end - start);
                    }
                }
            }
            
            // Extract arguments from argument_list
            if (strcmp(child_type, "argument_list") == 0) {
                context.parameters = ExtractCallArguments(child, content);
            }
        }
        
        if (context.signature_type.empty()) {
            context.signature_type = "function_call";  // Fallback
        }
        
        return context;
    }
    
    static NativeContext ExtractNewExpression(TSNode node, const string& content) {
        NativeContext context;
        
        // For new_expression: extract class name and constructor arguments
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip "new" keyword, look for type identifier
            if (strcmp(child_type, "type_identifier") == 0 ||
                strcmp(child_type, "identifier") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0) {
                
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                
                if (start < content.length() && end <= content.length() && end > start) {
                    context.signature_type = content.substr(start, end - start);
                }
            }
            
            // Extract constructor arguments from argument_list
            if (strcmp(child_type, "argument_list") == 0) {
                context.parameters = ExtractCallArguments(child, content);
            }
        }
        
        if (context.signature_type.empty()) {
            context.signature_type = "constructor_call";  // Fallback
        }
        
        return context;
    }
    
    static NativeContext ExtractDeleteExpression(TSNode node, const string& content) {
        NativeContext context;
        
        // For delete_expression: extract object identifier
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip "delete" keyword, look for identifier
            if (strcmp(child_type, "identifier") == 0 ||
                strcmp(child_type, "qualified_identifier") == 0 ||
                strcmp(child_type, "field_expression") == 0) {
                
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                
                if (start < content.length() && end <= content.length() && end > start) {
                    context.signature_type = content.substr(start, end - start);
                }
            }
        }
        
        if (context.signature_type.empty()) {
            context.signature_type = "delete_call";  // Fallback
        }
        
        return context;
    }
    
    static vector<ParameterInfo> ExtractCallArguments(TSNode args_node, const string& content) {
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
                arg.name = "";  // Arguments don't typically have names in calls
                arguments.push_back(arg);
            }
        }
        
        return arguments;
    }
};

} // namespace duckdb