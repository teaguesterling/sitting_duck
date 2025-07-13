#include "ast_type.hpp"
#include "language_adapter.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/string_util.hpp"
#include <sstream>

namespace duckdb {

Value ASTNode::ToValue() const {
    
    child_list_t<Value> struct_values;
    
    // FLAT STRUCTURE: All fields as direct struct members (no nesting except native)
    struct_values.emplace_back("node_id", Value::UBIGINT(node_id));
    struct_values.emplace_back("type", Value(type_raw));
    
    // Flat source fields
    struct_values.emplace_back("file_path", file_path.empty() ? Value(LogicalType::VARCHAR) : Value(file_path));
    struct_values.emplace_back("language", language.empty() ? Value(LogicalType::VARCHAR) : Value(language));
    struct_values.emplace_back("start_line", Value::UINTEGER(source_start_line));
    struct_values.emplace_back("start_column", Value::UINTEGER(source_start_column));
    struct_values.emplace_back("end_line", Value::UINTEGER(source_end_line));
    struct_values.emplace_back("end_column", Value::UINTEGER(source_end_column));
    
    // Flat structure fields
    struct_values.emplace_back("parent_id", parent_id < 0 ? Value(LogicalType::BIGINT) : Value::BIGINT(parent_id));
    struct_values.emplace_back("depth", Value::UINTEGER(depth));
    struct_values.emplace_back("sibling_index", Value::UINTEGER(sibling_index));
    struct_values.emplace_back("children_count", Value::UINTEGER(children_count));
    struct_values.emplace_back("descendant_count", Value::UINTEGER(descendant_count));
    
    // Flat context fields
    struct_values.emplace_back("name", name_raw.empty() ? Value(LogicalType::VARCHAR) : Value(name_raw));
    struct_values.emplace_back("semantic_type", Value::UTINYINT(semantic_type));
    struct_values.emplace_back("flags", Value::UTINYINT(universal_flags));
    
    // Native context struct - properly serialize if available
    if (native_extraction_attempted && !native.signature_type.empty()) {
        child_list_t<Value> native_values;
        native_values.emplace_back("signature_type", Value(native.signature_type));
        
        // Create parameters list - simplified for now to avoid memory issues
        vector<Value> parameter_values;
        for (const auto& param : native.parameters) {
            child_list_t<Value> param_struct;
            param_struct.emplace_back("name", Value(param.name));
            param_struct.emplace_back("type", Value(param.type));
            param_struct.emplace_back("default_value", Value(param.default_value));
            param_struct.emplace_back("is_optional", Value::BOOLEAN(param.is_optional));
            param_struct.emplace_back("is_variadic", Value::BOOLEAN(param.is_variadic));
            param_struct.emplace_back("annotations", Value(param.annotations));
            parameter_values.push_back(Value::STRUCT(param_struct));
        }
        native_values.emplace_back("parameters", Value::LIST(LogicalType::STRUCT({
            {"name", LogicalType::VARCHAR},
            {"type", LogicalType::VARCHAR},
            {"default_value", LogicalType::VARCHAR},
            {"is_optional", LogicalType::BOOLEAN},
            {"is_variadic", LogicalType::BOOLEAN},
            {"annotations", LogicalType::VARCHAR}
        }), parameter_values));
        
        // Create modifiers list
        vector<Value> modifier_values;
        for (const auto& modifier : native.modifiers) {
            modifier_values.push_back(Value(modifier));
        }
        native_values.emplace_back("modifiers", Value::LIST(LogicalType::VARCHAR, modifier_values));
        
        native_values.emplace_back("qualified_name", Value(native.qualified_name));
        native_values.emplace_back("annotations", Value(native.annotations));
        
        struct_values.emplace_back("native", Value::STRUCT(native_values));
    } else {
        // No native context available - use NULL struct
        child_list_t<LogicalType> native_schema;
        native_schema.push_back(make_pair("signature_type", LogicalType::VARCHAR));
        native_schema.push_back(make_pair("parameters", LogicalType::LIST(LogicalType::STRUCT({
            {"name", LogicalType::VARCHAR},
            {"type", LogicalType::VARCHAR},
            {"default_value", LogicalType::VARCHAR},
            {"is_optional", LogicalType::BOOLEAN},
            {"is_variadic", LogicalType::BOOLEAN},
            {"annotations", LogicalType::VARCHAR}
        }))));
        native_schema.push_back(make_pair("modifiers", LogicalType::LIST(LogicalType::VARCHAR)));
        native_schema.push_back(make_pair("qualified_name", LogicalType::VARCHAR));
        native_schema.push_back(make_pair("annotations", LogicalType::VARCHAR));
        struct_values.emplace_back("native", Value(LogicalType::STRUCT(native_schema)));
    }
    
    // Content preview (flat field)
    struct_values.emplace_back("peek", Value(peek));
    
    return Value::STRUCT(move(struct_values));
}


ASTNode ASTNode::FromValue(const Value &value) {
    auto &struct_value = StructValue::GetChildren(value);
    ASTNode node;
    
    // Identity
    node.node_id = struct_value[0].GetValue<uint64_t>();
    
    // Type struct
    auto &type_struct = StructValue::GetChildren(struct_value[1]);
    node.type_raw = type_struct[0].GetValue<string>();
    node.type_normalized = type_struct[1].GetValue<string>();
    node.type_kind = type_struct[2].GetValue<string>();
    
    // Name struct
    auto &name_struct = StructValue::GetChildren(struct_value[2]);
    node.name_raw = name_struct[0].IsNull() ? "" : name_struct[0].GetValue<string>();
    node.name_qualified = name_struct[1].IsNull() ? "" : name_struct[1].GetValue<string>();
    
    // File position struct
    auto &file_pos_struct = StructValue::GetChildren(struct_value[3]);
    node.start_line = file_pos_struct[0].GetValue<int64_t>();
    node.end_line = file_pos_struct[1].GetValue<int64_t>();
    node.start_column = file_pos_struct[2].GetValue<uint16_t>();
    node.end_column = file_pos_struct[3].GetValue<uint16_t>();
    
    // Tree position struct
    auto &tree_pos_struct = StructValue::GetChildren(struct_value[4]);
    node.node_index = tree_pos_struct[0].GetValue<int64_t>();
    node.parent_index = tree_pos_struct[1].IsNull() ? -1 : tree_pos_struct[1].GetValue<int64_t>();
    node.legacy_sibling_index = tree_pos_struct[2].GetValue<uint32_t>();
    node.node_depth = tree_pos_struct[3].GetValue<uint8_t>();
    
    // Subtree info struct
    auto &subtree_struct = StructValue::GetChildren(struct_value[5]);
    node.tree_depth = subtree_struct[0].GetValue<uint8_t>();
    node.legacy_children_count = subtree_struct[1].GetValue<uint16_t>();
    node.legacy_descendant_count = subtree_struct[2].GetValue<uint16_t>();
    
    // Content preview
    node.peek = struct_value[6].GetValue<string>();
    
    // Update computed taxonomy fields from node_id
    node.UpdateTaxonomyFields();
    
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
    // Parse using tree-sitter directly
    tree = ts_parser_parse_string(parser, nullptr, source_code.c_str(), source_code.length());
    if (!tree) {
        throw IOException("Failed to parse file: " + file_path);
    }
    
    // Build nodes vector using two-pass approach for count calculation
    TSNode root = ts_tree_root_node(tree);
    int64_t node_counter = 0;
    
    struct StackEntry {
        TSNode node;
        int64_t parent_id;
        int32_t depth;
        int32_t sibling_index;
        bool processed; // Track if node has been processed for descendant counting
        idx_t node_index; // Index in nodes array for this node
    };
    
    vector<StackEntry> stack;
    stack.push_back({root, -1, 0, 0, false, 0});
    
    while (!stack.empty()) {
        auto entry = stack.back();
        
        if (!entry.processed) {
            // First time processing this node - create ASTNode and add children
            stack.back().processed = true;
            stack.back().node_index = nodes.size();
            
            // Create node
            ASTNode ast_node;
            ast_node.node_id = node_counter++;
            ast_node.type_raw = string(ts_node_type(entry.node));
            ast_node.parent_index = entry.parent_id;
            ast_node.node_depth = entry.depth;
            ast_node.legacy_sibling_index = entry.sibling_index;
            
            // Extract position
            TSPoint start = ts_node_start_point(entry.node);
            TSPoint end = ts_node_end_point(entry.node);
            ast_node.start_line = start.row + 1;
            ast_node.start_column = start.column + 1;
            ast_node.end_line = end.row + 1;
            ast_node.end_column = end.column + 1;
            
            // Extract name using language adapter
            auto& registry = LanguageAdapterRegistry::GetInstance();
            const LanguageAdapter* adapter = registry.GetAdapter(language);
            ast_node.name_raw = adapter ? adapter->ExtractNodeName(entry.node, source_code) : "";
            
            // Extract source text
            uint32_t start_byte = ts_node_start_byte(entry.node);
            uint32_t end_byte = ts_node_end_byte(entry.node);
            if (start_byte < source_code.size() && end_byte <= source_code.size()) {
                string source_text = source_code.substr(start_byte, end_byte - start_byte);
                ast_node.peek = source_text.length() > 120 ? source_text.substr(0, 120) : source_text;
            }
            
            // Set children_count directly
            uint32_t child_count = ts_node_child_count(entry.node);
            ast_node.legacy_children_count = child_count;
            ast_node.legacy_descendant_count = 0; // Will be calculated in second pass
            
            nodes.push_back(ast_node);
            
            // Add children to stack in reverse order for correct processing
            int64_t current_id = ast_node.node_id;
            for (int32_t i = child_count - 1; i >= 0; i--) {
                TSNode child = ts_node_child(entry.node, i);
                stack.push_back({child, current_id, entry.depth + 1, i, false, 0});
            }
        } else {
            // Second time - all children have been processed, calculate descendant count
            stack.pop_back();
            
            // Calculate descendant count by summing children + their descendants
            int32_t descendant_count = 0;
            int64_t current_node_id = nodes[entry.node_index].node_id;
            
            for (const auto &node : nodes) {
                if (node.parent_index == current_node_id) {
                    descendant_count += 1 + node.legacy_descendant_count;
                }
            }
            
            nodes[entry.node_index].legacy_descendant_count = descendant_count;
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
        
        if (node.parent_index >= 0) {
            parent_to_children[node.parent_index].push_back(i);
        }
    }
}

vector<ASTNode> ASTType::FindNodes(const string &type) const {
    vector<ASTNode> result;
    for (const auto &node : nodes) {
        if (node.type_raw == type) {
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
        if (node.parent_index >= 0) {
            return GetNodeById(node.parent_index);
        }
    }
    return nullptr;
}

int32_t ASTType::MaxDepth() const {
    int32_t max_depth = 0;
    for (const auto &node : nodes) {
        max_depth = std::max(max_depth, static_cast<int32_t>(node.node_depth));
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
        json << "\"type\":\"" << node.type_raw << "\",";
        if (!node.name_raw.empty()) {
            json << "\"name\":\"" << StringUtil::Replace(node.name_raw, "\"", "\\\"") << "\",";
        }
        json << "\"start_line\":" << node.start_line << ",";
        json << "\"end_line\":" << node.end_line << ",";
        if (node.parent_index >= 0) {
            json << "\"parent_id\":" << node.parent_index << ",";
        }
        json << "\"depth\":" << static_cast<int32_t>(node.node_depth);
        json << "}";
    }
    
    json << "]}";
    return json.str();
}

Value ASTType::Serialize() const {
    child_list_t<Value> struct_values;
    
    // Create source substruct
    child_list_t<Value> source_values;
    source_values.emplace_back("file_path", Value(file_path));
    source_values.emplace_back("language", Value(language));
    struct_values.emplace_back("source", Value::STRUCT(move(source_values)));
    
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
    
    // Extract source substruct
    auto &source_struct = StructValue::GetChildren(struct_value[0]);
    auto ast = make_uniq<ASTType>(
        source_struct[0].GetValue<string>(),  // file_path
        source_struct[1].GetValue<string>()   // language
    );
    
    // Deserialize nodes
    auto &nodes_list = ListValue::GetChildren(struct_value[1]);
    for (const auto &node_value : nodes_list) {
        ast->AddNode(ASTNode::FromValue(node_value));
    }
    
    ast->BuildIndexes();
    return ast;
}

// Taxonomy implementation functions
uint64_t ASTNode::GenerateSemanticID(ASTKind kind, uint8_t universal_flags, 
                                    uint8_t super_type, uint8_t parser_type, 
                                    uint8_t arity, uint16_t primary_hash, 
                                    uint16_t parent_hash) {
    uint64_t semantic_id = 0;
    
    // Byte 0: Universal flags (0-3) + KIND (4-7)
    semantic_id |= (universal_flags & 0x0F);
    semantic_id |= ((static_cast<uint8_t>(kind) & 0x0F) << 4);
    
    // Byte 1: Super type (0-1) + Parser type (2-4) + Arity (5-7)
    semantic_id |= ((static_cast<uint64_t>(super_type) & 0x03) << 8);
    semantic_id |= ((static_cast<uint64_t>(parser_type) & 0x07) << 10);
    semantic_id |= ((static_cast<uint64_t>(arity) & 0x07) << 13);
    
    // Bytes 2-3: Future context (16 bits reserved)
    // Currently unused, set to 0
    
    // Bytes 4-5: Primary unique hash (16 bits)
    semantic_id |= (static_cast<uint64_t>(primary_hash) << 32);
    
    // Bytes 6-7: Parent unique hash (16 bits)
    semantic_id |= (static_cast<uint64_t>(parent_hash) << 48);
    
    return semantic_id;
}


void ASTNode::UpdateTaxonomyFields() {
    // Simplified for now - we're removing the complex KIND taxonomy
    // Just clear the fields since we're not using them yet
    kind = 0;
    universal_flags = 0;
    super_type = 0;
    arity_bin = 0;
}

// Static method implementations moved to header as inline functions

// Type registration will be implemented in extension initialization
void RegisterASTType(DatabaseInstance &db) {
    // This will be implemented when we add the custom type to DuckDB
    // For now, we'll use serialization to Value
}

} // namespace duckdb