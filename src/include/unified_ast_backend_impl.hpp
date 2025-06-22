#pragma once

#include "unified_ast_backend.hpp"
#include "duckdb/common/string_util.hpp"
#include "utf8proc_wrapper.hpp"
#include <algorithm>

namespace duckdb {

// Helper function to ensure UTF-8 validity using DuckDB's UTF8 processor
static string SanitizeUTF8(const string& input) {
    if (input.empty()) {
        return input;
    }
    
    // Check if already valid
    if (Utf8Proc::IsValid(input.c_str(), input.size())) {
        return input;
    }
    
    // Create mutable char array for MakeValid (following DuckDB pattern)
    std::vector<char> char_array(input.begin(), input.end());
    char_array.push_back('\0'); // Null-terminate
    Utf8Proc::MakeValid(&char_array[0], char_array.size() - 1, '?');  // Replace invalid bytes with '?'
    return string(char_array.begin(), char_array.end() - 1);  // Exclude null terminator
}

// Template implementation of the parsing function - eliminates virtual calls
template<typename AdapterType>
ASTResult UnifiedASTBackend::ParseToASTResultTemplated(const AdapterType* adapter,
                                                       const string& content, 
                                                       const string& language, 
                                                       const string& file_path,
                                                       int32_t peek_size,
                                                       const string& peek_mode) {
    ASTResult result;
    result.source.file_path = file_path;
    result.source.language = language;
    auto start_time = std::chrono::system_clock::now();
    
    // Parse the content using the adapter's smart pointer wrapper
    TSTreePtr tree = adapter->ParseContent(content);
    if (!tree) {
        throw InternalException("Failed to parse content");
    }
    
    // Process tree into ASTNodes using DFS ordering with O(1) descendant counting
    TSNode root = ts_tree_root_node(tree.get());
    uint32_t max_depth = 0;
    
    struct StackEntry {
        TSNode node;
        int64_t parent_id;
        uint32_t depth;
        uint32_t sibling_index;
        bool processed;        // Track if node has been processed
        idx_t node_index;      // Index in nodes array for this node
    };
    
    // Hoist GetNodeConfigs outside the hot loop - huge performance optimization!
    const auto& node_configs = adapter->GetNodeConfigs();
    
    vector<StackEntry> stack;
    stack.push_back({root, -1, 0, 0, false, 0});
    
    while (!stack.empty()) {
        // Check if the top entry is processed before copying
        if (!stack.back().processed) {
            // Copy the entry to avoid reference invalidation when stack reallocates
            auto entry = stack.back();
            // First visit - create node and add children
            entry.processed = true;
            entry.node_index = result.nodes.size();
            
            // Update the stack entry with the processed flag and node_index
            stack.back().processed = true;
            stack.back().node_index = entry.node_index;
            
            // Track max depth
            max_depth = std::max(max_depth, entry.depth);
            
            // Create ASTNode
            ASTNode ast_node;
            
            // Basic information - use node_index as node_id
            ast_node.node_id = entry.node_index;
            ast_node.type.raw = ts_node_type(entry.node);
            
            // Position information
            TSPoint start = ts_node_start_point(entry.node);
            TSPoint end = ts_node_end_point(entry.node);
            ast_node.file_position.start_line = start.row + 1;
            ast_node.file_position.end_line = end.row + 1;
            ast_node.file_position.start_column = start.column + 1;
            ast_node.file_position.end_column = end.column + 1;
            
            // Tree position
            ast_node.tree_position.node_index = entry.node_index;
            ast_node.tree_position.parent_index = entry.parent_id;
            ast_node.tree_position.sibling_index = entry.sibling_index;
            ast_node.tree_position.node_depth = entry.depth;
            
            // Subtree information
            uint32_t child_count = ts_node_child_count(entry.node);
            ast_node.subtree.children_count = child_count;
            ast_node.subtree.descendant_count = 0; // Will be calculated on second visit
            
            // Extract name (direct call, no virtual dispatch)
            string raw_name = adapter->ExtractNodeName(entry.node, content);
            ast_node.name.raw = SanitizeUTF8(raw_name);
            ast_node.name.qualified = ast_node.name.raw; // TODO: Implement qualified name logic
            
            // Extract source text (peek) with configurable size and mode
            uint32_t start_byte = ts_node_start_byte(entry.node);
            uint32_t end_byte = ts_node_end_byte(entry.node);
            if (start_byte < content.size() && end_byte <= content.size() && end_byte > start_byte) {
                string source_text = content.substr(start_byte, end_byte - start_byte);
                
                // Apply peek configuration and sanitize UTF-8
                if (peek_mode == "none" || peek_size == 0) {
                    ast_node.peek = "";  // Empty string will become NULL in output
                } else if (peek_mode == "full" || peek_size == -1) {
                    ast_node.peek = SanitizeUTF8(source_text);
                } else if (peek_mode == "line") {
                    // Extract just the first line
                    size_t newline_pos = source_text.find('\n');
                    ast_node.peek = SanitizeUTF8((newline_pos != string::npos) ? source_text.substr(0, newline_pos) : source_text);
                } else if (peek_mode == "smart") {
                    // Smart mode: adapt to content size and type
                    if (source_text.length() <= 50) {
                        // Small nodes: full content
                        ast_node.peek = SanitizeUTF8(source_text);
                    } else if (source_text.find('\n') == string::npos) {
                        // Single-line: truncate at display width
                        ast_node.peek = SanitizeUTF8(source_text.length() > 80 ? source_text.substr(0, 77) + "..." : source_text);
                    } else {
                        // Multi-line: first meaningful line with smart truncation
                        size_t newline_pos = source_text.find('\n');
                        string first_line = source_text.substr(0, newline_pos);
                        ast_node.peek = SanitizeUTF8(first_line.length() > 80 ? first_line.substr(0, 77) + "..." : first_line);
                    }
                } else if (peek_mode == "compact") {
                    // Compact mode: always truncate for table display
                    uint32_t compact_size = 60;
                    if (source_text.length() <= compact_size) {
                        ast_node.peek = SanitizeUTF8(source_text);
                    } else {
                        // Smart truncation at word boundary
                        string truncated = source_text.substr(0, compact_size);
                        size_t last_space = truncated.find_last_of(" \t");
                        if (last_space != string::npos && last_space > compact_size / 2) {
                            ast_node.peek = SanitizeUTF8(truncated.substr(0, last_space) + "...");
                        } else {
                            ast_node.peek = SanitizeUTF8(truncated + "...");
                        }
                    }
                } else if (peek_mode == "signature") {
                    // Signature mode: extract declaration/signature only
                    if (source_text.find('\n') == string::npos) {
                        // Single line - use as-is (likely already a signature)
                        ast_node.peek = SanitizeUTF8(source_text);
                    } else {
                        // Multi-line: extract until { or : (function/class signatures)
                        size_t brace_pos = source_text.find('{');
                        size_t colon_pos = source_text.find(':');
                        size_t end_pos = string::npos;
                        
                        if (brace_pos != string::npos && colon_pos != string::npos) {
                            end_pos = std::min(brace_pos, colon_pos);
                        } else if (brace_pos != string::npos) {
                            end_pos = brace_pos;
                        } else if (colon_pos != string::npos) {
                            end_pos = colon_pos;
                        }
                        
                        if (end_pos != string::npos) {
                            string signature = source_text.substr(0, end_pos);
                            // Remove trailing whitespace and newlines
                            while (!signature.empty() && (signature.back() == ' ' || signature.back() == '\n' || signature.back() == '\t')) {
                                signature.pop_back();
                            }
                            ast_node.peek = SanitizeUTF8(signature);
                        } else {
                            // Fallback to first line
                            size_t newline_pos = source_text.find('\n');
                            ast_node.peek = SanitizeUTF8((newline_pos != string::npos) ? source_text.substr(0, newline_pos) : source_text);
                        }
                    }
                } else {
                    // Default/auto mode with configurable size
                    uint32_t effective_size = (peek_size > 0) ? peek_size : 120;
                    ast_node.peek = SanitizeUTF8(source_text.length() > effective_size ? source_text.substr(0, effective_size) : source_text);
                }
            }
            
            // Populate semantic type and other fields - pass configs to avoid virtual call
            PopulateSemanticFieldsTemplated(ast_node, adapter, entry.node, content, node_configs);
            
            result.nodes.push_back(ast_node);
            
            // Add children to stack in reverse order for correct processing
            int64_t current_id = ast_node.tree_position.node_index;
            for (int32_t i = child_count - 1; i >= 0; i--) {
                TSNode child = ts_node_child(entry.node, i);
                stack.push_back({child, current_id, entry.depth + 1, static_cast<uint32_t>(i), false, 0});
            }
        } else {
            // Second visit - get the processed entry for descendant calculation
            auto entry = stack.back();
            
            // O(1) descendant count calculation!
            // All nodes between entry.node_index+1 and nodes.size() are descendants
            // due to DFS ordering
            int32_t descendant_count = result.nodes.size() - entry.node_index - 1;
            result.nodes[entry.node_index].subtree.descendant_count = descendant_count;
            
            stack.pop_back();
        }
    }
    
    // Smart pointer automatically cleans up the tree
    
    // Set metadata
    result.parse_time = start_time;
    result.node_count = result.nodes.size();
    result.max_depth = max_depth;
    
    return result;
}

// Templated version of PopulateSemanticFields - zero virtual calls!
template<typename AdapterType>
void PopulateSemanticFieldsTemplated(ASTNode& node, const AdapterType* adapter, TSNode ts_node, const string& content, 
                                    const unordered_map<string, NodeConfig>& node_configs) {
    // Direct hash lookup - no virtual calls!
    auto config_it = node_configs.find(node.type.raw);
    const NodeConfig* config = (config_it != node_configs.end()) ? &config_it->second : nullptr;
    
    if (config) {
        // Set semantic type from configuration
        node.semantic_type = config->semantic_type;
        
        // Set universal flags from configuration with conditional logic
        node.universal_flags = config->flags;
        
        // Handle IS_KEYWORD_IF_LEAF: only apply IS_KEYWORD flag if node has no children
        if (config->flags & ASTNodeFlags::IS_KEYWORD_IF_LEAF) {
            // Remove the conditional flag
            node.universal_flags &= ~ASTNodeFlags::IS_KEYWORD_IF_LEAF;
            
            // Only add IS_KEYWORD if this is a leaf node (no children)
            if (ts_node_child_count(ts_node) == 0) {
                node.universal_flags |= ASTNodeFlags::IS_KEYWORD;
            }
        }
    } else {
        // Fallback: use PARSER_CONSTRUCT for unknown types
        node.semantic_type = SemanticTypes::PARSER_CONSTRUCT;
        node.universal_flags = 0;
    }
    
    // Set normalized type for display/compatibility
    node.type.normalized = SemanticTypes::GetSemanticTypeName(node.semantic_type);
    
    // Calculate arity binning
    node.arity_bin = ASTNode::BinArityFibonacci(ts_node_child_count(ts_node));
    
    // Update legacy fields from semantic_type
    node.UpdateLegacyFields();
}

} // namespace duckdb