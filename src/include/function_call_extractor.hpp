#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>
#include <unordered_map>

namespace duckdb {

//==============================================================================
// Unified Function Call Extraction Framework
//==============================================================================

// Language-specific node type mappings for function call patterns
struct FunctionCallNodeTypes {
    const char* call_expression;      // Primary function call node
    const char* new_expression;       // Constructor call node (optional)
    const char* delete_expression;    // Destructor call node (optional)
    const char* arguments;            // Arguments container node
    const char* argument_list;        // Alternative arguments container
    
    // Function name node types (in order of preference)
    vector<const char*> function_name_types;
    
    // Punctuation to skip in arguments
    vector<const char*> argument_punctuation;
    
    // Additional call expression types (for languages like Rust with multiple call patterns)
    vector<const char*> additional_call_types;
    
    // Named parameter node types (for keyword arguments)
    vector<const char*> named_parameter_types;
};

// Default node types (most common across languages)
static const FunctionCallNodeTypes DEFAULT_FUNCTION_CALL_TYPES = {
    "call_expression",
    "new_expression", 
    "delete_expression",
    "arguments",
    "argument_list",
    {"identifier", "member_expression", "property_identifier", "qualified_identifier", "field_expression", "type_identifier"},
    {",", "(", ")", ";"},
    {},  // No additional call types by default
    {}   // No named parameter types by default
};

// Language-specific specializations
static const std::unordered_map<string, FunctionCallNodeTypes> LANGUAGE_FUNCTION_CALL_TYPES = {
    {"c", {
        "call_expression", nullptr, nullptr,
        "argument_list", "argument_list",
        {"identifier", "field_expression"},
        {",", "(", ")", ";"},
        {},  // No additional call types
        {}   // No named parameter types for C
    }},
    {"cpp", {
        "call_expression", "new_expression", "delete_expression",
        "argument_list", "argument_list",
        {"identifier", "qualified_identifier", "field_expression", "type_identifier"},
        {",", "(", ")", ";"},
        {},  // No additional call types
        {}   // No named parameter types for C++
    }},
    {"javascript", {
        "call_expression", "new_expression", nullptr,
        "arguments", "arguments", 
        {"identifier", "member_expression", "property_identifier"},
        {",", "(", ")", ";"},
        {},  // No additional call types
        {}   // No named parameter types for JavaScript
    }},
    {"typescript", {
        "call_expression", "new_expression", nullptr,
        "arguments", "arguments",
        {"identifier", "member_expression", "property_identifier"},
        {",", "(", ")", ";"},
        {},  // No additional call types
        {}   // No named parameter types for TypeScript
    }},
    {"python", {
        "call", "call", nullptr,
        "argument_list", "argument_list",
        {"identifier", "attribute", "subscript"},
        {",", "(", ")", ";"},
        {},  // No additional call types
        {"keyword_argument"}  // Named parameter types for Python
    }},
    {"go", {
        "call_expression", "composite_literal", nullptr,
        "argument_list", "literal_value",
        {"identifier", "selector_expression", "type_identifier"},
        {",", "(", ")", ";", "{", "}"},
        {},  // No additional call types
        {}   // No named parameter types for Go
    }},
    {"rust", {
        "call_expression", "struct_expression", nullptr,
        "arguments", "field_initializer_list", 
        {"identifier", "scoped_identifier", "field_identifier"},
        {",", "(", ")", ";", "{", "}", "!"},
        {"method_call_expression", "macro_invocation"},  // Additional call types for Rust
        {}   // No named parameter types for Rust (uses struct syntax)
    }},
    {"java", {
        "method_invocation", "object_creation_expression", nullptr,
        "argument_list", "argument_list",
        {"identifier", "scoped_identifier", "field_access"},
        {",", "(", ")", ";"},
        {},  // No additional call types for Java
        {}   // No named parameter types for Java
    }},
    {"php", {
        "function_call_expression", "object_creation_expression", nullptr,
        "arguments", "arguments",
        {"name", "variable_name", "member_access_expression"},
        {",", "(", ")", ";"},
        {"member_call_expression", "scoped_call_expression"},  // Additional call types for PHP
        {"argument_name"}   // Named parameter types for PHP 8+
    }},
    {"ruby", {
        "call", nullptr, nullptr,
        "argument_list", "argument_list",
        {"identifier", "constant", "scope_resolution"},
        {",", "(", ")", ";"},
        {"method_call", "chained_call"},  // Additional call types for Ruby
        {"pair", "keyword_parameter"}   // Named parameter types for Ruby (keyword arguments)
    }},
    {"swift", {
        "call_expression", "init_expression", nullptr,
        "argument_list", "call_suffix",
        {"identifier", "navigation_expression", "self_expression"},
        {",", "(", ")", ";", ":"},
        {"postfix_expression", "try_expression"},  // Additional call types for Swift
        {"labeled_argument"}  // Named parameter types for Swift
    }},
    {"r", {
        "call", nullptr, nullptr,
        "arguments", "arguments",
        {"identifier", "namespace_get", "namespace_get_internal"},
        {",", "(", ")", ";"},
        {},  // No additional call types for R
        {}   // No named parameter types for R (uses positional arguments)
    }},
    {"kotlin", {
        "call_expression", "constructor_invocation", nullptr,
        "value_arguments", "value_arguments",
        {"simple_identifier", "navigation_expression", "super_expression"},
        {",", "(", ")", ";"},
        {"postfix_expression", "callable_reference"},  // Additional call types for Kotlin
        {"value_argument"}  // Named parameter types for Kotlin
    }}
};

// Unified function call extractor using language-specific node types
template<typename LanguageTag>
class UnifiedFunctionCallExtractor {
private:
    static const FunctionCallNodeTypes& GetNodeTypes() {
        auto lang_name = LanguageTag::GetLanguageName();
        auto it = LANGUAGE_FUNCTION_CALL_TYPES.find(lang_name);
        return (it != LANGUAGE_FUNCTION_CALL_TYPES.end()) ? it->second : DEFAULT_FUNCTION_CALL_TYPES;
    }
    
public:
    static NativeContext Extract(TSNode node, const string& content) {
        const auto& types = GetNodeTypes();
        const char* node_type = ts_node_type(node);
        
        try {
            if (strcmp(node_type, types.call_expression) == 0) {
                return ExtractCallExpression(node, content, types);
            } 
            else if (types.new_expression && strcmp(node_type, types.new_expression) == 0) {
                return ExtractNewExpression(node, content, types);
            }
            else if (types.delete_expression && strcmp(node_type, types.delete_expression) == 0) {
                return ExtractDeleteExpression(node, content, types);
            }
            else {
                // Check additional call types
                for (const char* additional_type : types.additional_call_types) {
                    if (strcmp(node_type, additional_type) == 0) {
                        return ExtractCallExpression(node, content, types);
                    }
                }
                // Unknown node type for FUNCTION_CALL strategy
                return NativeContext();
            }
        } catch (...) {
            return NativeContext();
        }
    }
    
private:
    static NativeContext ExtractCallExpression(TSNode node, const string& content, const FunctionCallNodeTypes& types) {
        NativeContext context;
        
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Extract function name from first matching child
            if (i == 0 && context.signature_type.empty()) {
                for (const char* name_type : types.function_name_types) {
                    if (strcmp(child_type, name_type) == 0) {
                        context.signature_type = ExtractNodeText(child, content);
                        break;
                    }
                }
            }
            
            // Extract arguments from arguments container
            if (strcmp(child_type, types.arguments) == 0 || 
                strcmp(child_type, types.argument_list) == 0) {
                context.parameters = ExtractCallArguments(child, content, types);
            }
        }
        
        if (context.signature_type.empty()) {
            context.signature_type = "function_call";  // Fallback
        }
        
        return context;
    }
    
    static NativeContext ExtractNewExpression(TSNode node, const string& content, const FunctionCallNodeTypes& types) {
        NativeContext context;
        
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip "new" keyword, look for class identifier
            if (context.signature_type.empty()) {
                for (const char* name_type : types.function_name_types) {
                    if (strcmp(child_type, name_type) == 0) {
                        context.signature_type = ExtractNodeText(child, content);
                        break;
                    }
                }
            }
            
            // Extract constructor arguments
            if (strcmp(child_type, types.arguments) == 0 || 
                strcmp(child_type, types.argument_list) == 0) {
                context.parameters = ExtractCallArguments(child, content, types);
            }
        }
        
        if (context.signature_type.empty()) {
            context.signature_type = "constructor_call";  // Fallback
        }
        
        return context;
    }
    
    static NativeContext ExtractDeleteExpression(TSNode node, const string& content, const FunctionCallNodeTypes& types) {
        NativeContext context;
        
        uint32_t child_count = ts_node_child_count(node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip "delete" keyword, look for object identifier  
            for (const char* name_type : types.function_name_types) {
                if (strcmp(child_type, name_type) == 0) {
                    context.signature_type = ExtractNodeText(child, content);
                    break;
                }
            }
        }
        
        if (context.signature_type.empty()) {
            context.signature_type = "delete_call";  // Fallback
        }
        
        return context;
    }
    
    static vector<ParameterInfo> ExtractCallArguments(TSNode args_node, const string& content, const FunctionCallNodeTypes& types) {
        vector<ParameterInfo> arguments;
        
        uint32_t child_count = ts_node_child_count(args_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(args_node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip punctuation
            bool is_punctuation = false;
            for (const char* punct : types.argument_punctuation) {
                if (strcmp(child_type, punct) == 0) {
                    is_punctuation = true;
                    break;
                }
            }
            if (is_punctuation) continue;
            
            ParameterInfo arg;
            
            // Check if this is a named parameter (keyword argument)
            bool is_named_param = false;
            for (const char* named_type : types.named_parameter_types) {
                if (strcmp(child_type, named_type) == 0) {
                    is_named_param = true;
                    break;
                }
            }
            
            if (is_named_param) {
                // Extract named parameter (key=value pattern)
                arg = ExtractNamedArgument(child, content);
            } else {
                // Extract positional argument
                string arg_text = ExtractNodeText(child, content);
                if (!arg_text.empty()) {
                    arg.type = arg_text;  // Value/expression
                    arg.name = "";        // No name for positional args
                }
            }
            
            if (!arg.type.empty() || !arg.name.empty()) {
                arguments.push_back(arg);
            }
        }
        
        return arguments;
    }
    
    static ParameterInfo ExtractNamedArgument(TSNode named_arg_node, const string& content) {
        ParameterInfo param;
        
        uint32_t child_count = ts_node_child_count(named_arg_node);
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(named_arg_node, i);
            const char* child_type = ts_node_type(child);
            
            // Skip assignment operators
            if (strcmp(child_type, "=") == 0 || strcmp(child_type, ":") == 0) {
                continue;
            }
            
            string node_text = ExtractNodeText(child, content);
            if (node_text.empty()) continue;
            
            if (param.name.empty()) {
                // First non-operator node is usually the parameter name
                param.name = node_text;
            } else if (param.type.empty()) {
                // Second non-operator node is usually the value
                param.type = node_text;
            }
        }
        
        return param;
    }
    
    static string ExtractNodeText(TSNode node, const string& content) {
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        
        if (start < content.length() && end <= content.length() && end > start) {
            return content.substr(start, end - start);
        }
        
        return "";
    }
};

// Language tag definitions for template specialization
struct CLanguageTag {
    static string GetLanguageName() { return "c"; }
};

struct CppLanguageTag {
    static string GetLanguageName() { return "cpp"; }
};

struct JavaScriptLanguageTag {
    static string GetLanguageName() { return "javascript"; }
};

struct TypeScriptLanguageTag {
    static string GetLanguageName() { return "typescript"; }
};

struct PythonLanguageTag {
    static string GetLanguageName() { return "python"; }
};

struct GoLanguageTag {
    static string GetLanguageName() { return "go"; }
};

struct RustLanguageTag {
    static string GetLanguageName() { return "rust"; }
};

struct JavaLanguageTag {
    static string GetLanguageName() { return "java"; }
};

struct PHPLanguageTag {
    static string GetLanguageName() { return "php"; }
};

struct RubyLanguageTag {
    static string GetLanguageName() { return "ruby"; }
};

struct SwiftLanguageTag {
    static string GetLanguageName() { return "swift"; }
};

struct RLanguageTag {
    static string GetLanguageName() { return "r"; }
};

struct KotlinLanguageTag {
    static string GetLanguageName() { return "kotlin"; }
};

} // namespace duckdb