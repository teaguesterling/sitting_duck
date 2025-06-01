#pragma once

#include "duckdb.hpp"
#include "node_type_config.hpp"
#include <tree_sitter/api.h>
#include <memory>
#include <unordered_map>

namespace duckdb {

// Forward declaration
class LanguageConfig;

// Base class for language-specific handlers
class LanguageHandler {
public:
    virtual ~LanguageHandler();
    
    // Language identification
    virtual string GetLanguageName() const = 0;
    virtual vector<string> GetAliases() const = 0;
    
    // Type normalization
    virtual string GetNormalizedType(const string &node_type) const = 0;
    
    // Name extraction
    virtual string ExtractNodeName(TSNode node, const string &content) const = 0;
    
    // Value extraction (different from name - could be literal value, text content, etc.)
    virtual string ExtractNodeValue(TSNode node, const string &content) const = 0;
    
    // Determine if a node represents a public/exported construct
    virtual bool IsPublicNode(TSNode node, const string &content) const = 0;
    
    // Get taxonomy configuration for a node type
    virtual const NodeTypeConfig* GetNodeTypeConfig(const string &node_type) const = 0;
    
    // Get language-specific taxonomy configuration
    virtual const LanguageConfig& GetConfig() const = 0;
    
    // High-level parsing interface - uses owned parser
    virtual void ParseFile(const string &content, vector<ASTNode> &nodes) const = 0;
    
    // Compute semantic ID for a node
    uint64_t ComputeSemanticId(TSNode node, const string &content, 
                               uint64_t parent_hash = 0) const;
    
    // Compute arity bin from child count
    static uint8_t ComputeArityBin(uint32_t count) {
        // Fibonacci binning
        if (count == 0) return 0;
        if (count == 1) return 1;
        if (count == 2) return 2;
        if (count == 3) return 3;
        if (count <= 5) return 4;
        if (count <= 8) return 5;
        if (count <= 13) return 6;
        return 7; // 14+
    }
    
    // Get parser (lazy initialization)
    TSParser* GetParser() const {
        if (!parser) {
            InitializeParser();
        }
        return parser;
    }
    
protected:
    // Owned parser instance - created once per handler
    mutable TSParser* parser = nullptr;
    
    // Initialize parser with language-specific settings
    virtual void InitializeParser() const = 0;
    
    // Helper method for finding identifier children
    string FindIdentifierChild(TSNode node, const string &content) const;
    
    // Helper method for extracting node text
    string ExtractNodeText(TSNode node, const string &content) const;
    
    // Helper method for setting language with ABI validation
    void SetParserLanguageWithValidation(TSParser* parser, const TSLanguage* language, const string &language_name) const;
    
    // Lazy-loaded configuration
    mutable unique_ptr<LanguageConfig> config;
};

// Python language handler
class PythonLanguageHandler : public LanguageHandler {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    const NodeTypeConfig* GetNodeTypeConfig(const string &node_type) const override;
    const LanguageConfig& GetConfig() const override;
    void ParseFile(const string &content, vector<ASTNode> &nodes) const override;

protected:
    void InitializeParser() const override;
    
private:
    static const std::unordered_map<string, string> type_mappings;
};

// JavaScript language handler
class JavaScriptLanguageHandler : public LanguageHandler {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    const NodeTypeConfig* GetNodeTypeConfig(const string &node_type) const override;
    const LanguageConfig& GetConfig() const override;
    void ParseFile(const string &content, vector<ASTNode> &nodes) const override;

protected:
    void InitializeParser() const override;
    
private:
    static const std::unordered_map<string, string> type_mappings;
};

// C++ language handler
class CPPLanguageHandler : public LanguageHandler {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    const NodeTypeConfig* GetNodeTypeConfig(const string &node_type) const override;
    const LanguageConfig& GetConfig() const override;
    void ParseFile(const string &content, vector<ASTNode> &nodes) const override;

protected:
    void InitializeParser() const override;
    
private:
    static const std::unordered_map<string, string> type_mappings;
};

// Rust language handler
class RustLanguageHandler : public LanguageHandler {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    const NodeTypeConfig* GetNodeTypeConfig(const string &node_type) const override;
    const LanguageConfig& GetConfig() const override;
    void ParseFile(const string &content, vector<ASTNode> &nodes) const override;

protected:
    void InitializeParser() const override;
    
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
    
    // Validate language ABI compatibility
    void ValidateLanguageABI(const LanguageHandler* handler) const;
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