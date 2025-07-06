#pragma once

#include "native_context_extraction.hpp"
#include <tree_sitter/api.h>

namespace duckdb {

//==============================================================================
// Bash-Specific Native Context Extractors  
//==============================================================================

// Forward declaration for BashAdapter
class BashAdapter;

// Base template for Bash extractors - default returns empty context
template<NativeExtractionStrategy Strategy>
struct BashNativeExtractor {
    static NativeContext Extract(TSNode node, const string& content) {
        return NativeContext(); // Default: no extraction
    }
};

// Specialization for FUNCTION_WITH_PARAMS (Bash functions)
template<>
struct BashNativeExtractor<NativeExtractionStrategy::FUNCTION_WITH_PARAMS> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        try {
            // Bash functions don't have explicit return type annotations
            // Default to empty string (becomes NULL in output)
            context.signature_type = "";
            
            // Extract function parameters (Bash functions use positional parameters)
            context.parameters = ExtractBashParameters(node, content);
            
            // Extract function attributes and visibility
            auto modifiers = ExtractBashModifiers(node, content);
            context.modifiers = modifiers;
            
        } catch (...) {
            context.signature_type = "";
            context.parameters.clear();
            context.modifiers.clear();
        }
        
        return context;
    }
    
private:
    static vector<ParameterInfo> ExtractBashParameters(TSNode node, const string& content) {
        vector<ParameterInfo> params;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return params;
        }
        
        // In Bash, function parameters are accessed as $1, $2, $3, etc.
        // We need to analyze the function body to determine what parameters are used
        string function_name = ExtractBashFunctionName(node, content);
        TSNode body = FindFunctionBody(node);
        
        if (!ts_node_is_null(body)) {
            // Look for positional parameter usage ($1, $2, etc.) and special parameters
            auto used_params = AnalyzeParameterUsage(body, content);
            for (const auto& param_info : used_params) {
                params.push_back(param_info);
            }
        }
        
        return params;
    }
    
    static TSNode FindFunctionBody(TSNode function_node) {
        // Safety check: ensure node is valid
        if (ts_node_is_null(function_node)) {
            return {0};
        }
        
        uint32_t child_count = ts_node_child_count(function_node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 1000) {
            return {0};
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(function_node, i);
            
            // Safety check: ensure child node is valid
            if (ts_node_is_null(child)) {
                continue;
            }
            
            const char* child_type = ts_node_type(child);
            
            // Safety check: ensure child_type is not null
            if (!child_type) {
                continue;
            }
            
            if (strcmp(child_type, "compound_statement") == 0 ||
                strcmp(child_type, "do_group") == 0 ||
                strcmp(child_type, "subshell") == 0) {
                return child;
            }
        }
        
        return {0};
    }
    
    static vector<ParameterInfo> AnalyzeParameterUsage(TSNode body, const string& content) {
        vector<ParameterInfo> params;
        set<string> found_params;
        
        // Safety check: ensure node is valid
        if (ts_node_is_null(body)) {
            return params;
        }
        
        // Recursively analyze the function body for parameter usage
        AnalyzeNodeForParameters(body, content, found_params);
        
        // Convert found parameters to ParameterInfo objects
        for (const auto& param : found_params) {
            ParameterInfo info;
            info.name = param;
            info.type = ""; // Bash is untyped
            info.is_optional = false; // In Bash, all positional params are technically optional
            info.is_variadic = (param == "$@" || param == "$*");
            params.push_back(info);
        }
        
        return params;
    }
    
    static void AnalyzeNodeForParameters(TSNode node, const string& content, set<string>& found_params) {
        // Safety check: ensure node is valid
        if (ts_node_is_null(node)) {
            return;
        }
        
        const char* node_type = ts_node_type(node);
        
        // Safety check: ensure node_type is not null
        if (!node_type) {
            return;
        }
        
        // Check for variable expansions that might be parameters
        if (strcmp(node_type, "simple_expansion") == 0 || 
            strcmp(node_type, "expansion") == 0) {
            
            uint32_t start = ts_node_start_byte(node);
            uint32_t end = ts_node_end_byte(node);
            
            // Safety check: ensure bounds are valid
            if (start < content.length() && end <= content.length() && end > start) {
                string expansion_text = content.substr(start, end - start);
                
                // Look for positional parameters ($1, $2, etc.)
                if (expansion_text.length() >= 2 && expansion_text[0] == '$') {
                    string param_part = expansion_text.substr(1);
                    
                    // Check for numeric positional parameters
                    if (param_part.length() == 1 && isdigit(param_part[0]) && param_part[0] != '0') {
                        found_params.insert(expansion_text);
                    }
                    // Check for multi-digit positional parameters
                    else if (param_part.length() > 1 && param_part[0] == '{' && param_part.back() == '}') {
                        string inner = param_part.substr(1, param_part.length() - 2);
                        if (!inner.empty() && all_of(inner.begin(), inner.end(), ::isdigit) && inner[0] != '0') {
                            found_params.insert("$" + inner);
                        }
                    }
                    // Check for special parameters
                    else if (param_part == "@" || param_part == "*" || param_part == "#") {
                        found_params.insert(expansion_text);
                    }
                }
            }
        }
        
        // Recursively check child nodes
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 1000) {
            return;
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_null(child)) {
                AnalyzeNodeForParameters(child, content, found_params);
            }
        }
    }
    
    static vector<string> ExtractBashModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check function declaration style
        string function_syntax = CheckFunctionSyntax(node, content);
        if (!function_syntax.empty()) {
            modifiers.push_back(function_syntax);
        }
        
        // Check for local variable declarations within function
        TSNode body = FindFunctionBody(node);
        if (!ts_node_is_null(body)) {
            if (HasLocalVariables(body, content)) {
                modifiers.push_back("uses_local");
            }
            if (HasArrayUsage(body, content)) {
                modifiers.push_back("uses_arrays");
            }
            if (HasSubshells(body, content)) {
                modifiers.push_back("uses_subshells");
            }
        }
        
        return modifiers;
    }
    
    static string CheckFunctionSyntax(TSNode node, const string& content) {
        // Bash functions can be declared as:
        // 1. function name() { ... }
        // 2. name() { ... }
        // Check if "function" keyword is present
        
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            uint32_t parent_count = ts_node_child_count(parent);
            for (uint32_t i = 0; i < parent_count; i++) {
                TSNode sibling = ts_node_child(parent, i);
                const char* sibling_type = ts_node_type(sibling);
                
                if (strcmp(sibling_type, "function") == 0) {
                    return "function_keyword";
                }
            }
        }
        
        return "parentheses_syntax";
    }
    
    static bool HasLocalVariables(TSNode body, const string& content) {
        return ContainsNodeType(body, "local") || ContainsNodeType(body, "declare");
    }
    
    static bool HasArrayUsage(TSNode body, const string& content) {
        return ContainsNodeType(body, "array");
    }
    
    static bool HasSubshells(TSNode body, const string& content) {
        return ContainsNodeType(body, "subshell") || ContainsNodeType(body, "command_substitution");
    }
    
    static bool ContainsNodeType(TSNode node, const char* target_type) {
        // Safety check: ensure node is valid
        if (ts_node_is_null(node) || !target_type) {
            return false;
        }
        
        const char* node_type = ts_node_type(node);
        if (node_type && strcmp(node_type, target_type) == 0) {
            return true;
        }
        
        // Recursively check children
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 1000) {
            return false;
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_null(child) && ContainsNodeType(child, target_type)) {
                return true;
            }
        }
        
        return false;
    }
    
    static string ExtractBashFunctionName(TSNode node, const string& content) {
        // In Bash function_definition, the name is typically found as an identifier
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 100) {
            return "";
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
            
            if (strcmp(child_type, "word") == 0) {
                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                
                // Safety check: ensure bounds are valid
                if (start < content.length() && end <= content.length() && end > start) {
                    return content.substr(start, end - start);
                }
            }
        }
        
        return "";
    }
};

// Specialization for VARIABLE_WITH_TYPE (Bash variable declarations)
template<>
struct BashNativeExtractor<NativeExtractionStrategy::VARIABLE_WITH_TYPE> {
    static NativeContext Extract(TSNode node, const string& content) {
        NativeContext context;
        
        // Bash variables don't have explicit types, but we can infer declaration style
        context.signature_type = ExtractBashVariableType(node, content);
        
        // Extract variable modifiers and scope
        auto modifiers = ExtractBashVariableModifiers(node, content);
        context.modifiers = modifiers;
        
        return context;
    }
    
private:
    static string ExtractBashVariableType(TSNode node, const string& content) {
        // Check what type of variable declaration this is
        const char* node_type = ts_node_type(node);
        
        if (strcmp(node_type, "declaration_command") == 0) {
            // Check for declare, local, readonly, export commands
            return AnalyzeDeclarationCommand(node, content);
        } else if (strcmp(node_type, "variable_assignment") == 0) {
            // Simple assignment
            return "assignment";
        }
        
        return "";
    }
    
    static string AnalyzeDeclarationCommand(TSNode node, const string& content) {
        // Look for the command keyword (declare, local, readonly, export)
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 100) {
            return "";
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
            
            if (strcmp(child_type, "local") == 0) {
                return "local";
            } else if (strcmp(child_type, "declare") == 0) {
                return "declare";
            } else if (strcmp(child_type, "readonly") == 0) {
                return "readonly";
            } else if (strcmp(child_type, "export") == 0) {
                return "export";
            }
        }
        
        return "declaration";
    }
    
    static vector<string> ExtractBashVariableModifiers(TSNode node, const string& content) {
        vector<string> modifiers;
        
        // Check for variable declaration flags (declare -a, declare -A, etc.)
        auto flags = ExtractDeclarationFlags(node, content);
        for (const auto& flag : flags) {
            modifiers.push_back(flag);
        }
        
        // Check for array assignment patterns
        if (IsArrayAssignment(node, content)) {
            modifiers.push_back("array_assignment");
        }
        
        // Check for command substitution in assignment
        if (HasCommandSubstitution(node, content)) {
            modifiers.push_back("command_substitution");
        }
        
        return modifiers;
    }
    
    static vector<string> ExtractDeclarationFlags(TSNode node, const string& content) {
        vector<string> flags;
        
        // Look for declare/local flags like -a (array), -A (associative array), -r (readonly), etc.
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 100) {
            return flags;
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            
            // Safety check: ensure child node is valid
            if (ts_node_is_null(child)) {
                continue;
            }
            
            uint32_t start = ts_node_start_byte(child);
            uint32_t end = ts_node_end_byte(child);
            
            // Safety check: ensure bounds are valid
            if (start < content.length() && end <= content.length() && end > start) {
                string text = content.substr(start, end - start);
                
                // Check for common declare flags
                if (text == "-a") flags.push_back("indexed_array");
                else if (text == "-A") flags.push_back("associative_array");
                else if (text == "-r") flags.push_back("readonly");
                else if (text == "-i") flags.push_back("integer");
                else if (text == "-x") flags.push_back("export");
                else if (text == "-u") flags.push_back("uppercase");
                else if (text == "-l") flags.push_back("lowercase");
            }
        }
        
        return flags;
    }
    
    static bool IsArrayAssignment(TSNode node, const string& content) {
        // Look for array assignment patterns like var=(value1 value2) or var[index]=value
        return ContainsNodeType(node, "array") || ContainsPattern(node, content, "=(");
    }
    
    static bool HasCommandSubstitution(TSNode node, const string& content) {
        return ContainsNodeType(node, "command_substitution");
    }
    
    static bool ContainsNodeType(TSNode node, const char* target_type) {
        // Safety check: ensure node is valid
        if (ts_node_is_null(node) || !target_type) {
            return false;
        }
        
        const char* node_type = ts_node_type(node);
        if (node_type && strcmp(node_type, target_type) == 0) {
            return true;
        }
        
        // Recursively check children
        uint32_t child_count = ts_node_child_count(node);
        
        // Safety check: reasonable limit on child count
        if (child_count > 1000) {
            return false;
        }
        
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_null(child) && ContainsNodeType(child, target_type)) {
                return true;
            }
        }
        
        return false;
    }
    
    static bool ContainsPattern(TSNode node, const string& content, const string& pattern) {
        uint32_t start = ts_node_start_byte(node);
        uint32_t end = ts_node_end_byte(node);
        
        // Safety check: ensure bounds are valid
        if (start < content.length() && end <= content.length() && end > start) {
            string node_text = content.substr(start, end - start);
            return node_text.find(pattern) != string::npos;
        }
        
        return false;
    }
};

} // namespace duckdb