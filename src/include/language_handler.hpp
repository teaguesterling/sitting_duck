#pragma once

#include "duckdb.hpp"
#include <tree_sitter/api.h>
#include <memory>
#include <unordered_map>

namespace duckdb {

// Base class for language-specific handlers
class LanguageHandler {
public:
    virtual ~LanguageHandler() = default;
    
    // Language identification
    virtual string GetLanguageName() const = 0;
    virtual vector<string> GetAliases() const = 0;
    
    // Parser creation
    virtual TSParser* CreateParser() const = 0;
    
    // Type normalization
    virtual string GetNormalizedType(const string &node_type) const = 0;
    
    // Name extraction
    virtual string ExtractNodeName(TSNode node, const string &content) const = 0;
    
protected:
    // Helper method for finding identifier children
    string FindIdentifierChild(TSNode node, const string &content) const;
    
    // Helper method for extracting node text
    string ExtractNodeText(TSNode node, const string &content) const;
};

// Python language handler
class PythonLanguageHandler : public LanguageHandler {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    TSParser* CreateParser() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    
private:
    static const std::unordered_map<string, string> type_mappings;
};

// JavaScript language handler
class JavaScriptLanguageHandler : public LanguageHandler {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    TSParser* CreateParser() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    
private:
    static const std::unordered_map<string, string> type_mappings;
};

// Language handler registry
class LanguageHandlerRegistry {
public:
    static LanguageHandlerRegistry& GetInstance();
    
    // Register a language handler
    void RegisterHandler(std::unique_ptr<LanguageHandler> handler);
    
    // Get handler by language name or alias
    const LanguageHandler* GetHandler(const string &language) const;
    
    // Get list of supported languages
    vector<string> GetSupportedLanguages() const;
    
private:
    LanguageHandlerRegistry();
    std::unordered_map<string, std::unique_ptr<LanguageHandler>> handlers;
    std::unordered_map<string, string> alias_to_language;
    
    void InitializeDefaultHandlers();
};

// Normalized node type constants
namespace NormalizedTypes {
    // Declarations
    constexpr const char* FUNCTION_DECLARATION = "function_declaration";
    constexpr const char* CLASS_DECLARATION = "class_declaration";
    constexpr const char* VARIABLE_DECLARATION = "variable_declaration";
    constexpr const char* METHOD_DECLARATION = "method_declaration";
    
    // Expressions
    constexpr const char* FUNCTION_CALL = "function_call";
    constexpr const char* VARIABLE_REFERENCE = "variable_reference";
    constexpr const char* LITERAL = "literal";
    constexpr const char* BINARY_EXPRESSION = "binary_expression";
    
    // Control flow
    constexpr const char* IF_STATEMENT = "if_statement";
    constexpr const char* LOOP_STATEMENT = "loop_statement";
    constexpr const char* RETURN_STATEMENT = "return_statement";
    
    // Other
    constexpr const char* COMMENT = "comment";
    constexpr const char* IMPORT_STATEMENT = "import_statement";
    constexpr const char* EXPORT_STATEMENT = "export_statement";
}

} // namespace duckdb