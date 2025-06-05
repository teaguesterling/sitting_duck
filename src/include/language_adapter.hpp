#pragma once

#include "duckdb.hpp"
#include "node_config.hpp"
#include "tree_sitter_wrappers.hpp"
#include <tree_sitter/api.h>
#include <unordered_map>

namespace duckdb {

// Base class for language-specific adapters
class LanguageAdapter {
public:
    virtual ~LanguageAdapter();
    
    // Language identification
    virtual string GetLanguageName() const = 0;
    virtual vector<string> GetAliases() const = 0;
    
    // Core functionality - type normalization and content extraction
    virtual string GetNormalizedType(const string &node_type) const = 0;
    virtual string ExtractNodeName(TSNode node, const string &content) const = 0;
    virtual string ExtractNodeValue(TSNode node, const string &content) const = 0;
    
    // Basic node properties
    virtual bool IsPublicNode(TSNode node, const string &content) const = 0;
    virtual uint8_t GetNodeFlags(const string &node_type) const = 0;
    
    // Get node configuration
    virtual const NodeConfig* GetNodeConfig(const string &node_type) const = 0;
    
    // Get parser (lazy initialization)
    TSParser* GetParser() const {
        if (!parser_wrapper_) {
            InitializeParser();
        }
        return parser_wrapper_->get();
    }
    
    // Parse content directly and return owned tree
    TSTreePtr ParseContent(const string& content) const {
        printf("DEBUG: ParseContent called\n");
        if (!parser_wrapper_) {
            printf("DEBUG: Parser not initialized, calling InitializeParser\n");
            InitializeParser();
        }
        printf("DEBUG: Calling parser_wrapper_->ParseString\n");
        auto result = parser_wrapper_->ParseString(content);
        printf("DEBUG: ParseString returned\n");
        return result;
    }
    
protected:
    // Owned parser instance - created once per adapter
    mutable unique_ptr<TSParserWrapper> parser_wrapper_;
    
    // Initialize parser with language-specific settings
    virtual void InitializeParser() const = 0;
    
    // Helper methods for content extraction
    string ExtractNodeText(TSNode node, const string &content) const;
    string FindChildByType(TSNode node, const string &content, const string &child_type) const;
    string ExtractByStrategy(TSNode node, const string &content, ExtractionStrategy strategy) const;
    
};

// Macro for defining node type configurations
#define DEF_TYPE(raw_type, normalized, name_extraction, value_extraction, flags) \
    {#raw_type, NodeConfig(NormalizedTypes::normalized, ExtractionStrategy::name_extraction, ExtractionStrategy::value_extraction, flags)},

// Python language adapter
class PythonAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    uint8_t GetNodeFlags(const string &node_type) const override;
    const NodeConfig* GetNodeConfig(const string &node_type) const override;

protected:
    void InitializeParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// JavaScript language adapter
class JavaScriptAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    uint8_t GetNodeFlags(const string &node_type) const override;
    const NodeConfig* GetNodeConfig(const string &node_type) const override;

protected:
    void InitializeParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// C++ language adapter
class CPPAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    uint8_t GetNodeFlags(const string &node_type) const override;
    const NodeConfig* GetNodeConfig(const string &node_type) const override;

protected:
    void InitializeParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// Language adapter registry
class LanguageAdapterRegistry {
public:
    static LanguageAdapterRegistry& GetInstance();
    
    // Register a language adapter
    void RegisterAdapter(unique_ptr<LanguageAdapter> adapter);
    
    // Get adapter by language name or alias
    const LanguageAdapter* GetAdapter(const string &language) const;
    
    // Get list of supported languages
    vector<string> GetSupportedLanguages() const;
    
private:
    LanguageAdapterRegistry();
    unordered_map<string, unique_ptr<LanguageAdapter>> adapters;
    unordered_map<string, string> alias_to_language;
    
    void InitializeDefaultAdapters();
    
    // Validate language ABI compatibility
    void ValidateLanguageABI(const LanguageAdapter* adapter) const;
};

} // namespace duckdb