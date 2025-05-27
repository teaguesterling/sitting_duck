#include "parse_ast_function.hpp"
#include "language_handler.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/extension_util.hpp"
#include <tree_sitter/api.h>
#include <sstream>

namespace duckdb {

static string EscapeJSONString(const string &str) {
    string result;
    result.reserve(str.size() + 2);
    result += '"';
    
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c < 0x20) {
                    result += "\\u";
                    result += StringUtil::Format("%04x", (unsigned char)c);
                } else {
                    result += c;
                }
                break;
        }
    }
    
    result += '"';
    return result;
}

static string SerializeNodeToJSON(TSNode node, const string &content, const LanguageHandler *handler) {
    std::ostringstream json;
    json << "{";
    
    // Add node type
    const char* node_type = ts_node_type(node);
    json << "\"type\":" << EscapeJSONString(node_type);
    
    // Add normalized type if different
    string normalized = handler->GetNormalizedType(node_type);
    if (normalized != node_type) {
        json << ",\"normalized_type\":" << EscapeJSONString(normalized);
    }
    
    // Add position information
    json << ",\"position\":{";
    json << "\"start_byte\":" << ts_node_start_byte(node);
    json << ",\"end_byte\":" << ts_node_end_byte(node);
    json << ",\"start_row\":" << ts_node_start_point(node).row;
    json << ",\"start_column\":" << ts_node_start_point(node).column;
    json << ",\"end_row\":" << ts_node_end_point(node).row;
    json << ",\"end_column\":" << ts_node_end_point(node).column;
    json << "}";
    
    // Add text content
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (start < content.size() && end <= content.size() && end > start) {
        string text = content.substr(start, end - start);
        json << ",\"text\":" << EscapeJSONString(text);
    }
    
    // Add name if available
    string name = handler->ExtractNodeName(node, content);
    if (!name.empty()) {
        json << ",\"name\":" << EscapeJSONString(name);
    }
    
    // Add is_error flag
    if (ts_node_has_error(node)) {
        json << ",\"is_error\":true";
    }
    
    // Add children
    uint32_t child_count = ts_node_child_count(node);
    if (child_count > 0) {
        json << ",\"children\":[";
        for (uint32_t i = 0; i < child_count; i++) {
            if (i > 0) json << ",";
            TSNode child = ts_node_child(node, i);
            json << SerializeNodeToJSON(child, content, handler);
        }
        json << "]";
    }
    
    json << "}";
    return json.str();
}

void ParseASTFunction::ParseASTScalarFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &code_vector = args.data[0];
    auto &language_vector = args.data[1];
    
    BinaryExecutor::Execute<string_t, string_t, string_t>(
        code_vector, language_vector, result, args.size(),
        [&](string_t code, string_t language_str) {
            string language = language_str.GetString();
            
            // Get the language handler
            auto& registry = LanguageHandlerRegistry::GetInstance();
            const LanguageHandler* handler = registry.GetHandler(language);
            if (!handler) {
                throw InvalidInputException("Unsupported language: " + language);
            }
            
            // Create parser
            TSParser* parser = handler->CreateParser();
            if (!parser) {
                throw InternalException("Failed to create parser for language: " + language);
            }
            
            // Parse the code
            string code_str = code.GetString();
            TSTree* tree = ts_parser_parse_string(parser, nullptr, code_str.c_str(), code_str.length());
            if (!tree) {
                ts_parser_delete(parser);
                throw InternalException("Failed to parse code");
            }
            
            // Get root node and serialize to JSON
            TSNode root = ts_tree_root_node(tree);
            string json_result = SerializeNodeToJSON(root, code_str, handler);
            
            // Cleanup
            ts_tree_delete(tree);
            ts_parser_delete(parser);
            
            return StringVector::AddString(result, json_result);
        });
}

void ParseASTFunction::Register(DatabaseInstance &instance) {
    // Register parse_ast(code, language) -> VARCHAR (JSON string)
    vector<LogicalType> arguments = {LogicalType::VARCHAR, LogicalType::VARCHAR};
    
    ScalarFunction parse_ast_func("parse_ast", arguments, LogicalType::VARCHAR, ParseASTScalarFunction);
    
    ExtensionUtil::RegisterFunction(instance, parse_ast_func);
}

} // namespace duckdb