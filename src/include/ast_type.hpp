#pragma once

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/types/value.hpp"
#include "tree_sitter/api.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace duckdb {

struct ASTNode {
    int64_t node_id;
    string type;
    string name;
    int32_t start_line;
    int32_t start_column;
    int32_t end_line;
    int32_t end_column;
    int64_t parent_id;
    int32_t depth;
    int32_t sibling_index;
    string source_text;
    
    Value ToValue() const;
    static ASTNode FromValue(const Value &value);
};

class ASTType {
public:
    ASTType() = default;
    ASTType(const string &file_path, const string &language);
    ~ASTType();
    
    // Disable copy constructor and assignment (tree-sitter tree can't be copied)
    ASTType(const ASTType&) = delete;
    ASTType& operator=(const ASTType&) = delete;
    
    // Enable move semantics
    ASTType(ASTType&& other) noexcept;
    ASTType& operator=(ASTType&& other) noexcept;
    
    // Core properties
    const string& GetFilePath() const { return file_path; }
    const string& GetLanguage() const { return language; }
    idx_t NodeCount() const { return nodes.size(); }
    const vector<ASTNode>& GetNodes() const { return nodes; }
    
    // Tree operations
    void ParseFile(const string &source_code, TSParser *parser);
    
    // Node access methods
    vector<ASTNode> FindNodes(const string &type) const;
    unique_ptr<ASTNode> GetNodeById(int64_t node_id) const;
    vector<ASTNode> GetChildren(int64_t parent_id) const;
    unique_ptr<ASTNode> GetParent(int64_t node_id) const;
    int32_t MaxDepth() const;
    
    // Serialization
    string ToJSON() const;
    Value Serialize() const;
    static unique_ptr<ASTType> Deserialize(const Value &value);
    
    // For building AST during deserialization
    void AddNode(const ASTNode &node) { nodes.push_back(node); }
    void BuildIndexes();
    
private:
    string file_path;
    string language;
    vector<ASTNode> nodes;
    unordered_map<int64_t, idx_t> node_id_to_index;
    unordered_map<int64_t, vector<idx_t>> parent_to_children;
    TSTree *tree = nullptr;
    
    void ClearTree();
};

// DuckDB type registration
void RegisterASTType(DatabaseInstance &db);

} // namespace duckdb