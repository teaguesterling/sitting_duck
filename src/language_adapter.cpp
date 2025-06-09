#include "language_adapter.hpp"
#include "semantic_types.hpp"
#include "grammars.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_python();
    const TSLanguage *tree_sitter_javascript();
    const TSLanguage *tree_sitter_cpp();
}

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

// SetParserLanguageWithValidation is now handled by TSParserWrapper

//==============================================================================
// Python Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> PythonAdapter::node_configs = {
    #include "language_configs/python_types.def"
};

#undef DEF_TYPE

string PythonAdapter::GetLanguageName() const {
    return "python";
}

vector<string> PythonAdapter::GetAliases() const {
    return {"python", "py"};
}

void PythonAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_python();
    parser_wrapper_->SetLanguage(ts_language, "Python");
}

unique_ptr<TSParserWrapper> PythonAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_python();
    fresh_parser->SetLanguage(ts_language, "Python");
    return fresh_parser;
}

string PythonAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string PythonAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Fallback: try to find identifier child for common declaration types
    string node_type = string(node_type_str);
    if (node_type.find("definition") != string::npos || 
        node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string PythonAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool PythonAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In Python, names starting with underscore are typically private
    string name = ExtractNodeName(node, content);
    return !name.empty() && name[0] != '_';
}

uint8_t PythonAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* PythonAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// JavaScript Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> JavaScriptAdapter::node_configs = {
    #include "language_configs/javascript_types.def"
};

#undef DEF_TYPE

string JavaScriptAdapter::GetLanguageName() const {
    return "javascript";
}

vector<string> JavaScriptAdapter::GetAliases() const {
    return {"javascript", "js"};
}

void JavaScriptAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_javascript();
    parser_wrapper_->SetLanguage(ts_language, "JavaScript");
}

unique_ptr<TSParserWrapper> JavaScriptAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_javascript();
    fresh_parser->SetLanguage(ts_language, "JavaScript");
    return fresh_parser;
}

string JavaScriptAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string JavaScriptAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // JavaScript-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string JavaScriptAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool JavaScriptAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In JavaScript, check for export statements or public class members
    // For now, simple heuristic - refine later
    return true;  // Most things are public by default in JS
}

uint8_t JavaScriptAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* JavaScriptAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// C++ Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> CPPAdapter::node_configs = {
    #include "language_configs/cpp_types.def"
};

#undef DEF_TYPE

string CPPAdapter::GetLanguageName() const {
    return "cpp";
}

vector<string> CPPAdapter::GetAliases() const {
    return {"cpp", "c++", "cxx", "cc"};
}

void CPPAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_cpp();
    parser_wrapper_->SetLanguage(ts_language, "C++");
}

unique_ptr<TSParserWrapper> CPPAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_cpp();
    fresh_parser->SetLanguage(ts_language, "C++");
    return fresh_parser;
}

string CPPAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string CPPAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // C++-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("specifier") != string::npos || 
        node_type.find("definition") != string::npos) {
        // Try multiple identifier types
        string result = FindChildByType(node, content, "identifier");
        if (result.empty()) {
            result = FindChildByType(node, content, "type_identifier");
        }
        return result;
    }
    
    return "";
}

string CPPAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool CPPAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In C++, check for public access specifier
    // This is a simplified check - would need more sophisticated analysis
    return true;  // Default to public, refine later
}

uint8_t CPPAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* CPPAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
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

void LanguageAdapterRegistry::InitializeDefaultAdapters() {
    // Register factories instead of creating adapters immediately
    RegisterLanguageFactory("python", []() { return make_uniq<PythonAdapter>(); });
    RegisterLanguageFactory("javascript", []() { return make_uniq<JavaScriptAdapter>(); });
    RegisterLanguageFactory("cpp", []() { return make_uniq<CPPAdapter>(); });
}

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