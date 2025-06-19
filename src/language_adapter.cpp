#include "language_adapter.hpp"
#include "semantic_types.hpp"
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
        case ExtractionStrategy::FIND_IDENTIFIER:
            return FindChildByType(node, content, "identifier");
        case ExtractionStrategy::FIND_PROPERTY:
            return FindChildByType(node, content, "property_identifier");
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
    // First try direct lookup in already-created adapters
    auto it = adapters.find(language);
    if (it != adapters.end()) {
        return it->second.get();
    }
    
    // Try alias lookup
    string actual_language = language;
    auto alias_it = alias_to_language.find(language);
    if (alias_it != alias_to_language.end()) {
        actual_language = alias_it->second;
        
        // Check if already created
        auto adapter_it = adapters.find(actual_language);
        if (adapter_it != adapters.end()) {
            return adapter_it->second.get();
        }
    }
    
    // Check if we have a factory for this language
    auto factory_it = language_factories.find(actual_language);
    if (factory_it != language_factories.end()) {
        // Create the adapter on demand
        auto adapter = factory_it->second();
        
        // Validate ABI compatibility
        ValidateLanguageABI(adapter.get());
        
        // Store the created adapter
        auto* adapter_ptr = adapter.get();
        adapters[actual_language] = std::move(adapter);
        return adapter_ptr;
    }
    
    return nullptr;
}

unique_ptr<LanguageAdapter> LanguageAdapterRegistry::CreateAdapter(const string &language) const {
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
    // Test parser initialization to validate ABI compatibility
    try {
        auto test_adapter = const_cast<LanguageAdapter*>(adapter);
        TSParser* parser = test_adapter->GetParser();
        if (!parser) {
            throw InvalidInputException("Language adapter for '" + adapter->GetLanguageName() + 
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