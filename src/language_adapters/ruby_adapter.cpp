#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_ruby();
}

namespace duckdb {

//==============================================================================
// Ruby Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> RubyAdapter::node_configs = {
    #include "../language_configs/ruby_types.def"
};

#undef DEF_TYPE

string RubyAdapter::GetLanguageName() const {
    return "ruby";
}

vector<string> RubyAdapter::GetAliases() const {
    return {"ruby", "rb"};
}

void RubyAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_ruby(), "Ruby");
}

unique_ptr<TSParserWrapper> RubyAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_ruby(), "Ruby");
    return fresh_parser;
}

string RubyAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string RubyAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Ruby-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type == "method" || node_type == "singleton_method" || 
        node_type == "class" || node_type == "module") {
        return FindChildByType(node, content, "identifier");
    } else if (node_type == "assignment") {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string RubyAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
        // For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
        return "";
    }
    
    return "";
}

bool RubyAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In Ruby, methods/variables starting with underscore or all caps constants are often considered private/internal
    // Methods are public by default unless explicitly marked private/protected
    string name = ExtractNodeName(node, content);
    
    if (name.empty()) {
        return true;  // Default to public if no name
    }
    
    // Check for private/protected access modifiers in the surrounding context
    // This is a simplified implementation - full implementation would track access modifier state
    
    // Convention: underscore prefix typically indicates private/internal
    if (name[0] == '_') {
        return false;
    }
    
    // Convention: methods ending with ? or ! are typically public (Ruby idioms)
    if (name.back() == '?' || name.back() == '!') {
        return true;
    }
    
    // Default to public (Ruby's default visibility for methods)
    return true;
}

const unordered_map<string, NodeConfig>& RubyAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction RubyAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const RubyAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}




} // namespace duckdb
