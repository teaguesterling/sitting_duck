#include "language_adapter.hpp"
#include "semantic_types.hpp"
#include "ast_type.hpp"  // For ASTResult definition
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

namespace duckdb {

//==============================================================================
// Base LanguageAdapter implementation
//==============================================================================

LanguageAdapter::~LanguageAdapter() {
    // Smart pointer handles cleanup automatically
}

string LanguageAdapter::ExtractNodeText(TSNode node, const string &content) const {
    uint32_t start_byte = ts_node_start_byte(node);
    uint32_t end_byte = ts_node_end_byte(node);
    
    if (start_byte >= content.size() || end_byte > content.size() || start_byte >= end_byte) {
        return "";
    }
    
    return content.substr(start_byte, end_byte - start_byte);
}

string LanguageAdapter::FindChildByType(TSNode node, const string &content, const string &child_type) const {
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* type = ts_node_type(child);
        if (child_type == type) {
            return ExtractNodeText(child, content);
        }
    }
    return "";
}

string LanguageAdapter::ExtractQualifiedIdentifierName(TSNode node, const string &content) const {
    // Universal qualified identifier extraction
    // Searches for qualified/scoped identifiers and extracts just the name part
    
    // Common qualified identifier patterns across languages:
    // - qualified_identifier (C++): ClassName::methodName 
    // - scoped_identifier (Java, Rust): package.ClassName.methodName
    // - nested_identifier (TypeScript): module.submodule.functionName
    
    vector<string> patterns = {
        "qualified_identifier",
        "scoped_identifier", 
        "nested_identifier",
        "property_identifier"  // For JS/TS object.method patterns
    };
    
    // First, search direct children
    for (const string& pattern : patterns) {
        TSNode qualified_node = FindChildByTypeNode(node, pattern);
        if (!ts_node_is_null(qualified_node)) {
            // Found a qualified identifier, now extract the name part
            return ExtractNameFromQualifiedNode(qualified_node, content);
        }
    }
    
    // If not found directly, search recursively in common containers
    vector<string> container_patterns = {
        "function_declarator",
        "method_declarator", 
        "declarator",
        "class_body",
        "interface_body"
    };
    
    for (const string& container : container_patterns) {
        TSNode container_node = FindChildByTypeNode(node, container);
        if (!ts_node_is_null(container_node)) {
            // Recursively search in the container
            string result = ExtractQualifiedIdentifierName(container_node, content);
            if (!result.empty()) {
                return result;
            }
        }
    }
    
    // Fallback: try regular identifier
    return FindChildByType(node, content, "identifier");
}

TSNode LanguageAdapter::FindChildByTypeNode(TSNode node, const string &child_type) const {
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* type = ts_node_type(child);
        if (child_type == type) {
            return child;
        }
    }
    return {0}; // Return null node
}

string LanguageAdapter::ExtractNameFromQualifiedNode(TSNode qualified_node, const string &content) const {
    // Extract the final identifier from qualified names like:
    // - ClassName::methodName -> methodName
    // - package.Class.method -> method
    // - module.submodule.func -> func
    
    uint32_t child_count = ts_node_child_count(qualified_node);
    
    // Look for the last identifier in the qualified chain
    string last_identifier = "";
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(qualified_node, i);
        const char* child_type = ts_node_type(child);
        
        if (strcmp(child_type, "identifier") == 0) {
            // Keep track of the last identifier found
            last_identifier = ExtractNodeText(child, content);
        }
    }
    
    // If we found an identifier, return it; otherwise return the full qualified name
    if (!last_identifier.empty()) {
        return last_identifier;
    }
    
    // Fallback: return the entire qualified identifier text
    return ExtractNodeText(qualified_node, content);
}

string LanguageAdapter::ExtractNameFromDeclarator(TSNode node, const string &content) const {
    // Universal declarator extraction
    // Searches for identifiers inside declarator nodes

    // Common declarator patterns across languages:
    // - function_declarator (C/C++): contains function name and parameters
    // - method_declarator (Java): contains method name and parameters
    // - declarator (various): general declaration pattern

    vector<string> declarator_patterns = {
        "function_declarator",
        "method_declarator",
        "declarator",
        "procedure_declarator",  // Pascal-like languages
        "init_declarator"        // C++ initializing declarators
    };

    // Wrapper types that may contain the actual declarator (e.g., for pointer/array return types)
    vector<string> wrapper_patterns = {
        "pointer_declarator",    // char *func() - function_declarator is inside pointer_declarator
        "array_declarator",      // int arr[]() - function_declarator is inside array_declarator
        "reference_declarator"   // C++ references
    };

    // Helper lambda to search for declarator and extract name
    auto tryExtractFromDeclarator = [&](TSNode search_node) -> string {
        for (const string& pattern : declarator_patterns) {
            TSNode declarator_node = FindChildByTypeNode(search_node, pattern);
            if (!ts_node_is_null(declarator_node)) {
                // Found a declarator, extract identifier from it
                // First try qualified identifier (for method names like Class::method)
                string result = ExtractQualifiedIdentifierName(declarator_node, content);
                if (!result.empty()) {
                    return result;
                }

                // Fallback to simple identifier
                result = FindChildByType(declarator_node, content, "identifier");
                if (!result.empty()) {
                    return result;
                }
            }
        }
        return "";
    };

    // First, try to find declarator directly under node
    string result = tryExtractFromDeclarator(node);
    if (!result.empty()) {
        return result;
    }

    // If not found, check inside wrapper types (pointer_declarator, array_declarator)
    // This handles cases like: char *sortedWord(...) where function_declarator is inside pointer_declarator
    // Need to handle nested wrappers like: node **alloc2(...) - pointer_declarator inside pointer_declarator
    for (const string& wrapper : wrapper_patterns) {
        TSNode wrapper_node = FindChildByTypeNode(node, wrapper);
        while (!ts_node_is_null(wrapper_node)) {
            result = tryExtractFromDeclarator(wrapper_node);
            if (!result.empty()) {
                return result;
            }
            // Check for nested wrapper (e.g., pointer_declarator inside pointer_declarator)
            TSNode nested_wrapper = {0};
            for (const string& nested : wrapper_patterns) {
                nested_wrapper = FindChildByTypeNode(wrapper_node, nested);
                if (!ts_node_is_null(nested_wrapper)) {
                    break;
                }
            }
            wrapper_node = nested_wrapper;
        }
    }

    // Fallback: try direct identifier search on the original node
    result = FindChildByType(node, content, "identifier");
    if (!result.empty()) {
        return result;
    }

    // Last resort: text-based extraction for malformed AST structures
    return ExtractFunctionNameFromText(node, content);
}

string LanguageAdapter::ExtractFunctionNameFromText(TSNode node, const string &content) const {
    // Text-based extraction for malformed AST structures
    // Handles cases where tree-sitter parsing fails but we can extract from raw text
    
    string node_text = ExtractNodeText(node, content);
    if (node_text.empty()) {
        return "";
    }
    
    // Look for function name patterns in C/C++ style:
    // ReturnType ClassName::FunctionName(parameters) {
    // ReturnType FunctionName(parameters) const {
    
    // Find the position of the opening parenthesis
    size_t paren_pos = node_text.find('(');
    if (paren_pos == string::npos) {
        return "";  // Not a function signature
    }
    
    // Extract everything before the parenthesis
    string before_paren = node_text.substr(0, paren_pos);
    
    // Trim whitespace from the end
    while (!before_paren.empty() && isspace(before_paren.back())) {
        before_paren.pop_back();
    }
    
    if (before_paren.empty()) {
        return "";
    }
    
    // Find the last identifier before the parenthesis
    // This handles patterns like:
    // - "ReturnType FunctionName" -> "FunctionName"  
    // - "ReturnType ClassName::FunctionName" -> "FunctionName"
    // - "const ReturnType& ClassName::FunctionName" -> "FunctionName"
    
    size_t last_space = before_paren.find_last_of(" \t");
    size_t last_colon = before_paren.find_last_of(':');
    
    // Find the start position of the function name
    size_t start_pos = 0;
    if (last_colon != string::npos && last_colon > last_space) {
        // Case: ClassName::FunctionName
        start_pos = last_colon + 1;
    } else if (last_space != string::npos) {
        // Case: ReturnType FunctionName
        start_pos = last_space + 1;
    }
    
    // Extract the function name
    string function_name = before_paren.substr(start_pos);
    
    // Trim any remaining whitespace
    while (!function_name.empty() && isspace(function_name.front())) {
        function_name.erase(0, 1);
    }
    while (!function_name.empty() && isspace(function_name.back())) {
        function_name.pop_back();
    }
    
    // Validate that this looks like a valid identifier
    if (function_name.empty() || 
        (!isalpha(function_name[0]) && function_name[0] != '_' && function_name[0] != '~')) {
        return "";
    }
    
    return function_name;
}

string LanguageAdapter::ExtractByStrategy(TSNode node, const string &content, ExtractionStrategy strategy) const {
    switch (strategy) {
        case ExtractionStrategy::NONE:
            return "";
        case ExtractionStrategy::NODE_TEXT:
            return ExtractNodeText(node, content);
        case ExtractionStrategy::FIRST_CHILD: {
            uint32_t child_count = ts_node_child_count(node);
            if (child_count > 0) {
                TSNode first_child = ts_node_child(node, 0);
                return ExtractNodeText(first_child, content);
            }
            return "";
        }
        case ExtractionStrategy::FIND_IDENTIFIER: {
            // Try common identifier node types across languages
            string result = FindChildByType(node, content, "identifier");
            if (result.empty()) {
                result = FindChildByType(node, content, "property_identifier");  // JS methods
            }
            if (result.empty()) {
                result = FindChildByType(node, content, "field_identifier");  // Go methods
            }
            if (result.empty()) {
                result = FindChildByType(node, content, "qualified_identifier");  // C++
            }
            if (result.empty()) {
                result = FindChildByType(node, content, "name");  // PHP
            }
            if (result.empty()) {
                result = FindChildByType(node, content, "simple_identifier");  // Swift, Kotlin
            }
            if (result.empty()) {
                result = FindChildByType(node, content, "type_identifier");  // Swift types
            }
            return result;
        }
        case ExtractionStrategy::FIND_PROPERTY:
            return FindChildByType(node, content, "property_identifier");
        case ExtractionStrategy::FIND_QUALIFIED_IDENTIFIER:
            return ExtractQualifiedIdentifierName(node, content);
        case ExtractionStrategy::FIND_IN_DECLARATOR:
            return ExtractNameFromDeclarator(node, content);
        case ExtractionStrategy::FIND_ASSIGNMENT_TARGET: {
            // Universal pattern: find identifier in parent assignment
            // Handles: R (name <- func), JS (const name = func), C++ (auto name = lambda), Python (x = lambda), etc.
            TSNode parent = ts_node_parent(node);
            if (!ts_node_is_null(parent)) {
                string parent_type = ts_node_type(parent);
                // Check for assignment patterns across languages
                if (parent_type == "binary_operator" ||           // R: name <- function
                    parent_type == "variable_declarator" ||       // JS/TS: const name = function
                    parent_type == "init_declarator" ||           // C++: auto name = lambda
                    parent_type == "assignment" ||                // Python: x = lambda
                    parent_type == "named_expression" ||          // Python: (x := lambda)
                    parent_type.find("declarator") != string::npos) { // Other declarator patterns

                    // Look for the first child which should be the identifier
                    TSNode first_child = ts_node_child(parent, 0);
                    if (!ts_node_is_null(first_child)) {
                        string first_child_type = ts_node_type(first_child);
                        if (first_child_type == "identifier") {
                            return ExtractNodeText(first_child, content);
                        }
                    }
                }
            }
            return "";
        }
        case ExtractionStrategy::FIND_CALL_TARGET: {
            // Extract method/function name from call expressions
            // Handles: simple calls (print), method calls (obj.method), qualified calls (pkg.func)
            if (ts_node_child_count(node) == 0) {
                return "";
            }

            TSNode first_child = ts_node_child(node, 0);
            if (ts_node_is_null(first_child)) {
                return "";
            }

            string first_child_type = ts_node_type(first_child);

            // Simple function call: first child is identifier
            if (first_child_type == "identifier") {
                return ExtractNodeText(first_child, content);
            }

            // Method/member call patterns: find the rightmost identifier (method name)
            // Python: attribute (obj.method), JS/TS: member_expression, C++: field_expression
            // Go: selector_expression, Rust: field_expression, Java: field_access
            if (first_child_type == "attribute" ||
                first_child_type == "member_expression" ||
                first_child_type == "field_expression" ||
                first_child_type == "selector_expression" ||
                first_child_type == "field_access" ||
                first_child_type == "scoped_identifier" ||
                first_child_type == "qualified_identifier") {

                // Find the last identifier child (the method/function name)
                uint32_t child_count = ts_node_child_count(first_child);
                for (int i = child_count - 1; i >= 0; i--) {
                    TSNode child = ts_node_child(first_child, i);
                    string child_type = ts_node_type(child);

                    if (child_type == "identifier" ||
                        child_type == "property_identifier" ||
                        child_type == "field_identifier" ||
                        child_type == "simple_identifier") {
                        return ExtractNodeText(child, content);
                    }
                }

                // Fallback: return the full expression text
                return ExtractNodeText(first_child, content);
            }

            // Other patterns (subscript calls, etc.): try to find any identifier
            return FindChildByType(node, content, "identifier");
        }
        case ExtractionStrategy::CUSTOM:
            // Will be overridden by specific language adapters
            return "";
        default:
            return "";
    }
}

//==============================================================================
// LanguageAdapterRegistry implementation
//==============================================================================

LanguageAdapterRegistry::LanguageAdapterRegistry() {
    InitializeDefaultAdapters();
}

LanguageAdapterRegistry& LanguageAdapterRegistry::GetInstance() {
    static LanguageAdapterRegistry instance;
    return instance;
}

void LanguageAdapterRegistry::RegisterAdapter(unique_ptr<LanguageAdapter> adapter) {
    if (!adapter) {
        throw InvalidInputException("Cannot register null adapter");
    }
    
    // Validate ABI compatibility
    ValidateLanguageABI(adapter.get());
    
    string language = adapter->GetLanguageName();
    vector<string> aliases = adapter->GetAliases();
    
    // Register all aliases
    for (const auto &alias : aliases) {
        alias_to_language[alias] = language;
    }
    
    adapters[language] = std::move(adapter);
}

const TSLanguage* LanguageAdapterRegistry::GetTSLanguage(const string &language) const {
    const LanguageAdapter* adapter = GetAdapter(language);
    if (!adapter) {
        return nullptr;
    }
    
    // Get the TSLanguage from the adapter's parser
    TSParser* parser = adapter->GetParser();
    if (!parser) {
        return nullptr;
    }
    
    return ts_parser_language(parser);
}

const LanguageAdapter* LanguageAdapterRegistry::GetAdapter(const string &language) const {
    // MEMORY SAFETY FIX: Eliminate persistent adapter caching to prevent state accumulation
    // This function now creates fresh adapters for each call to prevent the segfault
    // caused by cumulative adapter state across multiple parsing operations
    
    // WARNING: This method should be deprecated in favor of CreateAdapter() 
    // for proper memory management. Using pre_created_adapters is preferred.
    
    lock_guard<mutex> lock(registry_mutex_);
    
    // Resolve alias to actual language name
    string actual_language = language;
    auto alias_it = alias_to_language.find(language);
    if (alias_it != alias_to_language.end()) {
        actual_language = alias_it->second;
    }
    
    // Check if we have a factory for this language
    auto factory_it = language_factories.find(actual_language);
    if (factory_it != language_factories.end()) {
        // Create a fresh adapter instance - NO PERSISTENT CACHING
        auto adapter = factory_it->second();
        
        // Validate ABI compatibility
        ValidateLanguageABI(adapter.get());
        
        // TEMPORARY: Store adapter in mutable map but clear it periodically to prevent accumulation
        // This is not ideal but maintains API compatibility while fixing the segfault
        auto* adapter_ptr = adapter.get();
        
        // Clear old entries periodically to prevent unbounded growth
        if (adapters.size() > 10) {
            adapters.clear();
        }
        
        adapters[actual_language] = std::move(adapter);
        return adapter_ptr;
    }
    
    return nullptr;
}

unique_ptr<LanguageAdapter> LanguageAdapterRegistry::CreateAdapter(const string &language) const {
    lock_guard<mutex> lock(registry_mutex_);
    
    // Resolve alias to actual language name
    string actual_language = language;
    auto alias_it = alias_to_language.find(language);
    if (alias_it != alias_to_language.end()) {
        actual_language = alias_it->second;
    }
    
    // Check if we have a factory for this language
    auto factory_it = language_factories.find(actual_language);
    if (factory_it != language_factories.end()) {
        // Create a fresh adapter instance
        auto adapter = factory_it->second();
        
        // Validate ABI compatibility
        ValidateLanguageABI(adapter.get());
        
        return adapter;
    }
    
    return nullptr;
}

vector<string> LanguageAdapterRegistry::GetSupportedLanguages() const {
    vector<string> languages;
    
    // Include already-created adapters
    for (const auto &pair : adapters) {
        languages.push_back(pair.first);
    }
    
    // Include factory-registered languages
    for (const auto &pair : language_factories) {
        // Only add if not already in the list
        if (adapters.find(pair.first) == adapters.end()) {
            languages.push_back(pair.first);
        }
    }
    
    return languages;
}

void LanguageAdapterRegistry::ValidateLanguageABI(const LanguageAdapter* adapter) const {
    // Test adapter functionality to validate ABI compatibility
    try {
        string language = adapter->GetLanguageName();
        
        // For DuckDB adapter, test that parsing function can be retrieved
        if (language == "duckdb") {
            // Test that the parsing function is available without calling it
            ParsingFunction parsing_fn = adapter->GetParsingFunction();
            if (!parsing_fn) {
                throw InvalidInputException("DuckDB adapter failed to provide parsing function");
            }
            // Parsing function exists - validation successful
            return;
        }
        
        // For tree-sitter based adapters, test parser creation
        auto test_adapter = const_cast<LanguageAdapter*>(adapter);
        TSParser* parser = test_adapter->GetParser();
        if (!parser) {
            throw InvalidInputException("Language adapter for '" + language + 
                                       "' failed to create parser");
        }
    } catch (const Exception& e) {
        throw InvalidInputException("Language adapter for '" + adapter->GetLanguageName() + 
                                   "' failed validation: " + e.what());
    }
}

// InitializeDefaultAdapters() is implemented in language_adapter_registry_init.cpp
// to avoid circular dependencies with the adapter implementations

void LanguageAdapterRegistry::RegisterLanguageFactory(const string &language, AdapterFactory factory) {
    if (!factory) {
        throw InvalidInputException("Cannot register null factory");
    }
    
    // Create a temporary adapter to get aliases
    auto temp_adapter = factory();
    if (!temp_adapter) {
        throw InvalidInputException("Factory returned null adapter");
    }
    
    vector<string> aliases = temp_adapter->GetAliases();
    
    // Register all aliases
    for (const auto &alias : aliases) {
        alias_to_language[alias] = language;
    }
    
    // Store the factory
    language_factories[language] = std::move(factory);
}

} // namespace duckdb