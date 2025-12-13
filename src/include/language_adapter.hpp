#pragma once

#include "duckdb.hpp"
#include "node_config.hpp"
#include "tree_sitter_wrappers.hpp"
#include <tree_sitter/api.h>
#include <unordered_map>
#include <functional>
#include "duckdb/common/mutex.hpp"

namespace duckdb {

// Forward declarations
struct ASTResult;
struct ExtractionConfig;
class UnifiedASTBackend;

// Type for the parsing function - takes adapter and parsing parameters, returns ASTResult
using ParsingFunction = std::function<ASTResult(const void*, const string&, const string&, const string&, int32_t, const string&)>;

// Base class for language-specific adapters
class LanguageAdapter {
public:
    virtual ~LanguageAdapter();
    
    // Language identification
    virtual string GetLanguageName() const = 0;
    virtual vector<string> GetAliases() const = 0;
    
    // Get the optimized parsing function for this language (single virtual call)
    virtual ParsingFunction GetParsingFunction() const = 0;
    
    // Core functionality - type normalization and content extraction
    virtual string GetNormalizedType(const string &node_type) const = 0;
    virtual string ExtractNodeName(TSNode node, const string &content) const = 0;
    virtual string ExtractNodeValue(TSNode node, const string &content) const = 0;
    
    // Basic node properties
    virtual bool IsPublicNode(TSNode node, const string &content) const = 0;
    
    // Non-virtual hot loop optimizations - these are identical across all adapters
    uint8_t GetNodeFlags(const string &node_type) const {
        const NodeConfig* config = GetNodeConfig(node_type);
        return config ? config->flags : 0;
    }
    
    // Get node configuration - non-virtual base implementation
    const NodeConfig* GetNodeConfig(const string &node_type) const {
        const auto& configs = GetNodeConfigs();  // Single virtual call
        auto it = configs.find(node_type);
        return (it != configs.end()) ? &it->second : nullptr;
    }
    
    // Get parser (lazy initialization) - made public for registry access
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
    
    // Pure virtual method to get the static node configs map - each adapter implements this
    // Made public for template function access (performance optimization)
    virtual const unordered_map<string, NodeConfig>& GetNodeConfigs() const = 0;
    
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
    
    // Qualified identifier extraction helpers
    string ExtractQualifiedIdentifierName(TSNode node, const string &content) const;
    TSNode FindChildByTypeNode(TSNode node, const string &child_type) const;
    string ExtractNameFromQualifiedNode(TSNode qualified_node, const string &content) const;
    string ExtractNameFromDeclarator(TSNode node, const string &content) const;
    string ExtractFunctionNameFromText(TSNode node, const string &content) const;
    
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
    string ExtractCppCustomName(TSNode node, const string &content, const char* node_type_str) const;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// PHP support disabled due to scanner dependency on tree-sitter internals
#if 1
class PHPAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};
#endif

class RustAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class JSONAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class YAMLAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class HTMLAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class BashAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

class SwiftAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

//==============================================================================
// R Adapter
//==============================================================================
class RAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;

protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// Kotlin language adapter
class KotlinAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
protected:
    void InitializeParser() const override;
    unique_ptr<TSParserWrapper> CreateFreshParser() const override;
    
private:
    static const unordered_map<string, NodeConfig> node_configs;
};

// C# language adapter
class CSharpAdapter : public LanguageAdapter {
public:
    string GetLanguageName() const override;
    vector<string> GetAliases() const override;
    string GetNormalizedType(const string &node_type) const override;
    string ExtractNodeName(TSNode node, const string &content) const override;
    string ExtractNodeValue(TSNode node, const string &content) const override;
    bool IsPublicNode(TSNode node, const string &content) const override;
    ParsingFunction GetParsingFunction() const override;
    const unordered_map<string, NodeConfig>& GetNodeConfigs() const override;
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
    
    // Create a fresh adapter instance (for thread-safe pre-creation)
    unique_ptr<LanguageAdapter> CreateAdapter(const string &language) const;
    
    // Get TSLanguage directly by language name or alias
    const TSLanguage* GetTSLanguage(const string &language) const;
    
    // Get list of supported languages
    vector<string> GetSupportedLanguages() const;
    
    // Fast runtime dispatch to compile-time templates - ZERO virtual calls in hot loop!
    ASTResult ParseContentTemplated(const string& content, const string& language, 
                                  const string& file_path, const ExtractionConfig& config) const;
                                  
    // Legacy parsing for backward compatibility
    ASTResult ParseContentTemplated(const string& content, const string& language, 
                                  const string& file_path, int32_t peek_size, const string& peek_mode) const;
    
private:
    LanguageAdapterRegistry();
    mutable mutex registry_mutex_;  // Synchronize access to mutable shared state
    mutable unordered_map<string, unique_ptr<LanguageAdapter>> adapters;  // mutable for lazy creation
    mutable unordered_map<string, AdapterFactory> language_factories;      // mutable for lazy creation
    unordered_map<string, string> alias_to_language;
    
    void InitializeDefaultAdapters();
    
    // Validate language ABI compatibility
    void ValidateLanguageABI(const LanguageAdapter* adapter) const;
};

} // namespace duckdb