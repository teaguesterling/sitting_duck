#include "ast_type.hpp"
#include "ast_parser.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>

namespace duckdb {

Value ASTNode::ToValue() const {
    child_list_t<Value> struct_values;
    struct_values.emplace_back("node_id", Value::BIGINT(node_id));
    struct_values.emplace_back("type", Value(type));
    struct_values.emplace_back("name", name.empty() ? Value(LogicalType::VARCHAR) : Value(name));
    struct_values.emplace_back("start_line", Value::INTEGER(start_line));
    struct_values.emplace_back("start_column", Value::INTEGER(start_column));
    struct_values.emplace_back("end_line", Value::INTEGER(end_line));
    struct_values.emplace_back("end_column", Value::INTEGER(end_column));
    struct_values.emplace_back("parent_id", parent_id < 0 ? Value(LogicalType::BIGINT) : Value::BIGINT(parent_id));
    struct_values.emplace_back("depth", Value::INTEGER(depth));
    struct_values.emplace_back("sibling_index", Value::INTEGER(sibling_index));
    struct_values.emplace_back("source_text", Value(source_text));
    
    return Value::STRUCT(move(struct_values));
}

ASTNode ASTNode::FromValue(const Value &value) {
    auto &struct_value = StructValue::GetChildren(value);
    ASTNode node;
    node.node_id = struct_value[0].GetValue<int64_t>();
    node.type = struct_value[1].GetValue<string>();
    node.name = struct_value[2].IsNull() ? "" : struct_value[2].GetValue<string>();
    node.start_line = struct_value[3].GetValue<int32_t>();
    node.start_column = struct_value[4].GetValue<int32_t>();
    node.end_line = struct_value[5].GetValue<int32_t>();
    node.end_column = struct_value[6].GetValue<int32_t>();
    node.parent_id = struct_value[7].IsNull() ? -1 : struct_value[7].GetValue<int64_t>();
    node.depth = struct_value[8].GetValue<int32_t>();
    node.sibling_index = struct_value[9].GetValue<int32_t>();
    node.source_text = struct_value[10].GetValue<string>();
    return node;
}

ASTType::ASTType(const string &file_path, const string &language) 
    : file_path(file_path), language(language) {
}

ASTType::~ASTType() {
    ClearTree();
}

ASTType::ASTType(ASTType&& other) noexcept 
    : file_path(std::move(other.file_path)),
      language(std::move(other.language)),
      nodes(std::move(other.nodes)),
      node_id_to_index(std::move(other.node_id_to_index)),
      parent_to_children(std::move(other.parent_to_children)),
      tree(other.tree) {
    other.tree = nullptr;
}

ASTType& ASTType::operator=(ASTType&& other) noexcept {
    if (this != &other) {
        ClearTree();
        file_path = std::move(other.file_path);
        language = std::move(other.language);
        nodes = std::move(other.nodes);
        node_id_to_index = std::move(other.node_id_to_index);
        parent_to_children = std::move(other.parent_to_children);
        tree = other.tree;
        other.tree = nullptr;
    }
    return *this;
}

void ASTType::ClearTree() {
    if (tree) {
        ts_tree_delete(tree);
        tree = nullptr;
    }
}

void ASTType::ParseFile(const string &source_code, TSParser *parser) {
    // Parse using existing parser
    ASTParser ast_parser;
    tree = ast_parser.ParseString(source_code, parser);
    if (!tree) {
        throw IOException("Failed to parse file: " + file_path);
    }
    
    // Build nodes vector
    TSNode root = ts_tree_root_node(tree);
    int64_t node_counter = 0;
    
    struct StackEntry {
        TSNode node;
        int64_t parent_id;
        int32_t depth;
        int32_t sibling_index;
    };
    
    vector<StackEntry> stack;
    stack.push_back({root, -1, 0, 0});
    
    while (!stack.empty()) {
        auto entry = stack.back();
        stack.pop_back();
        
        // Create node
        ASTNode ast_node;
        ast_node.node_id = node_counter++;
        ast_node.type = ts_node_type(entry.node);
        ast_node.parent_id = entry.parent_id;
        ast_node.depth = entry.depth;
        ast_node.sibling_index = entry.sibling_index;
        
        // Extract position
        TSPoint start = ts_node_start_point(entry.node);
        TSPoint end = ts_node_end_point(entry.node);
        ast_node.start_line = start.row + 1;
        ast_node.start_column = start.column + 1;
        ast_node.end_line = end.row + 1;
        ast_node.end_column = end.column + 1;
        
        // Extract name
        ast_node.name = ast_parser.ExtractNodeName(entry.node, source_code);
        
        // Extract source text
        uint32_t start_byte = ts_node_start_byte(entry.node);
        uint32_t end_byte = ts_node_end_byte(entry.node);
        if (start_byte < source_code.size() && end_byte <= source_code.size()) {
            ast_node.source_text = source_code.substr(start_byte, end_byte - start_byte);
        }
        
        nodes.push_back(ast_node);
        
        // Add children in reverse order for correct processing
        uint32_t child_count = ts_node_child_count(entry.node);
        for (int32_t i = child_count - 1; i >= 0; i--) {
            TSNode child = ts_node_child(entry.node, i);
            stack.push_back({child, ast_node.node_id, entry.depth + 1, i});
        }
    }
    
    BuildIndexes();
}

void ASTType::BuildIndexes() {
    node_id_to_index.clear();
    parent_to_children.clear();
    
    for (idx_t i = 0; i < nodes.size(); i++) {
        const auto &node = nodes[i];
        node_id_to_index[node.node_id] = i;
        
        if (node.parent_id >= 0) {
            parent_to_children[node.parent_id].push_back(i);
        }
    }
}

vector<ASTNode> ASTType::FindNodes(const string &type) const {
    vector<ASTNode> result;
    for (const auto &node : nodes) {
        if (node.type == type) {
            result.push_back(node);
        }
    }
    return result;
}

unique_ptr<ASTNode> ASTType::GetNodeById(int64_t node_id) const {
    auto it = node_id_to_index.find(node_id);
    if (it != node_id_to_index.end()) {
        return make_uniq<ASTNode>(nodes[it->second]);
    }
    return nullptr;
}

vector<ASTNode> ASTType::GetChildren(int64_t parent_id) const {
    vector<ASTNode> result;
    auto it = parent_to_children.find(parent_id);
    if (it != parent_to_children.end()) {
        for (idx_t idx : it->second) {
            result.push_back(nodes[idx]);
        }
    }
    return result;
}

unique_ptr<ASTNode> ASTType::GetParent(int64_t node_id) const {
    auto it = node_id_to_index.find(node_id);
    if (it != node_id_to_index.end()) {
        const auto &node = nodes[it->second];
        if (node.parent_id >= 0) {
            return GetNodeById(node.parent_id);
        }
    }
    return nullptr;
}

int32_t ASTType::MaxDepth() const {
    int32_t max_depth = 0;
    for (const auto &node : nodes) {
        max_depth = std::max(max_depth, node.depth);
    }
    return max_depth;
}

string ASTType::ToJSON() const {
    std::ostringstream json;
    json << "{";
    json << "\"file_path\":\"" << StringUtil::Replace(file_path, "\"", "\\\"") << "\",";
    json << "\"language\":\"" << language << "\",";
    json << "\"node_count\":" << NodeCount() << ",";
    json << "\"max_depth\":" << MaxDepth() << ",";
    json << "\"nodes\":[";
    
    bool first = true;
    for (const auto &node : nodes) {
        if (!first) json << ",";
        first = false;
        
        json << "{";
        json << "\"node_id\":" << node.node_id << ",";
        json << "\"type\":\"" << node.type << "\",";
        if (!node.name.empty()) {
            json << "\"name\":\"" << StringUtil::Replace(node.name, "\"", "\\\"") << "\",";
        }
        json << "\"start_line\":" << node.start_line << ",";
        json << "\"end_line\":" << node.end_line << ",";
        if (node.parent_id >= 0) {
            json << "\"parent_id\":" << node.parent_id << ",";
        }
        json << "\"depth\":" << node.depth;
        json << "}";
    }
    
    json << "]}";
    return json.str();
}

Value ASTType::Serialize() const {
    child_list_t<Value> struct_values;
    struct_values.emplace_back("file_path", Value(file_path));
    struct_values.emplace_back("language", Value(language));
    
    // Serialize nodes as a list of structs
    vector<Value> node_values;
    for (const auto &node : nodes) {
        node_values.push_back(node.ToValue());
    }
    struct_values.emplace_back("nodes", Value::LIST(move(node_values)));
    
    return Value::STRUCT(move(struct_values));
}

unique_ptr<ASTType> ASTType::Deserialize(const Value &value) {
    auto &struct_value = StructValue::GetChildren(value);
    
    auto ast = make_uniq<ASTType>(
        struct_value[0].GetValue<string>(),
        struct_value[1].GetValue<string>()
    );
    
    // Deserialize nodes
    auto &nodes_list = ListValue::GetChildren(struct_value[2]);
    for (const auto &node_value : nodes_list) {
        ast->AddNode(ASTNode::FromValue(node_value));
    }
    
    ast->BuildIndexes();
    return ast;
}

// Type registration will be implemented in extension initialization
void RegisterASTType(DatabaseInstance &db) {
    // This will be implemented when we add the custom type to DuckDB
    // For now, we'll use serialization to Value
}

} // namespace duckdb