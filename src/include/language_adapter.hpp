#pragma once

#include "duckdb.hpp"
#include "node_config.hpp"
#include "tree_sitter_wrappers.hpp"
#include <tree_sitter/api.h>
#include <unordered_map>
#include <functional>

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
    // Creates a fresh parser instance for each call to avoid shared state issues
    TSTreePtr ParseContent(const string& content) const {
        auto fresh_parser = CreateFreshParser();
        return fresh_parser->ParseString(content);
    }
    
protected:
    // Owned parser instance - created once per adapter
    mutable unique_ptr<TSParserWrapper> parser_wrapper_;
    
    // Initialize parser with language-specific settings
    virtual void InitializeParser() const = 0;
    
    // Create a fresh parser instance (for thread safety)
    virtual unique_ptr<TSParserWrapper> CreateFreshParser() const = 0;
    
    // Helper methods for content extraction
    string ExtractNodeText(TSNode node, const string &content) const;
    string FindChildByType(TSNode node, const string &content, const string &child_type) const;
    string ExtractByStrategy(TSNode node, const string &content, ExtractionStrategy strategy) const;
    
};

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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// TypeScript language adapter
class TypeScriptAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// SQL language adapter
class SQLAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class GoAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string GetSemanticTypeName(const string &node_type) const;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    uint8_t GetNodeFlags(const string &node_type) const override;
    const NodeConfig* GetNodeConfig(const string &node_type) const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class RubyAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class MarkdownAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class JavaAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// PHP support disabled due to scanner dependency on tree-sitter internals
#if 0
class PHPAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};
#endif

class HTMLAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class CSSAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class CAdapter : public LanguageAdapter {
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
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// Language adapter registry
class LanguageAdapterRegistry {
public:
    static LanguageAdapterRegistry& GetInstance();
    
    // Factory function type
    using AdapterFactory = std::function<unique_ptr<LanguageAdapter>()>;
    
    // Register a language adapter factory
    void RegisterLanguageFactory(const string &language, AdapterFactory factory);
    
    // Register a language adapter (legacy, for backwards compatibility)
    void RegisterAdapter(unique_ptr<LanguageAdapter> adapter);
    
    // Get adapter by language name or alias (creates on demand)
    const LanguageAdapter* GetAdapter(const string &language) const;
    
    // Get TSLanguage directly by language name or alias
    const TSLanguage* GetTSLanguage(const string &language) const;
    
    // Get list of supported languages
    vector<string> GetSupportedLanguages() const;
    
private:
    LanguageAdapterRegistry();
    mutable unordered_map<string, unique_ptr<LanguageAdapter>> adapters;  // mutable for lazy creation
    mutable unordered_map<string, AdapterFactory> language_factories;      // mutable for lazy creation
    unordered_map<string, string> alias_to_language;
    
    void InitializeDefaultAdapters();
    
    // Validate language ABI compatibility
    void ValidateLanguageABI(const LanguageAdapter* adapter) const;
};

} // namespace duckdb