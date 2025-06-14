#include "language_adapter.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_python();
    const TSLanguage *tree_sitter_javascript();
    const TSLanguage *tree_sitter_cpp();
    const TSLanguage *tree_sitter_typescript();
    const TSLanguage *tree_sitter_sql();
    const TSLanguage *tree_sitter_go();
    const TSLanguage *tree_sitter_ruby();
    const TSLanguage *tree_sitter_markdown();
    const TSLanguage *tree_sitter_markdown_inline();
    const TSLanguage *tree_sitter_java();
    // const TSLanguage *tree_sitter_php(); // Disabled due to scanner issues
    const TSLanguage *tree_sitter_html();
    const TSLanguage *tree_sitter_css();
    const TSLanguage *tree_sitter_c();
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
    // In JavaScript, check for export statements or naming conventions
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // Check if it's an export declaration
    if (node_type.find("export") != string::npos) {
        return true;
    }
    
    // Check parent nodes for export context
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent)) {
        const char* parent_type = ts_node_type(parent);
        if (string(parent_type).find("export") != string::npos) {
            return true;
        }
    }
    
    // Check naming conventions - underscore prefix typically indicates private
    string name = ExtractNodeName(node, content);
    if (!name.empty() && name[0] == '_') {
        return false;
    }
    
    // Default to public for JavaScript (no explicit access modifiers)
    return true;
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
    // In C++, check for access specifiers and scope
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // Functions and classes at namespace/global scope are generally public
    if (node_type == "function_definition" || 
        node_type == "function_declarator" ||
        node_type == "class_definition") {
        
        // Check if it's in a namespace (likely public API)
        TSNode parent = ts_node_parent(node);
        while (!ts_node_is_null(parent)) {
            const char* parent_type = ts_node_type(parent);
            if (string(parent_type) == "namespace_definition") {
                return true;  // In namespace = public API
            }
            if (string(parent_type) == "class_specifier" || 
                string(parent_type) == "struct_specifier") {
                break;  // Inside class, need to check access specifier
            }
            parent = ts_node_parent(parent);
        }
        
        // If at global scope, consider public
        if (ts_node_is_null(parent) || 
            string(ts_node_type(parent)) == "translation_unit") {
            return true;
        }
    }
    
    // For class members, check for access specifiers in surrounding context
    // This is simplified - real implementation would track access specifier state
    TSNode sibling = ts_node_prev_sibling(node);
    while (!ts_node_is_null(sibling)) {
        const char* sibling_type = ts_node_type(sibling);
        if (string(sibling_type) == "access_specifier") {
            string specifier_text = ExtractNodeText(sibling, content);
            if (specifier_text.find("public") != string::npos) {
                return true;
            }
            if (specifier_text.find("private") != string::npos || 
                specifier_text.find("protected") != string::npos) {
                return false;
            }
        }
        sibling = ts_node_prev_sibling(sibling);
    }
    
    // Check naming conventions - underscore suffix often indicates private/internal
    string name = ExtractNodeName(node, content);
    if (!name.empty() && name.back() == '_') {
        return false;
    }
    
    // Default to public for C++ (conservative approach)
    return true;
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
// TypeScript Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> TypeScriptAdapter::node_configs = {
    // For now, we'll reuse JavaScript types.def since TypeScript is a superset
    // TODO: Create typescript_types.def with TypeScript-specific additions
    #include "language_configs/javascript_types.def"
};

#undef DEF_TYPE

string TypeScriptAdapter::GetLanguageName() const {
    return "typescript";
}

vector<string> TypeScriptAdapter::GetAliases() const {
    return {"typescript", "ts"};
}

void TypeScriptAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_typescript();
    parser_wrapper_->SetLanguage(ts_language, "TypeScript");
}

unique_ptr<TSParserWrapper> TypeScriptAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_typescript();
    fresh_parser->SetLanguage(ts_language, "TypeScript");
    return fresh_parser;
}

string TypeScriptAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string TypeScriptAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // TypeScript-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string TypeScriptAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool TypeScriptAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In TypeScript, check for explicit access modifiers and export statements
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    
    // Check if it's an export declaration
    if (node_type.find("export") != string::npos) {
        return true;
    }
    
    // Check parent nodes for export context
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent)) {
        const char* parent_type = ts_node_type(parent);
        if (string(parent_type).find("export") != string::npos) {
            return true;
        }
    }
    
    // Check for explicit access modifiers in the node text
    uint32_t start_byte = ts_node_start_byte(node);
    uint32_t end_byte = ts_node_end_byte(node);
    if (start_byte < content.size() && end_byte <= content.size()) {
        string node_text = content.substr(start_byte, end_byte - start_byte);
        
        // Look for explicit private/protected keywords
        if (node_text.find("private ") != string::npos || 
            node_text.find("protected ") != string::npos) {
            return false;
        }
        
        // Look for explicit public keyword
        if (node_text.find("public ") != string::npos) {
            return true;
        }
    }
    
    // Check naming conventions - underscore prefix typically indicates private
    string name = ExtractNodeName(node, content);
    if (!name.empty() && name[0] == '_') {
        return false;
    }
    
    // Default to public (TypeScript default visibility)
    return true;
}

uint8_t TypeScriptAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* TypeScriptAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// SQL Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> SQLAdapter::node_configs = {
    // SQL-specific node types - DDL statements
    DEF_TYPE(create_table, DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(create_view, DEFINITION_CLASS, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(create_index, DEFINITION_VARIABLE, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(drop_statement, EXECUTION_STATEMENT, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(alter_table, EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0x01)
    
    // DML statements - queries and transforms
    DEF_TYPE(select_statement, TRANSFORM_QUERY, NONE, NONE, 0x01)
    DEF_TYPE(insert_statement, EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(update_statement, EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0x01)
    DEF_TYPE(delete_statement, EXECUTION_MUTATION, FIND_IDENTIFIER, NONE, 0x01)
    
    // Identifiers and names
    DEF_TYPE(identifier, NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE(column_reference, NAME_IDENTIFIER, NODE_TEXT, NONE, 0)
    DEF_TYPE(table_reference, NAME_QUALIFIED, NODE_TEXT, NONE, 0)
    DEF_TYPE(function_call, COMPUTATION_CALL, FIND_IDENTIFIER, NONE, 0)
    
    // Literals - name and value both contain the literal text
    DEF_TYPE(string_literal, LITERAL_STRING, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE(number_literal, LITERAL_NUMBER, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE(boolean_literal, LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, 0)
    DEF_TYPE(literal, LITERAL_ATOMIC, NODE_TEXT, NODE_TEXT, 0)
    
    // Keywords and comments
    DEF_TYPE(keyword, NAME_KEYWORD, NODE_TEXT, NONE, 0)
    DEF_TYPE(comment, METADATA_COMMENT, NONE, NODE_TEXT, 0x08)
    
    // Query clauses
    DEF_TYPE(where_clause, FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE(having_clause, FLOW_CONDITIONAL, NONE, NONE, 0)
    DEF_TYPE(order_by_clause, ORGANIZATION_LIST, NONE, NONE, 0)
    DEF_TYPE(group_by_clause, TRANSFORM_AGGREGATION, NONE, NONE, 0)
};

#undef DEF_TYPE

string SQLAdapter::GetLanguageName() const {
    return "sql";
}

vector<string> SQLAdapter::GetAliases() const {
    return {"sql"};
}

void SQLAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_sql();
    parser_wrapper_->SetLanguage(ts_language, "SQL");
}

unique_ptr<TSParserWrapper> SQLAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    auto ts_language = tree_sitter_sql();
    fresh_parser->SetLanguage(ts_language, "SQL");
    return fresh_parser;
}

string SQLAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string SQLAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // SQL-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("table") != string::npos || node_type.find("view") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string SQLAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool SQLAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In SQL, most objects are "public" in the sense they're accessible
    // Could refine this to check for schema-qualified names
    return true;
}

uint8_t SQLAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* SQLAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// Go Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> GoAdapter::node_configs = {
    #include "language_configs/go_types.def"
};

#undef DEF_TYPE

string GoAdapter::GetLanguageName() const {
    return "go";
}

vector<string> GoAdapter::GetAliases() const {
    return {"go", "golang"};
}

void GoAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_go(), "Go");
}

unique_ptr<TSParserWrapper> GoAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_go(), "Go");
    return fresh_parser;
}

string GoAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string GoAdapter::GetSemanticTypeName(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string GoAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Go-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    } else if (node_type.find("_spec") != string::npos) {
        return FindChildByType(node, content, "identifier");
    } else if (node_type == "package_clause") {
        return FindChildByType(node, content, "package_identifier");
    }
    
    return "";
}

string GoAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool GoAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In Go, names starting with uppercase are public (exported)
    string name = ExtractNodeName(node, content);
    return !name.empty() && isupper(name[0]);
}

uint8_t GoAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* GoAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// Ruby Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> RubyAdapter::node_configs = {
    #include "language_configs/ruby_types.def"
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
        return ExtractByStrategy(node, content, config->value_strategy);
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

uint8_t RubyAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* RubyAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// Markdown Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> MarkdownAdapter::node_configs = {
    #include "language_configs/markdown_types.def"
};

#undef DEF_TYPE

string MarkdownAdapter::GetLanguageName() const {
    return "markdown";
}

vector<string> MarkdownAdapter::GetAliases() const {
    return {"markdown", "md"};
}

void MarkdownAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_markdown(), "Markdown");
}

unique_ptr<TSParserWrapper> MarkdownAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_markdown(), "Markdown");
    return fresh_parser;
}

string MarkdownAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string MarkdownAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Handle custom extraction strategies
        if (config->name_strategy == ExtractionStrategy::CUSTOM) {
            string node_type = string(node_type_str);
            if (node_type == "link" || node_type == "image") {
                return FindChildByType(node, content, "link_text");
            } else if (node_type == "fenced_code_block") {
                return FindChildByType(node, content, "info_string");
            } else if (node_type == "link_reference_definition") {
                return FindChildByType(node, content, "link_label");
            }
        }
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    return "";
}

string MarkdownAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Handle custom extraction strategies
        if (config->value_strategy == ExtractionStrategy::CUSTOM) {
            string node_type = string(node_type_str);
            if (node_type == "link" || node_type == "image") {
                return FindChildByType(node, content, "link_destination");
            } else if (node_type == "link_reference_definition") {
                return FindChildByType(node, content, "link_destination");
            }
        }
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool MarkdownAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In Markdown, all content is essentially "public"
    return true;
}

uint8_t MarkdownAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* MarkdownAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// Java Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> JavaAdapter::node_configs = {
    #include "language_configs/java_types.def"
};

#undef DEF_TYPE

string JavaAdapter::GetLanguageName() const {
    return "java";
}

vector<string> JavaAdapter::GetAliases() const {
    return {"java"};
}

void JavaAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_java(), "Java");
}

unique_ptr<TSParserWrapper> JavaAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_java(), "Java");
    return fresh_parser;
}

string JavaAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string JavaAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // Java-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string JavaAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool JavaAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In Java, check for explicit access modifiers
    uint32_t start_byte = ts_node_start_byte(node);
    uint32_t end_byte = ts_node_end_byte(node);
    if (start_byte < content.size() && end_byte <= content.size()) {
        string node_text = content.substr(start_byte, end_byte - start_byte);
        
        // Look for explicit private/protected keywords
        if (node_text.find("private ") != string::npos || 
            node_text.find("protected ") != string::npos) {
            return false;
        }
        
        // Look for explicit public keyword
        if (node_text.find("public ") != string::npos) {
            return true;
        }
    }
    
    // Default package visibility in Java
    return false;
}

uint8_t JavaAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* JavaAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

// PHP support disabled due to scanner dependency on tree-sitter internals
// The PHP tree-sitter scanner requires internal headers not available
// when using tree-sitter as an external library.
#if 0
//==============================================================================
// PHP Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> PHPAdapter::node_configs = {
    #include "language_configs/php_types.def"
};

#undef DEF_TYPE

string PHPAdapter::GetLanguageName() const {
    return "php";
}

vector<string> PHPAdapter::GetAliases() const {
    return {"php"};
}

void PHPAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_php(), "PHP");
}

unique_ptr<TSParserWrapper> PHPAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_php(), "PHP");
    return fresh_parser;
}

string PHPAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string PHPAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // PHP-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declaration") != string::npos || node_type.find("definition") != string::npos) {
        return FindChildByType(node, content, "name");
    }
    
    return "";
}

string PHPAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool PHPAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In PHP, check for visibility modifiers
    TSNode sibling = ts_node_prev_sibling(node);
    while (!ts_node_is_null(sibling)) {
        const char* sibling_type = ts_node_type(sibling);
        if (string(sibling_type) == "visibility_modifier") {
            string modifier_text = ExtractNodeText(sibling, content);
            if (modifier_text == "private" || modifier_text == "protected") {
                return false;
            }
            if (modifier_text == "public") {
                return true;
            }
        }
        sibling = ts_node_prev_sibling(sibling);
    }
    
    // Default to public in PHP
    return true;
}

uint8_t PHPAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* PHPAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}
#endif

//==============================================================================
// HTML Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> HTMLAdapter::node_configs = {
    #include "language_configs/html_types.def"
};

#undef DEF_TYPE

string HTMLAdapter::GetLanguageName() const {
    return "html";
}

vector<string> HTMLAdapter::GetAliases() const {
    return {"html", "htm"};
}

void HTMLAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_html(), "HTML");
}

unique_ptr<TSParserWrapper> HTMLAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_html(), "HTML");
    return fresh_parser;
}

string HTMLAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string HTMLAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Handle custom HTML strategies
        if (config->name_strategy == ExtractionStrategy::CUSTOM) {
            string node_type = string(node_type_str);
            if (node_type == "element" || node_type == "start_tag" || node_type == "end_tag") {
                return FindChildByType(node, content, "tag_name");
            } else if (node_type == "attribute") {
                return FindChildByType(node, content, "attribute_name");
            }
        }
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    return "";
}

string HTMLAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Handle custom HTML strategies
        if (config->value_strategy == ExtractionStrategy::CUSTOM) {
            string node_type = string(node_type_str);
            if (node_type == "attribute") {
                return FindChildByType(node, content, "attribute_value");
            }
        }
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool HTMLAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In HTML, all elements are "public"
    return true;
}

uint8_t HTMLAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* HTMLAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// CSS Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> CSSAdapter::node_configs = {
    #include "language_configs/css_types.def"
};

#undef DEF_TYPE

string CSSAdapter::GetLanguageName() const {
    return "css";
}

vector<string> CSSAdapter::GetAliases() const {
    return {"css"};
}

void CSSAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_css(), "CSS");
}

unique_ptr<TSParserWrapper> CSSAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_css(), "CSS");
    return fresh_parser;
}

string CSSAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string CSSAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Handle custom CSS strategies
        if (config->name_strategy == ExtractionStrategy::CUSTOM) {
            string node_type = string(node_type_str);
            if (node_type == "declaration") {
                return FindChildByType(node, content, "property_name");
            } else if (node_type == "call_expression") {
                return FindChildByType(node, content, "function_name");
            } else if (node_type == "at_rule") {
                // Extract the @ keyword (e.g., @media, @import)
                return FindChildByType(node, content, "at_keyword");
            } else if (node_type == "import_statement" || node_type == "charset_statement") {
                // Extract the string value
                return FindChildByType(node, content, "string_value");
            } else if (node_type == "keyframe_block") {
                // Extract percentage or from/to keyword
                string percentage = FindChildByType(node, content, "integer_value");
                if (percentage.empty()) {
                    percentage = FindChildByType(node, content, "from");
                }
                if (percentage.empty()) {
                    percentage = FindChildByType(node, content, "to");
                }
                return percentage;
            }
        }
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    return "";
}

string CSSAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        // Handle custom CSS strategies
        if (config->value_strategy == ExtractionStrategy::CUSTOM) {
            string node_type = string(node_type_str);
            if (node_type == "declaration") {
                // Extract all values after the property name
                // This is simplified - in reality we'd need to collect all value nodes
                return ExtractNodeText(node, content);  // Fallback to full text for now
            }
        }
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool CSSAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In CSS, all rules are "public"
    return true;
}

uint8_t CSSAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* CSSAdapter::GetNodeConfig(const string &node_type) const {
    auto it = node_configs.find(node_type);
    return it != node_configs.end() ? &it->second : nullptr;
}

//==============================================================================
// C Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {#raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> CAdapter::node_configs = {
    #include "language_configs/c_types.def"
};

#undef DEF_TYPE

string CAdapter::GetLanguageName() const {
    return "c";
}

vector<string> CAdapter::GetAliases() const {
    return {"c"};
}

void CAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_c(), "C");
}

unique_ptr<TSParserWrapper> CAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_c(), "C");
    return fresh_parser;
}

string CAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string CAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // C-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declarator") != string::npos || 
        node_type.find("specifier") != string::npos ||
        node_type.find("definition") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string CAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool CAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In C, check for static keyword (which makes things file-local)
    TSNode parent = ts_node_parent(node);
    while (!ts_node_is_null(parent)) {
        uint32_t child_count = ts_node_child_count(parent);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(parent, i);
            const char* child_type = ts_node_type(child);
            if (string(child_type) == "storage_class_specifier") {
                string specifier_text = ExtractNodeText(child, content);
                if (specifier_text == "static") {
                    return false;  // Static = file-local
                }
            }
        }
        parent = ts_node_parent(parent);
    }
    
    // Default to public (external linkage)
    return true;
}

uint8_t CAdapter::GetNodeFlags(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    return config ? config->flags : 0;
}

const NodeConfig* CAdapter::GetNodeConfig(const string &node_type) const {
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
    RegisterLanguageFactory("typescript", []() { return make_uniq<TypeScriptAdapter>(); });
    RegisterLanguageFactory("sql", []() { return make_uniq<SQLAdapter>(); });
    RegisterLanguageFactory("go", []() { return make_uniq<GoAdapter>(); });
    RegisterLanguageFactory("ruby", []() { return make_uniq<RubyAdapter>(); });
    RegisterLanguageFactory("markdown", []() { return make_uniq<MarkdownAdapter>(); });
    RegisterLanguageFactory("java", []() { return make_uniq<JavaAdapter>(); });
    // PHP disabled due to scanner dependency on tree-sitter internals
    // RegisterLanguageFactory("php", []() { return make_uniq<PHPAdapter>(); });
    RegisterLanguageFactory("html", []() { return make_uniq<HTMLAdapter>(); });
    RegisterLanguageFactory("css", []() { return make_uniq<CSSAdapter>(); });
    RegisterLanguageFactory("c", []() { return make_uniq<CAdapter>(); });
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